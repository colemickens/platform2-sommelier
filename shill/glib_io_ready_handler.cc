//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/glib_io_ready_handler.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include <base/logging.h>

using base::Callback;

namespace shill {

static gboolean DispatchIOHandler(GIOChannel* chan,
                                  GIOCondition cond,
                                  gpointer data) {
  GlibIOReadyHandler* handler = reinterpret_cast<GlibIOReadyHandler*>(data);
  gint fd = g_io_channel_unix_get_fd(chan);

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
    LOG(WARNING) << "Unexpected GLib error condition " << cond << " on poll("
               << fd << "): " << strerror(errno);

  handler->callback().Run(fd);

  return TRUE;
}

GlibIOReadyHandler::GlibIOReadyHandler(int fd,
                                       IOHandler::ReadyMode mode,
                                       const Callback<void(int)>& callback)
    : channel_(nullptr),
      callback_(callback),
      source_id_(G_MAXUINT) {
  if (mode == kModeInput) {
    condition_ = static_cast<GIOCondition>(
        G_IO_IN | G_IO_NVAL | G_IO_HUP | G_IO_ERR);
  } else if (mode == kModeOutput) {
    condition_ = static_cast<GIOCondition>(
        G_IO_OUT | G_IO_NVAL | G_IO_HUP | G_IO_ERR);
  } else {
    NOTREACHED() << "Unknown IO ready mode: " << mode;
  }

  channel_ = g_io_channel_unix_new(fd);
  g_io_channel_set_close_on_unref(channel_, FALSE);
}

GlibIOReadyHandler::~GlibIOReadyHandler() {
  GlibIOReadyHandler::Stop();
  // NB: We don't shut down the channel since we don't own it
  g_io_channel_unref(channel_);
}

void GlibIOReadyHandler::Start() {
  if (source_id_ == G_MAXUINT) {
    source_id_ = g_io_add_watch(channel_, condition_, DispatchIOHandler, this);
  }
}

void GlibIOReadyHandler::Stop() {
  if (source_id_ != G_MAXUINT) {
    g_source_remove(source_id_);
    source_id_ = G_MAXUINT;
  }
}

}  // namespace shill
