/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "AddonString.h"
#include "guilib/GUIEditControl.h"
#include "swighelper.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace XBMCAddon
{
  namespace xbmcgui
  {
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

    //
    /// \defgroup python_xbmcgui Library - xbmcgui
    /// @{
    /// @brief **GUI functions on Kodi.**
    ///
    /// Offers classes and functions that manipulate the Graphical User
    /// Interface through windows, dialogs, and various control widgets.
    //

#ifdef DOXYGEN_SHOULD_USE_THIS
    ///
    /// \ingroup python_xbmcgui
    /// @brief \python_func{ xbmcgui.getCurrentWindowId() }
    /// Returns the id for the current 'active' window as an integer.
    ///
    /// @return                        The currently active window Id
    ///
    ///
    ///--------------------------------------------------------------------------
    ///
    /// **Example:**
    /// ~~~~~~~~~~~~~{.py}
    /// ..
    /// wid = xbmcgui.getCurrentWindowId()
    /// ..
    /// ~~~~~~~~~~~~~
    ///
    getCurrentWindowId();
#else
    long getCurrentWindowId();
#endif

#ifdef DOXYGEN_SHOULD_USE_THIS
    ///
    /// \ingroup python_xbmcgui
    /// @brief \python_func{ xbmcgui.getCurrentWindowDialogId() }
    /// Returns the id for the current 'active' dialog as an integer.
    ///
    /// @return                        The currently active dialog Id
    ///
    ///
    ///--------------------------------------------------------------------------
    ///
    /// **Example:**
    /// ~~~~~~~~~~~~~{.py}
    /// ..
    /// wid = xbmcgui.getCurrentWindowDialogId()
    /// ..
    /// ~~~~~~~~~~~~~
    ///
    getCurrentWindowDialogId();
#else
    long getCurrentWindowDialogId();
#endif

#ifdef DOXYGEN_SHOULD_USE_THIS
    ///
    /// \ingroup python_xbmcgui
    /// @brief \python_func{ getScreenHeight() }
    /// Returns the height of this screen.
    ///
    /// @return                       Screen height
    ///
    ///
    ///-------------------------------------------------------------------------
    /// @python_v18 New function added.
    ///
    getScreenHeight();
#else
    long getScreenHeight();
#endif

#ifdef DOXYGEN_SHOULD_USE_THIS
    ///
    /// \ingroup python_xbmcgui
    /// @brief \python_func{ getScreenWidth() }
    /// Returns the width of this screen.
    ///
    /// @return                       Screen width
    ///
    ///
    ///-------------------------------------------------------------------------
    /// @python_v18 New function added.
    ///
    getScreenWidth();
#else
    long getScreenWidth();
#endif

#ifdef DOXYGEN_SHOULD_USE_THIS
    ///
    /// \ingroup python_xbmcgui
    /// @brief \python_func{ xbmcgui.getAccessibilitySnapshot() }
    /// Returns the current accessibility state as a JSON string.
    ///
    /// Kodi's first window and focus announcements happen before a service addon's
    /// Python exists, and they are not replayed, so a screen reader starting up has no
    /// way to learn where it is except by polling. This answers that in one call.
    ///
    /// The object carries `seq`, `window`, `dialog`, `heading`, `text`, `capabilities`
    /// and `hasFocus`, plus a `focus` object when something is focused. Compare `seq`
    /// against the `seq` on accessibility announcements to discard any that describe
    /// state this snapshot already supersedes.
    ///
    /// @return                        The accessibility state as JSON, or an empty string
    ///
    ///
    ///--------------------------------------------------------------------------
    ///
    /// **Example:**
    /// ~~~~~~~~~~~~~{.py}
    /// ..
    /// state = json.loads(xbmcgui.getAccessibilitySnapshot())
    /// ..
    /// ~~~~~~~~~~~~~
    ///
    getAccessibilitySnapshot();
#else
    String getAccessibilitySnapshot();
#endif
    /// Read the active dialog's visible passive text on Kodi's GUI thread.
    /// Returns JSON with window, dialog, heading and text; empty on shutdown.
    String getAccessibilityWindowText();
    ///@}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    SWIG_CONSTANT2(int, ICON_OVERLAY_NONE, CGUIListItem::ICON_OVERLAY_NONE);
    SWIG_CONSTANT2(int, ICON_OVERLAY_RAR, CGUIListItem::ICON_OVERLAY_RAR);
    SWIG_CONSTANT2(int, ICON_OVERLAY_ZIP, CGUIListItem::ICON_OVERLAY_ZIP);
    SWIG_CONSTANT2(int, ICON_OVERLAY_LOCKED, CGUIListItem::ICON_OVERLAY_LOCKED);
    SWIG_CONSTANT2(int, ICON_OVERLAY_UNWATCHED, CGUIListItem::ICON_OVERLAY_UNWATCHED);
    SWIG_CONSTANT2(int, ICON_OVERLAY_WATCHED, CGUIListItem::ICON_OVERLAY_WATCHED);
    SWIG_CONSTANT2(int, ICON_OVERLAY_HD, CGUIListItem::ICON_OVERLAY_HD);

    SWIG_CONSTANT2(int, INPUT_TYPE_TEXT, CGUIEditControl::INPUT_TYPE_TEXT);
    SWIG_CONSTANT2(int, INPUT_TYPE_NUMBER, CGUIEditControl::INPUT_TYPE_NUMBER);
    SWIG_CONSTANT2(int, INPUT_TYPE_DATE, CGUIEditControl::INPUT_TYPE_DATE);
    SWIG_CONSTANT2(int, INPUT_TYPE_TIME, CGUIEditControl::INPUT_TYPE_TIME);
    SWIG_CONSTANT2(int, INPUT_TYPE_IPADDRESS, CGUIEditControl::INPUT_TYPE_IPADDRESS);
    SWIG_CONSTANT2(int, INPUT_TYPE_PASSWORD, CGUIEditControl::INPUT_TYPE_PASSWORD);
    SWIG_CONSTANT2(int, INPUT_TYPE_PASSWORD_MD5, CGUIEditControl::INPUT_TYPE_PASSWORD_MD5);
    SWIG_CONSTANT2(int, INPUT_TYPE_SECONDS, CGUIEditControl::INPUT_TYPE_SECONDS);
    SWIG_CONSTANT2(int, INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW, CGUIEditControl::INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW);

    SWIG_CONSTANT_FROM_GETTER(const char*, NOTIFICATION_INFO);
    SWIG_CONSTANT_FROM_GETTER(const char*, NOTIFICATION_WARNING);
    SWIG_CONSTANT_FROM_GETTER(const char*, NOTIFICATION_ERROR);

    SWIG_CONSTANT(int, INPUT_ALPHANUM);
    SWIG_CONSTANT(int, INPUT_NUMERIC);
    SWIG_CONSTANT(int, INPUT_DATE);
    SWIG_CONSTANT(int, INPUT_TIME);
    SWIG_CONSTANT(int, INPUT_IPADDRESS);
    SWIG_CONSTANT(int, INPUT_PASSWORD);

    SWIG_CONSTANT(int, HORIZONTAL);
    SWIG_CONSTANT(int, VERTICAL);

    SWIG_CONSTANT(int, PASSWORD_VERIFY);
    SWIG_CONSTANT(int, ALPHANUM_HIDE_INPUT);

  }
}
#endif /* DOXYGEN_SHOULD_SKIP_THIS */
