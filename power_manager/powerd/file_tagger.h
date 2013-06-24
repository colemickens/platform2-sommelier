// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_FILE_TAGGER_H_
#define POWER_MANAGER_POWERD_FILE_TAGGER_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <map>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/common/inotify.h"

namespace base {
class Time;
}  // namespace base

namespace power_manager {

class FileTagger {
 public:
  explicit FileTagger(const base::FilePath& trace_dir);

  void Init();

  // Event-based callbacks
  void HandleSuspendEvent();        // Tags the suspend file
  void HandleResumeEvent();         // Tags the resume file
  void HandleLowBatteryEvent();     // Creates the low battery indicator file
  void HandleSafeBatteryEvent();    // Deletes the low battery indicator file

  bool can_tag_files() const {
    return can_tag_files_;
  }

 private:
  friend class FileTaggerTest;
  FRIEND_TEST(FileTaggerTest, SuspendFile);
  FRIEND_TEST(FileTaggerTest, LowBatteryFile);
  FRIEND_TEST(FileTaggerTest, FileCache);

  // For file access
  bool TouchFile(const base::FilePath& file);
  bool DeleteFile(const base::FilePath& file);

  // Initialize the Inotify for trace files
  bool SetupTraceFileNotifier();

  // Callback for Inotify of trace file changes.
  // Handles deletion of the trace files, signaling that the data has been
  // collected, and can be written to anew.  This depends on crash reporter to
  // do the right thing by deleting the files after it reads them.
  static gboolean TraceFileChangeHandler(const char* name, int watch_handle,
                                         unsigned int mask, gpointer data);

  // For file system notifications and changes
  Inotify notifier_;
  bool can_tag_files_;

  // Directory where files are tagged
  base::FilePath trace_dir_;
  // Files to be tagged
  base::FilePath suspend_file_;
  base::FilePath low_battery_file_;

  // Used for recording writes/deletes before the crash reporter has removed
  // the previous trace files.
  typedef std::map<base::FilePath, base::Time> FileCache;
  FileCache cached_files_;

  DISALLOW_COPY_AND_ASSIGN(FileTagger);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_FILE_TAGGER_H_
