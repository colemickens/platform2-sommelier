// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/glib-process.h"

#include <glib.h>

#include <base/bind.h>
#include <gtest/gtest.h>

namespace cros_disks {

class GlibProcessTest : public ::testing::Test {
 public:
  void OnProcessTerminated(GlibProcess* process) {
    EXPECT_EQ(&process_, process);
    callback_invoked_ = true;
    g_main_loop_quit(main_loop_);
  }

 protected:
  GlibProcessTest()
      : callback_invoked_(false),
        main_loop_(NULL),
        timeout_id_(0) {
  }

  static gboolean OnTimeout(gpointer data) {
    GMainLoop* main_loop = static_cast<GMainLoop*>(data);
    g_main_loop_quit(main_loop);
    return TRUE;
  }

  // Runs the Glib main loop and waits for the termination of the process
  // under test. Returns true if the process termination callback is invoked.
  bool WaitForProcessTermination(int64 timeout_in_seconds) {
    callback_invoked_ = false;

    main_loop_ = g_main_loop_new(g_main_context_default(), FALSE);
    timeout_id_ = g_timeout_add_seconds(timeout_in_seconds,
                                        &GlibProcessTest::OnTimeout,
                                        main_loop_);
    g_main_loop_run(main_loop_);

    g_source_remove(timeout_id_);
    timeout_id_ = 0;

    g_main_loop_unref(main_loop_);
    main_loop_ = NULL;

    return callback_invoked_;
  }

  // Set to true once OnProcessTerminated() is invoked.
  bool callback_invoked_;
  GlibProcess process_;
  GMainLoop* main_loop_;
  guint timeout_id_;
};

TEST_F(GlibProcessTest, Start) {
  process_.AddArgument("/bin/ls");
  process_.set_callback(base::Bind(&GlibProcessTest::OnProcessTerminated,
                                   base::Unretained(this)));
  EXPECT_TRUE(process_.Start());
  EXPECT_NE(0, process_.pid());
  EXPECT_TRUE(WaitForProcessTermination(10));
  // Process ID should remain unchanged after process termination.
  EXPECT_NE(0, process_.pid());
}

}  // namespace cros_disks
