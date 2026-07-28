/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ModuleXbmcgui.h"

#include "LanguageHook.h"
#include "ServiceBroker.h"
#include "guilib/AccessibilityAnnouncer.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "windowing/GraphicContext.h"

#include <mutex>

#define NOTIFICATION_INFO     "info"
#define NOTIFICATION_WARNING  "warning"
#define NOTIFICATION_ERROR    "error"

namespace XBMCAddon
{
  namespace xbmcgui
  {
    long getCurrentWindowId()
    {
      DelayedCallGuard dg;
      std::unique_lock<CCriticalSection> gl(CServiceBroker::GetWinSystem()->GetGfxContext());
      return CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindow();
    }

    long getCurrentWindowDialogId()
    {
      DelayedCallGuard dg;
      std::unique_lock<CCriticalSection> gl(CServiceBroker::GetWinSystem()->GetGfxContext());
      return CServiceBroker::GetGUI()->GetWindowManager().GetTopmostModalDialog();
    }

    long getScreenHeight()
    {
      XBMC_TRACE;
      return CServiceBroker::GetWinSystem()->GetGfxContext().GetHeight();
    }

    long getScreenWidth()
    {
      XBMC_TRACE;
      return CServiceBroker::GetWinSystem()->GetGfxContext().GetWidth();
    }

    String getAccessibilitySnapshot()
    {
      XBMC_TRACE;
      // No DelayedCallGuard and no graphics context lock. This returns a copy of state
      // Kodi's own thread recorded as it announced it, so the only lock involved is the
      // announcer's own, held for a struct copy. Deliberately not a live read of the
      // window manager and control tree: Kodi mutates those from paths that do not hold
      // the graphics lock, so reading them from this thread would be a lifetime race
      // rather than merely a slow call.
      return KODI::ACCESSIBILITY::CAccessibilityAnnouncer::GetSnapshotJSON();
    }

    const char* getNOTIFICATION_INFO()    { return NOTIFICATION_INFO; }
    const char* getNOTIFICATION_WARNING() { return NOTIFICATION_WARNING; }
    const char* getNOTIFICATION_ERROR()   { return NOTIFICATION_ERROR; }

  }
}
