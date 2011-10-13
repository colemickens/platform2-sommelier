// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/glib_io_input_handler.h"

#include <stdio.h>
#include <glib.h>

namespace shill {

static gboolean DispatchIOHandler(GIOChannel *chan,
                                  GIOCondition cond,
                                  gpointer data) {
  Callback1<InputData*>::Type *callback =
      static_cast<Callback1<InputData*>::Type *>(data);
  unsigned char buf[4096];
  gsize len;
  GIOError err;
  gboolean ret = TRUE;

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
    return FALSE;

  err = g_io_channel_read(chan, reinterpret_cast<gchar *>(buf), sizeof(buf),
                          &len);
  if (err) {
    if (err == G_IO_ERROR_AGAIN)
      return TRUE;
    len = 0;
    ret = FALSE;
  }

  InputData input_data(buf, len);
  callback->Run(&input_data);

  return ret;
}

GlibIOInputHandler::GlibIOInputHandler(int fd,
                                       Callback1<InputData*>::Type *callback)
    : channel_(g_io_channel_unix_new(fd)),
      callback_(callback),
      source_id_(G_MAXUINT) {
  g_io_channel_set_close_on_unref(channel_, TRUE);
}

GlibIOInputHandler::~GlibIOInputHandler() {
  g_source_remove(source_id_);
  g_io_channel_shutdown(channel_, TRUE, NULL);
  g_io_channel_unref(channel_);
}

void GlibIOInputHandler::Start() {
  if (source_id_ == G_MAXUINT) {
    source_id_ = g_io_add_watch(channel_,
                                static_cast<GIOCondition>(
                                    G_IO_IN | G_IO_NVAL | G_IO_HUP | G_IO_ERR),
                                DispatchIOHandler, callback_);
  }
}

void GlibIOInputHandler::Stop() {
  if (source_id_ != G_MAXUINT) {
    g_source_remove(source_id_);
    source_id_ = G_MAXUINT;
  }
}

}  // namespace shill
