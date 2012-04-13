// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/xevent_observer.h"

#include <list>

#include "base/logging.h"
#include "power_manager/util.h"

namespace power_manager {

XEventObserverManager* XEventObserverManager::GetInstance() {
  return Singleton<XEventObserverManager>::get();
}

// Adds an XEventObserverInterface object to the list of observers.
void XEventObserverManager::AddObserver(XEventObserverInterface* observer) {
  CHECK(observers_.find(observer) == observers_.end())
    << "Attempting to add observer that has already been added.";
  observers_.insert(observer);
}

// Removes an XEventObserverInterface object from the list of observers.
void XEventObserverManager::RemoveObserver(XEventObserverInterface* observer) {
  CHECK(observers_.find(observer) != observers_.end())
    << "Attempting to remove observer that has not been added.";
  observers_.erase(observer);
}


XEventObserverManager::XEventObserverManager() {
  int fd = XConnectionNumber(util::GetDisplay());
  channel_ = g_io_channel_unix_new(fd);
  CHECK(channel_) << "Unable to create GIO channel for X display.";
  source_id_ = g_io_add_watch(
      channel_,
      static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR),
      GIOEventHandler,
      NULL);
  CHECK(source_id_ > 0) << "Unable to create GIO watch source for X display.";

  // Use XEventObserver's custom polling function for glib polling.
  CHECK(g_main_context_get_poll_func(NULL) == g_poll)
      << "A custom glib main context poll function has already been set.";
  g_main_context_set_poll_func(NULL, PollXFd);
}

XEventObserverManager::~XEventObserverManager() {
  CHECK(g_main_context_get_poll_func(NULL) == PollXFd)
      << "Unexpected / unexpected glib main context poll function found.";
  g_main_context_set_poll_func(NULL, g_poll);
  g_source_remove(source_id_);
  g_io_channel_shutdown(channel_, true, NULL);
  g_io_channel_unref(channel_);
}

// static
gint XEventObserverManager::PollXFd(GPollFD* fds, guint num_fds, gint timeout) {
  // If X requests were made in the previous iteration of the event loop,
  // Xlib could've read additional events from the FD while waiting for the
  // replies. Those additional events would be in Xlib's internal queue now,
  // so we need to check for them before polling the FD.
  if (XPending(util::GetDisplay()) > 0)
    GIOEventHandler(NULL, G_IO_IN, NULL);
  return g_poll(fds, num_fds, timeout);
}

// static
gboolean XEventObserverManager::GIOEventHandler(GIOChannel*,
                                                GIOCondition,
                                                gpointer) {  // NOLINT
  Display* display = util::GetDisplay();
  // Handle each X event with the event handler.
  while (XPending(display) > 0) {
    XEvent event;
    XNextEvent(display, &event);
    std::set<XEventObserverInterface*>::const_iterator iter;
    for (iter = GetInstance()->observers_.begin();
         iter != GetInstance()->observers_.end();
         ++iter) {
      XEventObserverInterface* observer = *iter;
      XEventHandlerStatus status = observer->HandleXEvent(&event);
      bool done_processing_event = false;
      switch (status) {
        case XEVENT_HANDLER_CONTINUE:
          done_processing_event = false;
          break;
        case XEVENT_HANDLER_STOP:
          done_processing_event = true;
          break;
        default:
          NOTREACHED();
      }
      if (done_processing_event)
        break;
    }
  }
  return TRUE;
}

}  // namespace power_manager
