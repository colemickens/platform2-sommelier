// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/glib_io_input_handler.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "shill/logging.h"

using base::Callback;

namespace shill {

static gboolean DispatchIOHandler(GIOChannel *chan,
                                  GIOCondition cond,
                                  gpointer data) {
  GlibIOInputHandler *handler = reinterpret_cast<GlibIOInputHandler *>(data);
  unsigned char buf[4096];
  gsize len = 0;
  gint fd = g_io_channel_unix_get_fd(chan);
  GError *err = 0;

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR))
    LOG(WARNING) << "Unexpected GLib error condition " << cond << " on poll("
               << fd << "): " << strerror(errno);

  GIOStatus status = g_io_channel_read_chars(
      chan, reinterpret_cast<gchar *>(buf), sizeof(buf), &len, &err);
  if (err) {
    LOG(WARNING) << "GLib error code " << err->domain << "/" << err->code
                 << " (" << err->message << ") on read(" << fd << "):"
                 << strerror(errno);
    g_error_free(err);
  }
  if (status == G_IO_STATUS_AGAIN)
    return TRUE;
  if (status != G_IO_STATUS_NORMAL)
    LOG(FATAL) << "Unexpected GLib return status " << status;

  InputData input_data(buf, len);
  handler->callback().Run(&input_data);

  return TRUE;
}

GlibIOInputHandler::GlibIOInputHandler(
    int fd, const Callback<void(InputData *)> &callback)
    : channel_(g_io_channel_unix_new(fd)),
      callback_(callback),
      source_id_(G_MAXUINT) {
  // To avoid blocking in g_io_channel_read_chars() due to its internal buffer,
  // set the channel to unbuffered, which in turns requires encoding to be NULL.
  // This assumes raw binary data are read from |fd| via the channel.
  CHECK_EQ(G_IO_STATUS_NORMAL, g_io_channel_set_encoding(channel_, NULL, NULL));
  g_io_channel_set_buffered(channel_, FALSE);
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
                                DispatchIOHandler, this);
  }
}

void GlibIOInputHandler::Stop() {
  if (source_id_ != G_MAXUINT) {
    g_source_remove(source_id_);
    source_id_ = G_MAXUINT;
  }
}

}  // namespace shill
