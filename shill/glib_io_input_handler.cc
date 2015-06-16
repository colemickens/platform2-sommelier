// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/glib_io_input_handler.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

using base::Callback;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

static gboolean DispatchIOHandler(GIOChannel* chan,
                                  GIOCondition cond,
                                  gpointer data) {
  GlibIOInputHandler* handler = reinterpret_cast<GlibIOInputHandler*>(data);
  unsigned char buf[IOHandler::kDataBufferSize];
  gsize len = 0;
  gint fd = g_io_channel_unix_get_fd(chan);
  GError* err = 0;
  vector<string> error_conditions;

  if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
    string condition = base::StringPrintf(
        "Unexpected GLib error condition %#x on poll(%d): %s",
        cond, fd, strerror(errno));
    LOG(WARNING) << condition;
    error_conditions.push_back(condition);
  }

  GIOStatus status = g_io_channel_read_chars(
      chan, reinterpret_cast<gchar*>(buf), sizeof(buf), &len, &err);
  if (err) {
    string condition = base::StringPrintf(
        "GLib error code %d/%d (%s) on read(%d): %s",
        err->domain, err->code, err->message, fd, strerror(errno));
    LOG(WARNING) << condition;
    error_conditions.push_back(condition);
    g_error_free(err);
  }
  if (status == G_IO_STATUS_AGAIN)
    return TRUE;
  if (status == G_IO_STATUS_ERROR) {
    string condition = base::StringPrintf(
        "Unexpected GLib return status: %d", status);
    LOG(ERROR) << condition;
    error_conditions.push_back(condition);
    handler->error_callback().Run(JoinString(error_conditions, ';'));
    return FALSE;
  }

  InputData input_data(buf, len);
  handler->input_callback().Run(&input_data);

  if (status == G_IO_STATUS_EOF) {
    LOG(INFO) << "InputHandler on fd " << fd << " closing due to EOF.";
    CHECK_EQ(0U, len);
    return FALSE;
  }
  return TRUE;
}

GlibIOInputHandler::GlibIOInputHandler(
    int fd,
    const InputCallback& input_callback,
    const ErrorCallback& error_callback)
    : channel_(g_io_channel_unix_new(fd)),
      input_callback_(input_callback),
      error_callback_(error_callback),
      source_id_(G_MAXUINT) {
  // To avoid blocking in g_io_channel_read_chars() due to its internal buffer,
  // set the channel to unbuffered, which in turns requires encoding to be NULL.
  // This assumes raw binary data are read from |fd| via the channel.
  CHECK_EQ(G_IO_STATUS_NORMAL,
           g_io_channel_set_encoding(channel_, nullptr, nullptr));
  g_io_channel_set_buffered(channel_, FALSE);
  g_io_channel_set_close_on_unref(channel_, TRUE);
}

GlibIOInputHandler::~GlibIOInputHandler() {
  g_source_remove(source_id_);
  g_io_channel_shutdown(channel_, TRUE, nullptr);
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
