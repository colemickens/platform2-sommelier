// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
