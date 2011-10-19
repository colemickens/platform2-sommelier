// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/glib_io_ready_handler.h"

#include <base/logging.h>
#include <glib.h>
#include <stdio.h>
#include <sys/socket.h>

namespace shill {

static gboolean DispatchIOHandler(GIOChannel *chan,
                                  GIOCondition cond,
                                  gpointer data) {
  Callback1<int>::Type *callback = static_cast<Callback1<int>::Type *>(data);

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
    return FALSE;

  callback->Run(g_io_channel_unix_get_fd(chan));

  return TRUE;
}

GlibIOReadyHandler::GlibIOReadyHandler(int fd,
                                       IOHandler::ReadyMode mode,
                                       Callback1<int>::Type *callback)
    : channel_(NULL),
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
  g_source_remove(source_id_);
  g_io_channel_shutdown(channel_, TRUE, NULL);
  g_io_channel_unref(channel_);
}

void GlibIOReadyHandler::Start() {
  if (source_id_ == G_MAXUINT) {
    source_id_ = g_io_add_watch(channel_, condition_, DispatchIOHandler,
                                callback_);
  }
}

void GlibIOReadyHandler::Stop() {
  if (source_id_ != G_MAXUINT) {
    g_source_remove(source_id_);
    source_id_ = G_MAXUINT;
  }
}

}  // namespace shill
