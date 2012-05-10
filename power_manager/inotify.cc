// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/inotify.h"

#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "base/logging.h"

namespace power_manager {

// Buffer size for read of inotify event data.
static const gsize kInotifyBufferSize = 32768;

Inotify::Inotify()
    : channel_(NULL),
      callback_(NULL),
      callback_data_(NULL),
      gio_watch_id_(0) {}

Inotify::~Inotify() {
  if (channel_) {
    LOG(INFO) << "cleaning inotify";
    if (gio_watch_id_ > 0)
      g_source_remove(gio_watch_id_);
    int fd = g_io_channel_unix_get_fd(channel_);
    g_io_channel_shutdown(channel_, true, NULL);
    g_io_channel_unref(channel_);
    close(fd);
    LOG(INFO) << "done!";
  }
}

bool Inotify::Init(InotifyCallback callback, gpointer data) {
  int fd = inotify_init();
  if (fd < 0) {
    LOG(ERROR) << "Error in inotify_init";
    return false;
  }
  channel_ = g_io_channel_unix_new(fd);
  if (!channel_) {
    LOG(ERROR) << "Error creating gio channel for Inotify.";
    return false;
  }
  callback_ = callback;
  callback_data_ = data;
  return true;
}

int Inotify::AddWatch(const char* name, int mask) {
  int fd = g_io_channel_unix_get_fd(channel_);
  if (fd < 0) {
    LOG(ERROR) << "Error getting fd";
    return -1;
  }
  LOG(INFO) << "Creating watch for " << name;
  int watch_handle = inotify_add_watch(fd, name, mask);
  if (watch_handle < 0)
    LOG(ERROR) << "Error creating inotify watch for " << name;
  return watch_handle;
}

void Inotify::Start() {
  LOG(INFO) << "Starting Inotify Monitoring!";
  gio_watch_id_ = g_io_add_watch(channel_, G_IO_IN, &(Inotify::CallbackHandler),
                                 this);
}

gboolean Inotify::CallbackHandler(GIOChannel* source,
                                  GIOCondition condition,
                                  gpointer data) {
  if (G_IO_IN != condition)
    return false;
  Inotify* inotifier = static_cast<Inotify*>(data);
  if (!inotifier) {
    LOG(ERROR) << "Bad callback data!";
    return false;
  }
  InotifyCallback callback = inotifier->callback_;
  gpointer callback_data = inotifier->callback_data_;
  char buf[kInotifyBufferSize];
  gsize len;
  GIOError error = g_io_channel_read(source, buf, kInotifyBufferSize, &len);
  if (G_IO_ERROR_NONE != error) {
    LOG(ERROR) << "Error reading from inotify!";
    return false;
  }
  guint i = 0;
  while (i < len) {
    const char* name = "The watch";
    struct inotify_event* event = (struct inotify_event*) &buf[i];
    if (!event) {
      LOG(ERROR) << "garbage inotify_event data!";
      return false;
    }
    if (event->len)
      name = event->name;
    i += sizeof(struct inotify_event) + event->len;
    if (!callback)
      continue;
    if (false == (*callback)(name, event->wd, event->mask, callback_data)) {
      return false;
    }
  }
  return true;
}

}  // namespace power_manager
