/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/CriticalSection.h"

#include <atomic>
#include <cstdint>
#include <string>

class CGUIControl;

namespace KODI
{
namespace ACCESSIBILITY
{

/*!
 * \brief Everything a screen reader needs to speak one thing, each part in its own field.
 *
 * This struct is the point of the accessibility patch. Emitting events was the easy half:
 * Kodi's own surface gives out almost nothing to describe what the events are about.
 * CGUIControl::GetDescription() returns "" in the base implementation and only about half
 * of the control classes override it; a container returns nothing but its selected item's
 * label, with square brackets as the entire signal that something is a folder; a plain
 * slider returns its value and never its name.
 *
 * A client receiving only "control 52 got focus" would have to guess all of it back, which
 * is exactly what the unpatched addon has to do. So role, name, value, state, index and
 * count each get a field, and nothing has to be parsed out of a display string.
 */
struct CAccessibilityNode
{
  int window = -1;
  bool dialog = false;

  //! \brief Whether a dialog is modal, and so actually where the user is.
  bool modal = false;

  int control = -1;

  //! \brief One of the role names below. Empty when we have nothing honest to say.
  std::string role;
  std::string name;

  //! \brief A slider's or spinner's value, deliberately kept apart from its name.
  std::string value;

  //! \brief "checked" or "unchecked" for two-state controls, otherwise empty.
  std::string state;

  //! \brief The second line, for controls and items that have one.
  std::string secondary;

  //! \brief 1-based position within a container; -1 when the control is not in one.
  int index = -1;
  int total = -1;

  //! \brief A real boolean, rather than square brackets around the label.
  bool isFolder = false;
};

/*!
 * \brief Pushes accessibility nodes out through Kodi's existing announcement system.
 *
 * Announcements go out as ANNOUNCEMENT::GUI, so a Python addon receives them in
 * Monitor.onNotification as "GUI.OnAccessibility*" with no new API to bind.
 *
 * Every method here is static and the class holds no state beyond one atomic flag.
 * That is deliberate: there is nothing to construct, register or tear down, so there is
 * no startup or shutdown ordering in which announcements could quietly stop working.
 * For a screen reader, going silent is the worst failure there is.
 *
 * Threading: these are safe to call from the GUI and render threads.
 * CAnnouncementManager::Announce() copies its arguments, takes only its own queue lock
 * and returns; delivery happens later on the announcement thread. It takes no GUI lock
 * and does not touch the frame-move guard.
 *
 * There is one rule for callers. The announcement thread delivers into Python, which can
 * call back into Kodi, so never announce while holding a lock that a Python callback could
 * end up waiting on. Collect what you need, release the lock, then announce.
 */
class CAccessibilityAnnouncer
{
public:
  /*!
   * \brief A control has taken focus.
   *
   * Call from the GUI thread, at the point where focus has actually succeeded.
   */
  static void OnControlFocus(const CGUIControl* control);

  /*!
   * \brief A focused container's selected item has changed.
   *
   * Moving through a list changes no control focus at all -- the container keeps focus and
   * only its internal cursor moves -- so this is a separate event, and it is the one that
   * covers most of what anyone actually does in Kodi.
   */
  static void OnContainerSelection(const CGUIControl* container);

  /*!
   * \brief A focused control's value changed, with nothing else changing.
   *
   * Turning a spinner or dragging a slider moves no focus and shifts no list selection, so
   * none of the other entry points fire for it. Without this a client has to poll for value
   * changes, which is the last thing the patch left it having to do.
   *
   * Announce on genuine change only. The announcement queue is unbounded, and this is
   * reached from a per-frame path.
   */
  static void OnControlValue(const CGUIControl* control);

  /*!
   * \brief Text was typed into or removed from an edit control.
   *
   * \param text what was added, or what was removed when \p deleted is true. Never the
   *        contents of a password field -- the caller is responsible for that, and
   *        CGUIEditControl is where the masking lives.
   * \param deleted true when \p text was removed rather than entered. Reported as a state
   *        rather than a word, so the client says it in its own language.
   * \param emptied true when that removal left the field with nothing in it. A client has
   *        no way to work this out for itself -- it is told what changed, not what remains
   *        -- and "the box is now empty" is exactly what someone who cannot see it needs.
   */
  static void OnTextEdit(const CGUIControl* control,
                         const std::string& text,
                         bool deleted,
                         bool emptied);

  /*!
   * \brief A window or dialog opened or closed.
   *
   * \param heading what the window is called, from CGUIWindow::GetAccessibilityHeading().
   *        Carried on the event because Kodi already has it here; without it a client has
   *        to go back and read info labels to recover something it was just told about.
   * \param text content the window holds that is not reachable as a focusable control,
   *        which in practice means a message box's question.
   */
  /*!
   * \param modal whether a dialog is modal. Only modal dialogs are where the user is:
   *        Kodi's own GetActiveWindowOrDialog() promotes modal dialogs and nothing else,
   *        and something like the mouse pointer dialog or a background progress bar would
   *        otherwise be reported as the current screen. Ignored when \p dialog is false.
   */
  static void OnWindow(int windowID,
                       bool dialog,
                       bool modal,
                       bool opening,
                       const std::string& heading = {},
                       const std::string& text = {});

  /*!
   * \brief A toast notification was shown.
   *
   * Announced from the strings the dialog already holds. This cannot be done from a Python
   * addon at all: toasts are non-modal, so they appear neither as the current dialog nor as
   * the info-label context, and their text lands in fadelabels the Python wrapper cannot read.
   */
  static void OnNotification(const std::string& caption, const std::string& description);

  /*!
   * \brief The displayed subtitle text changed.
   */
  static void OnSubtitle(const std::string& text);

  /*!
   * \brief The last accessibility state Kodi announced, as JSON, for a client that has
   *        just started.
   *
   * This exists because Kodi's first window and focus events happen before a service addon's
   * Python does, and announcements are broadcast to whoever is registered at the time rather
   * than replayed. So a screen reader starts not knowing what window it is in or what has
   * focus, and the only way to find out otherwise is to poll during Kodi's busiest phase --
   * which is when the calls it would have to use are at their most expensive.
   *
   * **It is a replay of what was announced, not a fresh read of the GUI, and that is the
   * whole point.** The obvious implementation -- walk the window manager and the focused
   * control when asked -- is not safe from a addon thread. Kodi mutates the control tree
   * from paths that do not hold the graphics context lock: CGUIWindowManager::HandleAction()
   * unlocks before calling OnAction(), and CGUIDialog::Open_Internal() registers a dialog
   * and then initialises it unlocked, which can delete controls through ClearAll(). Holding
   * a lock that the writers do not take synchronises nothing, so traversing that tree from
   * another thread is a lifetime race, and a crash here means total silence for someone who
   * cannot see the screen.
   *
   * So the state is recorded on Kodi's own thread as each announcement is built, guarded by
   * its own small lock, and this call copies it. No GUI object is touched here.
   *
   * The `seq` carried is exactly the sequence number of the newest announcement reflected
   * in this record, updated with the record itself under one lock. An event whose `seq` is
   * not greater is therefore one already incorporated here, and a client may ignore it
   * without any argument about interleaving.
   *
   * `hasFocus` is a real boolean, so "this window has nothing focused" is distinguishable
   * from "Kodi has nothing to tell you yet" -- a client cannot otherwise know when it is
   * safe to stop polling.
   */
  static std::string GetSnapshotJSON();

  /*!
   * \brief Whether to do the per-frame work needed to notice subtitle changes.
   *
   * Only the subtitle path is gated, because only it has to do real work per rendered frame
   * to notice a change; the GUI hooks are already gated by an integer comparison on data
   * they hold anyway. It defaults to enabled so that a client which never sets it still
   * gets subtitles -- this fails open, never closed.
   */
  static bool SubtitlesEnabled() { return m_subtitlesEnabled; }
  static void SetSubtitlesEnabled(bool enabled) { m_subtitlesEnabled = enabled; }

private:
  /*!
   * \brief Which part of the remembered state an announcement updates.
   *
   * An explicit argument rather than a string comparison on the message name, so that
   * adding an announcement forces a decision about whether it is state. Subtitles and
   * toasts are `None`: they are things that happened, not where you are, and recording
   * them would make the snapshot describe a passing event as the user's location.
   */
  enum class StateKind
  {
    None,
    Window,
    Focus,
  };

  static void Send(const char* message, const CAccessibilityNode& node, StateKind kind);

  //! \brief Fold one announcement into the remembered state. Called on Kodi's own thread.
  static void RecordState(StateKind kind, const CAccessibilityNode& node, uint64_t seq);

  /*!
   * \brief Fill in whatever the control can honestly supply.
   * \return False when the control is not worth announcing at all.
   */
  static bool BuildNode(const CGUIControl* control, CAccessibilityNode& node);

  static std::atomic<bool> m_subtitlesEnabled;

  /*!
   * \brief Stamped on every announcement, so a snapshot can be ordered against them.
   *
   * Incremented in Send(). A snapshot samples it while holding the graphics context lock,
   * and the two emitters that matter announce from inside that same lock -- container
   * selection from CGUIWindowManager::Process(), control focus from
   * CGUIWindowManager::SendMessage() -- so an event either completes entirely before the
   * snapshot's read, and carries a lower number along with older state, or entirely after
   * it and carries a higher one. There is no interleaving that gives newer state a lower
   * number.
   */
  static std::atomic<uint64_t> m_sequence;

  /*!
   * \brief Where the user is, as last announced. See GetSnapshotJSON().
   */
  struct SnapshotState
  {
    uint64_t seq = 0;
    int window = -1;
    bool dialog = false;
    bool modal = false;
    std::string heading;
    std::string text;
    bool hasFocus = false;
    CAccessibilityNode focus;
  };

  static SnapshotState m_state;

  /*!
   * \brief Guards m_state only.
   *
   * Deliberately its own lock and not the graphics context. It is held for a struct copy
   * and nothing else, so it can never be the thing a render or GUI thread waits behind,
   * and it is the only lock this class takes.
   */
  static CCriticalSection m_stateSection;
};

} // namespace ACCESSIBILITY
} // namespace KODI
