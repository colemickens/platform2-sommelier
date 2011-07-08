// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <glib.h>

#include "shill/glib_io_handler.h"

namespace shill {

static gboolean DispatchIOHandler(GIOChannel *chan,
                                  GIOCondition cond,
                                  gpointer data);

GlibIOInputHandler::GlibIOInputHandler(int fd,
                                       Callback1<InputData*>::Type *callback)
    : callback_(callback) {
  channel_ = g_io_channel_unix_new(fd);
  g_io_channel_set_close_on_unref(channel_, TRUE);
  source_id_ = g_io_add_watch(channel_,
                              (GIOCondition)(G_IO_IN | G_IO_NVAL |
                                             G_IO_HUP | G_IO_ERR),
                              DispatchIOHandler, this);
}

GlibIOInputHandler::~GlibIOInputHandler() {
  g_source_remove(source_id_);
  g_io_channel_shutdown(channel_, TRUE, NULL);
  g_io_channel_unref(channel_);
}

static gboolean DispatchIOHandler(GIOChannel *chan,
                                  GIOCondition cond,
                                  gpointer data) {
  GlibIOInputHandler *handler = static_cast<GlibIOInputHandler *>(data);
  unsigned char buf[4096];
  gsize len;
  GIOError err;

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
    return FALSE;

  err = g_io_channel_read(chan, reinterpret_cast<gchar *>(buf), sizeof(buf),
                          &len);
  if (err) {
    if (err == G_IO_ERROR_AGAIN)
      return TRUE;
    return FALSE;
  }

  InputData input_data(buf, len);
  handler->callback_->Run(&input_data);

  return TRUE;
}

}  // namespace shill
