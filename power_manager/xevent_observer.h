// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_XEVENT_OBSERVER_H_
#define POWER_MANAGER_XEVENT_OBSERVER_H_

#include <glib.h>
#include <X11/Xlib.h>
#include <set>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/memory/singleton.h"

namespace power_manager {

// Used for X event handler return value, to determine whether the handled event
// should be passed to other handlers or discarded.
enum XEventHandlerStatus {
  XEVENT_HANDLER_CONTINUE,  // Let other event handlers process the event.
  XEVENT_HANDLER_STOP,      // Do not call any other handlers for this event.
};

// Interface for classes that are to handle X events.
class XEventObserverInterface {
 public:
  XEventObserverInterface() {}
  virtual ~XEventObserverInterface() {}

  // X event handler to be implemented by all classes that use this interface.
  virtual XEventHandlerStatus HandleXEvent(XEvent* event) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(XEventObserverInterface);
};

// Singleton class for passing X events to all XEventObserverInterface objects.
class XEventObserverManager {
 public:
  // Returns the singleton object.
  static XEventObserverManager* GetInstance();

  // Adds an XEventObserverInterface object to the list of observers.
  void AddObserver(XEventObserverInterface* observer);

  // Removes an XEventObserverInterface object from the list of observers.
  void RemoveObserver(XEventObserverInterface* observer);

 private:
  XEventObserverManager();
  ~XEventObserverManager();

  friend struct DefaultSingletonTraits<XEventObserverManager>;

  // X event poll function.  Handles any queued X events before waiting for more
  // X events.
  static gint PollXFd(GPollFD* fds, guint num_fds, gint timeout);

  // X event handler.
  static gboolean GIOEventHandler(GIOChannel*, GIOCondition, gpointer);

  // Tracks all XEventObserverInterface objects.
  std::set<XEventObserverInterface*> observers_;

  // A GIO channel for monitoring X events.
  GIOChannel* channel_;
  guint source_id_;

  // For base::Singleton.
  base::AtExitManager exit_manager_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_XEVENT_OBSERVER_H_
