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

static gboolean DispatchIOHandler(GIOChannel *chan, GIOCondition cond,
                                  gpointer data);

class GlibIOInputHandler : public IOInputHandler {
public:
  GIOChannel *channel_;
  Callback<InputData *> *callback_;
  guint source_id_;
  EventDispatcher *dispatcher_;

  GlibIOInputHandler(EventDispatcher *dispatcher, int fd,
                     Callback<InputData *> *callback) :
    dispatcher_(dispatcher), callback_(callback) {
    channel_ = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(channel_, TRUE);
    source_id_ = g_io_add_watch(channel_,
                                (GIOCondition)(G_IO_IN | G_IO_NVAL |
                                               G_IO_HUP | G_IO_ERR),
                                DispatchIOHandler, this);
  }

  ~GlibIOInputHandler() {
    g_source_remove(source_id_);
    g_io_channel_shutdown(channel_, TRUE, NULL);
    g_io_channel_unref(channel_);
  }
};

static gboolean DispatchIOHandler(GIOChannel *chan, GIOCondition cond,
                                  gpointer data) {
  GlibIOInputHandler *handler = static_cast<GlibIOInputHandler *>(data);
  unsigned char buf[4096];
  gsize len;
  GIOError err;

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
    return FALSE;

  err = g_io_channel_read(chan, (gchar *) buf, sizeof(buf), &len);
  if (err) {
    if (err == G_IO_ERROR_AGAIN)
      return TRUE;
    return FALSE;
  }

  InputData input_data = { buf, len };
  handler->callback_->Run(&input_data);

  return TRUE;
}

IOInputHandler *EventDispatcher::CreateInputHandler(int fd,
        Callback<InputData *> *callback) {
  return new GlibIOInputHandler(this, fd, callback);
}

}  // namespace shill
