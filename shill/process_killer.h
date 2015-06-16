// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROCESS_KILLER_H_
#define SHILL_PROCESS_KILLER_H_

#include <map>

#include <base/callback.h>
#include <base/lazy_instance.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

// The ProcessKiller singleton can be used to asynchronously and robustly
// terminate and reap child processes by their process IDs.
class ProcessKiller {
 public:
  virtual ~ProcessKiller();

  // This is a singleton -- use ProcessKiller::GetInstance()->Foo().
  static ProcessKiller* GetInstance();

  // Uses GLib to wait asynchronously for the process to exit and reap it. GLib
  // supports only a single callback per process ID so there should be no other
  // child watch callbacks registered for this |pid|. If |callback| is non-null,
  // runs it when the process exits. Returns false if the process has already
  // exited.
  virtual bool Wait(int pid, const base::Closure& callback);

  // Terminates process |pid| and reaps it through Wait(pid, callback).
  virtual void Kill(int pid, const base::Closure& callback);

 protected:
  ProcessKiller();

 private:
  friend struct base::DefaultLazyInstanceTraits<ProcessKiller>;
  FRIEND_TEST(ProcessKillerTest, OnProcessDied);

  static void OnProcessDied(GPid pid, gint status, gpointer data);

  std::map<int, base::Closure> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ProcessKiller);
};

}  // namespace shill

#endif  // SHILL_PROCESS_KILLER_H_
