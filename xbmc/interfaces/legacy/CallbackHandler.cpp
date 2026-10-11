/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "CallbackHandler.h"

#include "AddonUtils.h"
#include "commons/Exception.h"
#include "utils/log.h"

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace XBMCAddon
{
  class AsyncCallbackMessage : public AddonClass
  {
  public:
    AddonClass::Ref<Callback> cb;
    AddonClass::Ref<RetardedAsyncCallbackHandler> handler;
    AsyncCallbackMessage(Callback* _cb, RetardedAsyncCallbackHandler* _handler) :
      cb(_cb), handler(_handler) { XBMC_TRACE; }
  };

  using CallbackQueue = std::list<AddonClass::Ref<AsyncCallbackMessage>>;

  static CCriticalSection critSection;
  // Each language thread drains only its own FIFO. A dormant Monitor in a
  // different add-on must not make every screen-reader tick scan its backlog.
  static std::unordered_map<void*, CallbackQueue> g_callQueues;

  void RetardedAsyncCallbackHandler::invokeCallback(Callback* cb)
  {
    XBMC_TRACE;
    std::unique_lock<CCriticalSection> lock(critSection);
    g_callQueues[m_context].push_back(new AsyncCallbackMessage(cb,this));
  }

  RetardedAsyncCallbackHandler::~RetardedAsyncCallbackHandler()
  {
    XBMC_TRACE;
    std::unique_lock<CCriticalSection> lock(critSection);

    auto queue = g_callQueues.find(m_context);
    if (queue == g_callQueues.end())
      return;
    auto& calls = queue->second;
    for (auto iter = calls.begin(); iter != calls.end();)
    {
      if ((*iter)->handler.get() == this) // then this message is because of me
        iter = calls.erase(iter);
      else
        ++iter;
    }
    if (calls.empty())
      g_callQueues.erase(queue);
  }

  void RetardedAsyncCallbackHandler::makePendingCalls(void* context)
  {
    XBMC_TRACE;
    std::unique_lock<CCriticalSection> lock(critSection);
    auto queue = g_callQueues.find(context);
    if (queue == g_callQueues.end())
      return;
    // Messages enqueued by a callback are handled on the next pump, allowing
    // the caller to process navigation and abort requests between batches.
    size_t remaining = queue->second.size();
    while (remaining-- > 0)
    {
      queue = g_callQueues.find(context);
      if (queue == g_callQueues.end())
        break;
      auto& calls = queue->second;
      AddonClass::Ref<AsyncCallbackMessage> p(calls.front());

      // only call when we are in the right thread state
      if(p->handler->isStateOk(p->cb->getObject()))
      {
        // remove it from the queue. No matter what we're done with
        //  this. Even if it doesn't execute for some reason.
        calls.pop_front();
        if (calls.empty())
          g_callQueues.erase(queue);

        // we need to release the critSection lock prior to grabbing the
        //  lock on the object. Not doing so results in deadlocks. No queue
        //  references are used while the lock is released.
        {
          XBMCAddonUtils::InvertSingleLockGuard unlock(lock);

          // make sure the object is not deallocating

          // we need to grab the object lock to see if the object of the call
          //  is deallocating. holding this lock should prevent it from
          //  deallocating during the execution of this call.
#ifdef ENABLE_XBMC_TRACE_API
          CLog::Log(LOGDEBUG, "{}NEWADDON executing callback 0x{:x}", _tg.getSpaces(),
                    (long)(p->cb.get()));
#endif
          AddonClass* obj = (p->cb->getObject());
          AddonClass::Ref<AddonClass> ref(obj);
          std::unique_lock<CCriticalSection> lock2(*obj);
          if (!p->cb->getObject()->isDeallocating())
          {
            try
            {
              // need to make the call
              p->cb->executeCallback();
            }
            catch (XbmcCommons::Exception& e) { e.LogThrowMessage(); }
            catch (...)
            {
              CLog::Log(LOGERROR, "Unknown exception while executing callback {:#x}",
                        reinterpret_cast<int64_t>(p->cb.get()));
            }
          }
        }

      }
      else // if we're not in the right thread for this callback...
        calls.splice(calls.end(), calls, calls.begin());
    }
  }

  void RetardedAsyncCallbackHandler::clearPendingCalls(void* userData)
  {
    XBMC_TRACE;
    // Hold removed messages until after the map traversal: releasing their
    // handler can re-enter this map through its destructor.
    std::vector<AddonClass::Ref<AsyncCallbackMessage>> removed;
    {
      std::unique_lock<CCriticalSection> lock(critSection);
      for (auto queue = g_callQueues.begin(); queue != g_callQueues.end();)
      {
        auto& calls = queue->second;
        for (auto iter = calls.begin(); iter != calls.end();)
        {
          const auto& p = *iter;
          if (p->handler->shouldRemoveCallback(p->cb->getObject(), userData))
          {
            removed.push_back(p);
            iter = calls.erase(iter);
          }
          else
            ++iter;
        }
        if (calls.empty())
          queue = g_callQueues.erase(queue);
        else
          ++queue;
      }
    }
  }
}
