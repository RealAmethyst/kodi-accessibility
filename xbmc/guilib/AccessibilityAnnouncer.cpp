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
#include "GUIEditControl.h"
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
#include "utils/Variant.h"

namespace KODI
{
namespace ACCESSIBILITY
{

std::atomic<bool> CAccessibilityAnnouncer::m_subtitlesEnabled{true};

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

} // unnamed namespace

bool CAccessibilityAnnouncer::BuildNode(const CGUIControl* control, CAccessibilityNode& node)
{
  if (!control)
    return false;

  node.control = control->GetID();
  node.window = control->GetParentID();
  node.role = RoleForControl(control->GetControlType());

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

void CAccessibilityAnnouncer::Send(const char* message, const CAccessibilityNode& node)
{
  const std::shared_ptr<ANNOUNCEMENT::CAnnouncementManager> announcer =
      CServiceBroker::GetAnnouncementManager();

  // Null during early startup and late shutdown. Both are entirely normal, and neither is
  // worth crashing over.
  if (!announcer)
    return;

  CVariant data(CVariant::VariantTypeObject);
  data["window"] = node.window;
  data["dialog"] = node.dialog;
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

  // The three-argument overload fills in the standard sender, so this looks like every
  // other Kodi announcement and reaches Python as "GUI.<message>".
  announcer->Announce(ANNOUNCEMENT::GUI, message, data);
}

void CAccessibilityAnnouncer::OnControlFocus(const CGUIControl* control)
{
  CAccessibilityNode node;
  if (BuildNode(control, node))
    Send("OnAccessibilityFocus", node);
}

void CAccessibilityAnnouncer::OnContainerSelection(const CGUIControl* container)
{
  CAccessibilityNode node;
  if (BuildNode(container, node))
    Send("OnAccessibilitySelection", node);
}

void CAccessibilityAnnouncer::OnWindow(int windowID,
                                       bool dialog,
                                       bool opening,
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
  node.role = "window";
  node.state = opening ? "opened" : "closed";
  node.secondary = text;
  Send("OnAccessibilityWindow", node);
}

void CAccessibilityAnnouncer::OnNotification(const std::string& caption,
                                             const std::string& description)
{
  CAccessibilityNode node;
  node.role = "notification";
  node.name = caption;
  node.secondary = description;
  Send("OnAccessibilityNotification", node);
}

void CAccessibilityAnnouncer::OnSubtitle(const std::string& text)
{
  CAccessibilityNode node;
  node.role = "subtitle";
  node.name = text;
  Send("OnAccessibilitySubtitle", node);
}

} // namespace ACCESSIBILITY
} // namespace KODI
