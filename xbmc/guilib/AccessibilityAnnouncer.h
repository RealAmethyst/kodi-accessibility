/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <atomic>
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
   * \brief A window or dialog opened or closed.
   */
  static void OnWindow(int windowID, bool dialog, bool opening, const std::string& text = {});

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
  static void Send(const char* message, const CAccessibilityNode& node);

  /*!
   * \brief Fill in whatever the control can honestly supply.
   * \return False when the control is not worth announcing at all.
   */
  static bool BuildNode(const CGUIControl* control, CAccessibilityNode& node);

  static std::atomic<bool> m_subtitlesEnabled;
};

} // namespace ACCESSIBILITY
} // namespace KODI
