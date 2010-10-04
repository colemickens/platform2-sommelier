// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome_event_source.h"

#include <base/logging.h>
#include <poll.h>
#include <unistd.h>

namespace cryptohome {

GSourceFuncs CryptohomeEventSource::source_functions_ = {
  CryptohomeEventSource::Prepare,
  CryptohomeEventSource::Check,
  CryptohomeEventSource::Dispatch,
  NULL
};

CryptohomeEventSource::CryptohomeEventSource()
    : sink_(NULL),
      source_(NULL),
      events_(),
      events_lock_(),
      poll_fd_() {
  pipe_fds_[0] = -1;
  pipe_fds_[1] = -1;
}

CryptohomeEventSource::~CryptohomeEventSource() {
  if (source_) {
    g_source_destroy(source_);
    g_source_unref(source_);
  }
  Clear();
}

void CryptohomeEventSource::Reset(CryptohomeEventSourceSink* sink,
                                  GMainContext* main_context) {
  sink_ = sink;
  if (source_) {
    g_source_destroy(source_);
    g_source_unref(source_);
    source_ = NULL;
  }

  for (int i = 0; i < 2; i++) {
    if (pipe_fds_[i] != -1) {
      close (pipe_fds_[i]);
      pipe_fds_[i] = -1;
    }
  }

  Clear();

  if (!pipe(pipe_fds_)) {
    poll_fd_.fd = pipe_fds_[0];
    poll_fd_.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
    poll_fd_.revents = 0;
    source_ = static_cast<Source*>(g_source_new(&source_functions_,
                                                sizeof(Source)));
    source_->event_source = this;
    g_source_add_poll(source_, &poll_fd_);
    if (main_context) {
      g_source_attach(source_, main_context);
    }
    g_source_set_can_recurse(source_, true);
  } else {
    LOG(ERROR) << "Couldn't set up pipe for notifications.";
  }
}

bool CryptohomeEventSource::EventsPending() {
  bool result = true;
  if (events_lock_.Try()) {
    result = !events_.empty();
    events_lock_.Release();
  }
  return result;
}

void CryptohomeEventSource::HandleDispatch() {
  // Clear pending notifications from the pipe.  This is done in reverse order
  // of the AddEvent() function so that we cannot get into a state where there
  // is an event queued but no pending read on the pipe.
  bool more = false;
  do {
    struct pollfd fds = { pipe_fds_[0], POLLIN, 0 };
    if (poll(&fds, 1, 0) && (fds.revents & POLLIN)) {
      char c;
      if (read(pipe_fds_[0], &c, 1) == 1) {
        more = true;
      } else {
        more = false;
      }
    } else {
      more = false;
    }
  } while(more);
  // Now handle pending events
  more = false;
  do {
    events_lock_.Acquire();
    if (!events_.empty()) {
      CryptohomeEventBase* event = events_[0];
      events_.erase(events_.begin());
      more = !events_.empty();
      events_lock_.Release();
      if (sink_) {
        sink_->NotifyEvent(event);
      }
      delete event;
    } else {
      more = false;
      events_lock_.Release();
    }
  } while (more);
}

void CryptohomeEventSource::AddEvent(CryptohomeEventBase* event) {
  events_lock_.Acquire();
  events_.push_back(event);
  if (write(pipe_fds_[1], "G", 1) != 1) {
    LOG(INFO) << "Couldn't notify of pending events through the message pipe."
              << "  Events will be cleared on next call to Prepare().";
  }
  events_lock_.Release();
}

void CryptohomeEventSource::Clear() {
  std::vector<CryptohomeEventBase*> events;
  events_lock_.Acquire();
  events_.swap(events);
  events_lock_.Release();
  for (std::vector<CryptohomeEventBase*>::const_iterator itr = events.begin();
       itr != events.end();
       ++itr) {
    delete (*itr);
  }
}

gboolean CryptohomeEventSource::Prepare(GSource* source, gint* timeout_ms) {
  if (static_cast<Source*>(source)->event_source->EventsPending()) {
    *timeout_ms = 0;
    return true;
  }
  *timeout_ms = -1;
  return false;
}

gboolean CryptohomeEventSource::Check(GSource* source) {
  return static_cast<Source*>(source)->event_source->EventsPending();
}

gboolean CryptohomeEventSource::Dispatch(GSource* source,
                                         GSourceFunc unused_func,
                                         gpointer unused_data) {
  static_cast<Source*>(source)->event_source->HandleDispatch();
  return true;
}

}  // namespace cryptohome
