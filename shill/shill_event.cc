// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <glib.h>

#include "shill/shill_event.h"

namespace shill {

EventQueueItem::EventQueueItem(EventDispatcher *dispatcher)
  : dispatcher_(dispatcher) {
  dispatcher->RegisterCallbackQueue(this);
}

EventQueueItem::~EventQueueItem() {
  dispatcher_->UnregisterCallbackQueue(this);
}

void EventQueueItem::AlertDispatcher() {
  dispatcher_->ExecuteOnIdle();
}

void EventDispatcher::DispatchEvents() {
  for (size_t idx = 0; idx < queue_list_.size(); ++idx)
    queue_list_[idx]->Dispatch();
}

static gboolean DispatchEventsHandler(gpointer data) {
  EventDispatcher *dispatcher = static_cast<EventDispatcher *>(data);
  dispatcher->DispatchEvents();
  return false;
}

void EventDispatcher::ExecuteOnIdle() {
  g_idle_add(&DispatchEventsHandler, this);
}

void EventDispatcher::RegisterCallbackQueue(EventQueueItem *queue) {
  queue_list_.push_back(queue);
}

void EventDispatcher::UnregisterCallbackQueue(EventQueueItem *queue) {
  for (size_t idx = 0; idx < queue_list_.size(); ++idx) {
    if (queue_list_[idx] == queue) {
      queue_list_.erase(queue_list_.begin() + idx);
      return;
    }
  }
}

}  // namespace shill
