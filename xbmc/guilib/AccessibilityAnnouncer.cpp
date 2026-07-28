/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AccessibilityAnnouncer.h"

#include "GUIBaseContainer.h"
#include "GUIButtonControl.h"
#include "GUIComponent.h"
#include "GUIControlGroupList.h"
#include "GUIEditControl.h"
#include "GUISpinControlEx.h"
#include "GUIListItem.h"
#include "GUIRadioButtonControl.h"
#include "GUISettingsSliderControl.h"
#include "GUITextLayout.h"
#include "GUISliderControl.h"
#include "GUIWindow.h"
#include "GUIWindowManager.h"
#include "WindowIDs.h"
#include "ServiceBroker.h"
#include "interfaces/AnnouncementManager.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"

#include <mutex>

namespace KODI
{
namespace ACCESSIBILITY
{

std::atomic<bool> CAccessibilityAnnouncer::m_subtitlesEnabled{true};
std::atomic<uint64_t> CAccessibilityAnnouncer::m_sequence{0};
CAccessibilityAnnouncer::SnapshotState CAccessibilityAnnouncer::m_state;
CCriticalSection CAccessibilityAnnouncer::m_stateSection;

namespace
{

/*!
 * \brief Map Kodi's control type to a stable role name.
 *
 * Taken from CGUIControl::GetControlType(), which is an exact enum rather than something
 * recovered from a display string. Unmapped types deliberately return an empty role: a
 * client that does not recognise a role simply says the name without one, which is always
 * better than inventing a word for it.
 */
std::string RoleForControl(CGUIControl::GUICONTROLTYPES type)
{
  switch (type)
  {
    case CGUIControl::GUICONTAINER_EPGGRID:
    case CGUIControl::GUICONTAINER_FIXEDLIST:
    case CGUIControl::GUICONTAINER_LIST:
    case CGUIControl::GUICONTAINER_PANEL:
    case CGUIControl::GUICONTAINER_WRAPLIST:
      return "list";

    case CGUIControl::GUICONTROL_BUTTON:
    case CGUIControl::GUICONTROL_COLORBUTTON:
      return "button";

    // A toggle button is a two-state button, so it is spoken the same way a radio button is.
    case CGUIControl::GUICONTROL_RADIO:
    case CGUIControl::GUICONTROL_TOGGLEBUTTON:
      return "radiobutton";

    case CGUIControl::GUICONTROL_EDIT:
      return "edit";

    case CGUIControl::GUICONTROL_TEXTBOX:
      return "textbox";

    case CGUIControl::GUICONTROL_LABEL:
    case CGUIControl::GUICONTROL_LISTLABEL:
      return "label";

    case CGUIControl::GUICONTROL_FADELABEL:
      return "fadelabel";

    case CGUIControl::GUICONTROL_GROUP:
    case CGUIControl::GUICONTROL_GROUPLIST:
    case CGUIControl::GUICONTROL_LISTGROUP:
      return "group";

    case CGUIControl::GUICONTROL_BORDEREDIMAGE:
    case CGUIControl::GUICONTROL_IMAGE:
    case CGUIControl::GUICONTROL_MULTI_IMAGE:
      return "image";

    case CGUIControl::GUICONTROL_PROGRESS:
      return "progress";

    case CGUIControl::GUICONTROL_RANGES:
    case CGUIControl::GUICONTROL_SETTINGS_SLIDER:
    case CGUIControl::GUICONTROL_SLIDER:
      return "slider";

    case CGUIControl::GUICONTROL_SPIN:
    case CGUIControl::GUICONTROL_SPINEX:
      return "spin";

    case CGUIControl::GUICONTROL_SCROLLBAR:
      return "scrollbar";

    default:
      return {};
  }
}

/*!
 * \brief Strip Kodi's own markup, so it is not read out as words.
 *
 * Labels legitimately carry things like "[COLOR orchid]THE CREW REPO[/COLOR]", and a skin
 * can add [B], [I], [UPPERCASE] and friends. Spoken raw that becomes "COLOR orchid THE CREW
 * REPO COLOR". CGUITextLayout::Filter is Kodi's own parser for exactly this, so the rules
 * stay in one place and keep matching what is actually drawn on screen.
 */
std::string PlainText(std::string text)
{
  // Filter() converts to UTF-16, walks every character building a parsed vector,
  // and converts back. Almost no label contains markup, so check for the only
  // character that can start a tag first. This runs on every announcement, and
  // container selections announce from the render thread, so the difference is
  // a frame's worth of work per keypress rather than a micro-optimisation.
  if (text.find('[') == std::string::npos)
    return text;

  CGUITextLayout::Filter(text);
  return text;
}

/*!
 * \brief One node as a CVariant object.
 *
 * Shared by the event path and the snapshot deliberately. If the snapshot built its focus
 * node its own way, the two could describe the same control differently, and a client would
 * hear one thing at startup and another the moment anything moved.
 */
CVariant NodeToVariant(const CAccessibilityNode& node)
{
  CVariant data(CVariant::VariantTypeObject);
  data["window"] = node.window;
  data["dialog"] = node.dialog;
  data["modal"] = node.modal;
  data["control"] = node.control;
  data["role"] = node.role;
  // Everything a client might speak is filtered here, in the one place every
  // node passes through, rather than at each site that fills one in.
  data["name"] = PlainText(node.name);
  data["value"] = PlainText(node.value);
  data["state"] = node.state;
  data["secondary"] = PlainText(node.secondary);
  data["index"] = node.index;
  data["total"] = node.total;
  data["isFolder"] = node.isFolder;
  return data;
}

} // unnamed namespace

bool CAccessibilityAnnouncer::BuildNode(const CGUIControl* control, CAccessibilityNode& node)
{
  if (!control)
    return false;

  node.control = control->GetID();
  node.window = control->GetParentID();
  node.role = RoleForControl(control->GetControlType());

  // Position within a group list, which is how Kodi lays out every settings page: the
  // individual settings are ordinary buttons, spinners and radio buttons parented to a
  // CGUIControlGroupList rather than items in a container, so nothing about them reports a
  // position and a client can only say "Skip steps" with no idea it is the fourth of
  // fourteen. The group list already tracks both numbers for its own scrolling, so this
  // costs a parent lookup and no traversal beyond what it does anyway.
  //
  // Set before the type ladder below, so a container that happens to sit inside a group
  // list still overrides this with its own selection -- which is the more specific answer.
  if (const auto* group = dynamic_cast<const CGUIControlGroupList*>(control->GetParentControl()))
  {
    const int position = group->GetSelectedItem(); // 1-based, or -1
    if (position > 0)
    {
      node.index = position;
      node.total = group->GetNumItems();
    }
  }

  // `dialog` is deliberately left alone here. Asking the window manager meant
  // taking the graphics context lock on every single focus and selection —
  // and container selections announce from the render thread, so that was a
  // lock acquisition per keypress competing with drawing the frame. The window
  // event already carries the flag, from a place where it costs nothing, and a
  // client knows which window it is in.

  // Containers first, and before the button cases below, because this is both the worst
  // thing Kodi exposes and the most common thing anyone does. GetDescription() on a
  // container hands back the selected item's label and nothing else, with square brackets
  // as the entire folder signal, so none of what follows can be recovered from it.
  if (const auto* container = dynamic_cast<const CGUIBaseContainer*>(control))
  {
    const int selected = container->GetSelectedItem();
    const size_t count = container->GetItemCount();

    node.total = static_cast<int>(count);
    if (selected >= 0 && selected < static_cast<int>(count))
    {
      node.index = selected + 1; // 1-based, because it is spoken aloud
      if (const std::shared_ptr<CGUIListItem> item = container->GetListItem(0))
      {
        node.name = item->GetLabel();
        node.secondary = item->GetLabel2();
        node.isFolder = item->m_bIsFolder;

        // Which entry is the one currently in force. A select dialog -- what a settings
        // "dropdown" actually opens -- marks its current value this way, and without it a
        // client reads out all fourteen choices with no way to tell which one is already
        // chosen, or to confirm that picking one worked.
        if (item->IsSelected())
          node.state = "selected";
      }
    }
    return true;
  }

  // Radio before button: CGUIRadioButtonControl derives from CGUIButtonControl, and its
  // GetDescription() appends " (*)" or " ( )" to the label. Reading IsSelected() directly
  // gives a real boolean and leaves the name clean, which is the whole point of the node.
  if (const auto* radio = dynamic_cast<const CGUIRadioButtonControl*>(control))
  {
    node.name = radio->CGUIButtonControl::GetDescription();
    node.secondary = radio->GetLabel2();
    node.state = radio->IsSelected() ? "checked" : "unchecked";
    return true;
  }

  // Settings slider before plain slider, for the same derived-class reason. This one is
  // the only slider that knows its own name; GetDescription() concatenates the two, so we
  // ask for them separately and let the client decide how to join them.
  if (const auto* settingsSlider = dynamic_cast<const CGUISettingsSliderControl*>(control))
  {
    node.name = settingsSlider->GetTextLabel();
    node.value = settingsSlider->CGUISliderControl::GetDescription();
    return true;
  }

  // A plain slider has no label of its own anywhere -- it is a bare widget, and its
  // GetDescription() returns only the value. Saying so honestly beats inventing a name.
  if (const auto* slider = dynamic_cast<const CGUISliderControl*>(control))
  {
    node.value = slider->GetDescription();
    return true;
  }

  // A settings spinner, which is what every "choose one of these" setting is until you open
  // it. Its GetDescription() is literally "{label} ({value})"
  // (GUISpinControlEx.cpp:111-114), so the two are pulled apart here for the same reason the
  // settings slider is: a client can then say "Skip delay, 750 milliseconds" with a real
  // pause, instead of reading a bracket aloud.
  if (const auto* spin = dynamic_cast<const CGUISpinControlEx*>(control))
  {
    node.name = spin->GetTextLabel();
    node.value = spin->GetCurrentLabel();
    return true;
  }

  // Edit before button, and this ordering is load-bearing rather than cosmetic:
  // CGUIEditControl derives from CGUIButtonControl, so without this it would fall into the
  // branch below and have GetLabel2() read as its secondary text. That override returns the
  // real typed characters -- for a password field, the password in clear. Masking exists
  // only in the control's protected GetDisplayedText(), so nothing here may read the value
  // of a protected field. Speaking it aloud would be bad enough; it would also be
  // serialised into an announcement that every listening addon receives.
  if (const auto* edit = dynamic_cast<const CGUIEditControl*>(control))
  {
    node.name = edit->GetDescription(); // not overridden by edit, so this is the clean label
    if (edit->IsPasswordInput())
      node.state = "protected";
    else if (edit->IsReadOnly())
      node.state = "readonly";

    if (!edit->IsPasswordInput())
      node.value = edit->GetLabel2();
    return true;
  }

  if (const auto* button = dynamic_cast<const CGUIButtonControl*>(control))
  {
    node.name = button->GetDescription();
    node.secondary = button->GetLabel2();
    if (button->IsSelected())
      node.state = "checked";
    return true;
  }

  // Everything else: whatever GetDescription() will admit to, which for roughly half of
  // the control classes is an empty string. The role still carries useful information.
  node.name = control->GetDescription();
  return true;
}

void CAccessibilityAnnouncer::RecordState(StateKind kind,
                                          const CAccessibilityNode& node,
                                          uint64_t seq)
{
  if (kind == StateKind::None)
    return;

  std::unique_lock<CCriticalSection> lock(m_stateSection);

  // The sequence moves with the content, under this one lock, so the number a snapshot
  // carries is always exactly the newest announcement folded into it. That is what lets a
  // client discard an older event without having to reason about interleaving at all.
  m_state.seq = seq;

  if (kind == StateKind::Window)
  {
    // Only an opening replaces where we are. A close tells us a window went away but not
    // what is underneath it, and Kodi does not re-focus the control below a dialog that
    // closes -- it was never unfocused, so no focus event follows. Overwriting the record
    // with nothing would leave a client that started at that moment knowing less than
    // before, so the last place we actually knew about is kept.
    if (node.state != "opened")
      return;

    m_state.window = node.window;
    m_state.dialog = node.dialog;
    m_state.modal = node.modal;
    m_state.heading = node.name;
    m_state.text = node.secondary;

    // A window that has only just opened has not focused anything yet: OnInitWindow runs
    // after this announcement. Saying so honestly is what lets a client tell "nothing is
    // focused here" apart from "we have not been told yet", and both of those are real.
    m_state.hasFocus = false;
    m_state.focus = CAccessibilityNode();
    return;
  }

  m_state.focus = node;
  m_state.hasFocus = true;

  // Focus and selection nodes carry the control's parent window, which is the authority on
  // where the user is -- and it is the full window id, not one masked to its base, so a
  // second instance of a dialog is not reported as the first.
  if (node.window > 0)
    m_state.window = node.window;
}

void CAccessibilityAnnouncer::Send(const char* message,
                                   const CAccessibilityNode& node,
                                   StateKind kind)
{
  // Numbered and remembered before the announcer is consulted, so that the state a client
  // can ask for is recorded even during the early startup when there is no announcer yet --
  // which is exactly the window this whole mechanism exists to cover.
  const uint64_t seq = ++m_sequence;
  RecordState(kind, node, seq);

  const std::shared_ptr<ANNOUNCEMENT::CAnnouncementManager> announcer =
      CServiceBroker::GetAnnouncementManager();

  // Null during early startup and late shutdown. Both are entirely normal, and neither is
  // worth crashing over.
  if (!announcer)
    return;

  CVariant data = NodeToVariant(node);

  // Pre-incremented, so the first announcement is 1 and 0 means "nothing announced yet",
  // which is what a snapshot taken before anything happened reports.
  data["seq"] = seq;

  // The three-argument overload fills in the standard sender, so this looks like every
  // other Kodi announcement and reaches Python as "GUI.<message>".
  announcer->Announce(ANNOUNCEMENT::GUI, message, data);
}

void CAccessibilityAnnouncer::OnControlFocus(const CGUIControl* control)
{
  CAccessibilityNode node;
  if (BuildNode(control, node))
    Send("OnAccessibilityFocus", node, StateKind::Focus);
}

void CAccessibilityAnnouncer::OnContainerSelection(const CGUIControl* container)
{
  CAccessibilityNode node;
  if (BuildNode(container, node))
    Send("OnAccessibilitySelection", node, StateKind::Focus);
}

void CAccessibilityAnnouncer::OnControlValue(const CGUIControl* control)
{
  CAccessibilityNode node;
  if (BuildNode(control, node))
    Send("OnAccessibilityValue", node, StateKind::Focus);
}

void CAccessibilityAnnouncer::OnTextEdit(const CGUIControl* control,
                                         const std::string& text,
                                         bool deleted,
                                         bool emptied)
{
  CAccessibilityNode node;
  if (!BuildNode(control, node))
    return;

  // BuildNode fills in the field's whole contents, which is right when focus arrives and
  // wrong here: what wants saying is the character just typed, not everything typed so far.
  node.value = text;

  // "protected" is set by BuildNode for a password field and must survive, because it is
  // what tells a client there is deliberately nothing to say. Otherwise the removal is
  // marked so the client can put it in its own language rather than ours -- and "cleared"
  // is kept distinct from "deleted" because a field that is now empty is worth saying so,
  // and only this side knows what was left behind.
  if (node.state.empty())
  {
    if (emptied)
      node.state = "cleared";
    else if (deleted)
      node.state = "deleted";
  }

  Send("OnAccessibilityValue", node, StateKind::Focus);
}

void CAccessibilityAnnouncer::OnWindow(int windowID,
                                       bool dialog,
                                       bool modal,
                                       bool opening,
                                       const std::string& heading,
                                       const std::string& text)
{
  // The toast dialog reports itself separately, with its actual text. Letting it also open
  // and close as a window would have a client announce a nameless dialog either side of
  // every notification.
  if (windowID == WINDOW_DIALOG_KAI_TOAST)
    return;

  CAccessibilityNode node;
  node.window = windowID;
  node.dialog = dialog;
  node.modal = modal;
  node.role = "window";
  node.name = heading;
  node.state = opening ? "opened" : "closed";
  node.secondary = text;

  // A dialog that is not modal is not where the user is. The event still goes out, because
  // a client may want to know something appeared, but it must not become the remembered
  // location -- which is exactly what went wrong in the first real run: the mouse pointer
  // dialog (WINDOW_DIALOG_POINTER) opened during startup, and the reader spent the next few
  // seconds believing it was in a dialog with nothing in it. Kodi draws the same line, in
  // GetActiveWindowOrDialog(), which promotes modal dialogs and nothing else.
  const StateKind kind = (dialog && !modal) ? StateKind::None : StateKind::Window;
  Send("OnAccessibilityWindow", node, kind);
}

void CAccessibilityAnnouncer::OnNotification(const std::string& caption,
                                             const std::string& description)
{
  CAccessibilityNode node;
  node.role = "notification";
  node.name = caption;
  node.secondary = description;
  Send("OnAccessibilityNotification", node, StateKind::None);
}

void CAccessibilityAnnouncer::OnSubtitle(const std::string& text)
{
  CAccessibilityNode node;
  node.role = "subtitle";
  node.name = text;
  Send("OnAccessibilitySubtitle", node, StateKind::None);
}

std::string CAccessibilityAnnouncer::GetSnapshotJSON()
{
  CVariant data(CVariant::VariantTypeObject);

  // What this build can emit, so a client learns it from a capability list rather than by
  // inferring it from a version number. It matters because the events a fork emits decide
  // what a client still has to poll for: there is no focus or selection event when only a
  // slider's value changes, so a reader has to keep watching that itself until this list
  // says otherwise.
  CVariant capabilities(CVariant::VariantTypeArray);
  capabilities.push_back("focus");
  capabilities.push_back("selection");
  capabilities.push_back("value");
  capabilities.push_back("window");
  capabilities.push_back("windowHeading");
  capabilities.push_back("notification");
  capabilities.push_back("subtitle");
  capabilities.push_back("snapshot");
  data["capabilities"] = std::move(capabilities);

  // A copy of what Kodi's own thread last recorded, and nothing else. No window manager, no
  // control tree, no graphics context lock -- see the header for why reading those from a
  // caller's thread is a lifetime race rather than merely a slow one. This lock is held for
  // a struct copy.
  SnapshotState state;
  {
    std::unique_lock<CCriticalSection> lock(m_stateSection);
    state = m_state;
  }

  data["seq"] = state.seq;

  // Stays -1 until a real window has actually been announced, and deliberately never
  // WINDOW_INVALID: that constant is 9999, a positive number a client checking "is this a
  // real window id" would happily accept. Accepting it would mean a screen reader deciding
  // it knows where it is when it does not, and stopping the polling that was covering for
  // it.
  data["window"] = state.window;
  data["dialog"] = state.dialog;
  data["modal"] = state.modal;
  data["heading"] = PlainText(state.heading);
  data["text"] = PlainText(state.text);

  // A real boolean rather than an absent key. A window genuinely reporting nothing focused
  // has to be distinguishable from Kodi not having told us anything yet, or a client can
  // never tell whether it is safe to stop polling -- and both guesses are bad. Stopping too
  // early means silence; never stopping means the polling this call exists to remove.
  data["hasFocus"] = state.hasFocus;
  if (state.hasFocus)
  {
    CVariant focus = NodeToVariant(state.focus);
    // The focus node's own dialog flag is left unset by BuildNode, because asking the
    // window manager on every focus would take the graphics context lock once per keypress.
    // Here the window record already knows, so the snapshot can carry what the events
    // cannot afford to.
    focus["dialog"] = state.dialog;
    data["focus"] = std::move(focus);
  }

  std::string json;
  if (!CJSONVariantWriter::Write(data, json, true))
    return {};

  return json;
}

} // namespace ACCESSIBILITY
} // namespace KODI
