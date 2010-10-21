// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_INOTIFY_H_
#define POWER_MANAGER_INOTIFY_H_

#include <glib.h>

#include "base/basictypes.h"

namespace power_manager {

class Inotify {
 public:
  typedef gboolean (*InotifyCallback)(const char* name,   // path name to watch
                                      int wd,             // watch handle
                                      unsigned int mask,  // inotify mask
                                      gpointer);          // data from init
  Inotify();
  ~Inotify();

  // |cb| is the callback used when an event occurs. |data| is passed to cb.
  bool Init(InotifyCallback cb, gpointer data);

  // Add an inotify watch on on path |name|. |mask| is an inotify event mask.
  // See linux/inotify.h for legal values.
  int AddWatch(const char* name, int mask);

  // Start inotify monitoring.
  void Start();

 private:
  // Internal GIOChannel G_IO_IN callback. Slurps as many events as are
  // available and calls the user's callback, callback_.
  // If any of the user's callback returns false, so do we, terminating this
  // watch, otherwise returns true.
  static gboolean CallbackHandler(GIOChannel* source,
                                  GIOCondition condition,
                                  gpointer data);

  GIOChannel* channel_;
  InotifyCallback callback_;
  gpointer callback_data_;
  guint gio_watch_id_;

  DISALLOW_COPY_AND_ASSIGN(Inotify);
};

} // namespace power_manager

#endif // POWER_MANAGER_INOTIFY_H_
