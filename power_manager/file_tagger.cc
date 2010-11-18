// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/file_tagger.h"

#include <sys/inotify.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/time.h"

namespace power_manager {

// Files used for crash reporting
// File with timestamp of last suspend
static const char kPowerdSuspendFile[] = "powerd_suspending";
// File with timestamp of last resume
static const char kPowerdResumeFile[] = "powerd_resuming";
// Presence of this file indicates that the battery is critically low and the
// system is running on battery, not on AC power.
static const char kPowerdLowBatteryFile[] = "powerd_low_battery";

FileTagger::FileTagger(const FilePath& trace_dir)
  : can_tag_files_(false),
    trace_dir_(trace_dir),
    suspend_file_(trace_dir.Append(kPowerdSuspendFile)),
    resume_file_(trace_dir.Append(kPowerdResumeFile)),
    low_battery_file_(trace_dir.Append(kPowerdLowBatteryFile)) {}

void FileTagger::Init() {
  SetupTraceFileNotifier();

  // If all three trace files have been deleted, allow tracing.
  // This prevents the files from being overwritten until the crash reporter
  // has collected the data from them.  When crash reporter is done, it will
  // delete them, at which point the tagger can start a new round of file
  // writes/deletes.
  if (!file_util::PathExists(suspend_file_) &&
      !file_util::PathExists(resume_file_) &&
      !file_util::PathExists(low_battery_file_)) {
    LOG(INFO) << "Enabling trace file tagging";
    can_tag_files_ = true;
  } else {
    LOG(INFO) << "Not enabling trace file tagging";
  }
}

void FileTagger::HandleSuspendEvent() {
  TouchFile(suspend_file_);
}

void FileTagger::HandleResumeEvent() {
  TouchFile(resume_file_);
}

void FileTagger::HandleLowBatteryEvent() {
  TouchFile(low_battery_file_);
}

void FileTagger::HandleSafeBatteryEvent() {
  DeleteFile(low_battery_file_);
}

bool FileTagger::TouchFile(const FilePath& file_path) {
  if (can_tag_files_) {
    return file_util::WriteFile(FilePath(file_path), "", 0) == 0;
  }
  // If file access is not allowed yet, cache the file write.
  cached_files_[file_path] = base::Time::Now();
  return true;
}

bool FileTagger::DeleteFile(const FilePath& file_path) {
  if (can_tag_files_)
    return file_util::Delete(FilePath(file_path), false);
  // If file access is not allowed yet, cache the delete operation.
  return cached_files_.erase(file_path) == 1;
}

bool FileTagger::SetupTraceFileNotifier() {
  notifier_.Init(TraceFileChangeHandler, this);
  if (notifier_.AddWatch(trace_dir_.value().c_str(), IN_DELETE) < 0)
    return false;
  notifier_.Start();
  return true;
}

gboolean FileTagger::TraceFileChangeHandler(const char* name,
                                           int,               // watch handle
                                           unsigned int,      // mask
                                           gpointer data) {
  FileTagger* tagger = static_cast<FileTagger*>(data);
  LOG(INFO) << "Received file system change signal from file " << name;
  if (!strcmp(name, tagger->suspend_file_.BaseName().value().c_str()) ||
      !strcmp(name, tagger->resume_file_.BaseName().value().c_str()) ||
      !strcmp(name, tagger->low_battery_file_.BaseName().value().c_str())) {
    // Make sure that all three files have been deleted.
    if (!file_util::PathExists(tagger->suspend_file_) &&
        !file_util::PathExists(tagger->resume_file_) &&
        !file_util::PathExists(tagger->low_battery_file_)) {
      tagger->can_tag_files_ = true;
      LOG(INFO) << "Enabling file tagging, writing any cached files.";
      // If there are cached files from before this flag was set, write them
      // to the file system now.
      for (FileCache::const_iterator iter = tagger->cached_files_.begin();
           iter != tagger->cached_files_.end(); ++iter) {
        tagger->TouchFile((*iter).first);
        file_util::SetLastModifiedTime(FilePath((*iter).first), (*iter).second);
      }
      tagger->cached_files_.clear();
    }
  }
  return true;
}

}  // namespace power_manager