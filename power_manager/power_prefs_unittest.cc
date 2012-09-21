// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "power_manager/power_prefs.h"
#include "power_manager/signal_callback.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::Test;

namespace {

const int kNumPrefDirectories = 3;
const int64 kIntTestValue = 0xdeadbeef;
const double kDoubleTestValue = 0.1337;
const char kGarbageString[] = "This is garbage";

const char kIntTestFileName[] = "intfile";
const char kDoubleTestFileName[] = "doublefile";

// The test crashes after this many milliseconds if an expected preference
// change notification is never received.
const guint kPrefChangeTimeoutMs = 60 * 1000;

}  // namespace

namespace power_manager {

// Simple class that runs a GLib main loop until it sees a pref get changed.
// Tests should pass &OnPrefChangedThunk to PowerPrefs::StartPrefWatching(),
// update a pref file, and then call RunUntilPrefChanged().
class TestPrefObserver {
 public:
  TestPrefObserver() : loop_(g_main_loop_new(NULL, FALSE)), timeout_id_(0) {}
  ~TestPrefObserver() {
    if (timeout_id_)
      g_source_remove(timeout_id_);
    g_main_loop_unref(loop_);
    loop_ = NULL;
  }

  // Runs |loop_| until OnPrefChanged() is called, then quits the loop
  // and returns a string containing the name of the pref that was changed.
  std::string RunUntilPrefChanged() {
    timeout_id_ = g_timeout_add(kPrefChangeTimeoutMs, OnTimeoutThunk, this);
    g_main_loop_run(loop_);
    return pref_name_;
  }

  // Listens for preference change notifications.  Cancels |timeout_id_|, quits
  // the main loop, and updates |pref_name_|.
  SIGNAL_CALLBACK_3(TestPrefObserver,
                    gboolean,
                    OnPrefChanged,
                    const char*,
                    int,
                    unsigned int);

  // Triggers a crash.
  SIGNAL_CALLBACK_0(TestPrefObserver,
                    gboolean,
                    OnTimeout);

 private:
  GMainLoop* loop_;  // not owned

  // Name of the last pref that was changed.
  std::string pref_name_;

  // ID of the timeout that was installed to run OnTimeout().  0 if not present.
  guint timeout_id_;

  DISALLOW_COPY_AND_ASSIGN(TestPrefObserver);
};

gboolean TestPrefObserver::OnPrefChanged(const char* name,
                                         int handle,
                                         unsigned int mask) {
  // OnPrefChanged() can be called multiple times for a single update.  For
  // example, when a file is first written, we'll get notified both about it
  // being created and about it being modified.
  if (timeout_id_) {
    g_source_remove(timeout_id_);
    timeout_id_ = 0;
  }
  g_main_loop_quit(loop_);
  pref_name_ = name;
  return TRUE;
}

gboolean TestPrefObserver::OnTimeout() {
  timeout_id_ = 0;
  CHECK(false) << "Timeout exceeded; pref change notification not received";
  return FALSE;
}

class PowerPrefsTest : public Test {
 public:
  PowerPrefsTest() {}

  virtual void SetUp() {
    paths_.clear();
    // Create new temp directories.
    for (int i = 0; i < kNumPrefDirectories; ++i) {
      temp_dir_generators_[i].reset(new ScopedTempDir());
      ASSERT_TRUE(temp_dir_generators_[i]->CreateUniqueTempDir());
      EXPECT_TRUE(temp_dir_generators_[i]->IsValid());
      paths_.push_back(temp_dir_generators_[i]->path());
    }
  }

 protected:
  std::vector<FilePath> paths_;
  scoped_ptr<ScopedTempDir> temp_dir_generators_[kNumPrefDirectories];
};

// Test read/write with only one directory.
TEST_F(PowerPrefsTest, TestOneDirectory) {
  PowerPrefs prefs(paths_[0]);

  // Make sure the pref files don't already exist.
  EXPECT_FALSE(file_util::PathExists(paths_[0].Append(kIntTestFileName)));
  EXPECT_FALSE(file_util::PathExists(paths_[0].Append(kDoubleTestFileName)));

  // Write int and double values to pref files.
  EXPECT_TRUE(prefs.SetInt64(kIntTestFileName, kIntTestValue));
  EXPECT_TRUE(prefs.SetDouble(kDoubleTestFileName, kDoubleTestValue));

  // Make sure the files were only created in the first directory.
  for (int i = 0; i < kNumPrefDirectories; ++i) {
    if (i == 0) {
      EXPECT_TRUE(file_util::PathExists(paths_[i].Append(kIntTestFileName)));
      EXPECT_TRUE(file_util::PathExists(paths_[i].Append(kDoubleTestFileName)));
      continue;
    }
    EXPECT_FALSE(file_util::PathExists(paths_[i].Append(kIntTestFileName)));
    EXPECT_FALSE(file_util::PathExists(paths_[i].Append(kDoubleTestFileName)));
  }

  // Now read them back and make sure they have the right values.
  int64 int_value = -1;
  double double_value = -1;
  EXPECT_TRUE(prefs.GetInt64(kIntTestFileName, &int_value));
  EXPECT_TRUE(prefs.GetDouble(kDoubleTestFileName, &double_value));
  EXPECT_EQ(kIntTestValue, int_value);
  EXPECT_EQ(kDoubleTestValue, double_value);
}

// Test read/write with three directories.
TEST_F(PowerPrefsTest, TestThreeDirectories) {
  PowerPrefs prefs(paths_);

  // Make sure the files don't already exist.
  for (int i = 0; i < kNumPrefDirectories; ++i) {
    EXPECT_FALSE(file_util::PathExists(paths_[i].Append(kIntTestFileName)));
    EXPECT_FALSE(file_util::PathExists(paths_[i].Append(kDoubleTestFileName)));
  }

  // Write int and double values to pref files and make sure those files were
  // created in the first directory and not in the other two.
  EXPECT_TRUE(prefs.SetInt64(kIntTestFileName, kIntTestValue));
  EXPECT_TRUE(file_util::PathExists(paths_[0].Append(kIntTestFileName)));
  EXPECT_FALSE(file_util::PathExists(paths_[1].Append(kIntTestFileName)));
  EXPECT_FALSE(file_util::PathExists(paths_[2].Append(kIntTestFileName)));

  EXPECT_TRUE(prefs.SetDouble(kDoubleTestFileName, kDoubleTestValue));
  EXPECT_TRUE(file_util::PathExists(paths_[0].Append(kDoubleTestFileName)));
  EXPECT_FALSE(file_util::PathExists(paths_[1].Append(kDoubleTestFileName)));
  EXPECT_FALSE(file_util::PathExists(paths_[2].Append(kDoubleTestFileName)));

  // Now read them back and make sure they have the right values.
  int64 int_value = -1;
  double double_value = -1;
  EXPECT_TRUE(prefs.GetInt64(kIntTestFileName, &int_value));
  EXPECT_TRUE(prefs.GetDouble(kDoubleTestFileName, &double_value));
  EXPECT_EQ(kIntTestValue, int_value);
  EXPECT_EQ(kDoubleTestValue, double_value);
}

// Test read from three directories, checking for precedence of directories.
// Prefs in |paths_[i]| take precedence over the same prefs in |paths_[j]|, for
// i < j.
TEST_F(PowerPrefsTest, TestThreeDirectoriesStacked) {
  // Run cycles from 1 to |(1 << kNumPrefDirectories) - 1|.  Each cycle number's
  // bits will represent the paths to populate with pref files.  e.g...
  // cycle 2 = 010b  =>  write prefs to |paths_[1]|
  // cycle 5 = 101b  =>  write prefs to |paths_[0]| and |paths_[2]|
  // cycle 7 = 111b  =>  write prefs to all paths in |paths_|.
  // This will test all the valid combinations of which directories have pref
  // files.
  for (int cycle = 1; cycle < (1 << kNumPrefDirectories); ++cycle) {
    LOG(INFO) << "Testing stacked directories, cycle #" << cycle;
    SetUp();
    PowerPrefs prefs(paths_);

    // Write values to the pref directories as appropriate for this cycle.
    int i;
    for (i = 0; i < kNumPrefDirectories; ++i) {
      const FilePath& path = paths_[i];
      // Make sure the files didn't exist already.
      EXPECT_FALSE(file_util::PathExists(path.Append(kIntTestFileName)));
      EXPECT_FALSE(file_util::PathExists(path.Append(kDoubleTestFileName)));

      // Determine if this directory path's bit is set in the current cycle
      // number.
      if (!((cycle >> i) & 1))
        continue;

      // For path[i], write the default test values + i.
      // This way, each path's pref file will have a unique value.
      std::string int_string = base::Int64ToString(kIntTestValue + i);
      EXPECT_EQ(int_string.size(),
                file_util::WriteFile(path.Append(kIntTestFileName),
                                     int_string.data(),
                                     int_string.size()));
      EXPECT_TRUE(file_util::PathExists(path.Append(kIntTestFileName)));

      std::string double_string = base::DoubleToString(kDoubleTestValue + i);
      EXPECT_EQ(double_string.size(),
                file_util::WriteFile(path.Append(kDoubleTestFileName),
                                     double_string.data(),
                                     double_string.size()));
      EXPECT_TRUE(file_util::PathExists(path.Append(kDoubleTestFileName)));
    }

    // Read the pref files.
    int64 int_value = -1;
    double double_value = -1;
    EXPECT_TRUE(prefs.GetInt64(kIntTestFileName, &int_value));
    EXPECT_TRUE(prefs.GetDouble(kDoubleTestFileName, &double_value));

    // Make sure the earlier paths take precedence over later paths.
    bool is_first_valid_directory = true;
    int num_directories_checked = 0;
    for (i = 0; i < kNumPrefDirectories; ++i) {
      // If the current directory was not used this cycle, disregard it.
      if (!((cycle >> i) & 1))
        continue;
      if (is_first_valid_directory) {
        // First valid directory should match.
        EXPECT_EQ(kIntTestValue + i, int_value);
        EXPECT_EQ(kDoubleTestValue + i, double_value);
        is_first_valid_directory = false;
      } else {
        EXPECT_NE(kIntTestValue + i, int_value);
        EXPECT_NE(kDoubleTestValue + i, double_value);
      }
      ++num_directories_checked;
    }
    EXPECT_GT(num_directories_checked, 0);
  }
}

// Test read from three directories, with the higher precedence directories
// containing garbage.
TEST_F(PowerPrefsTest, TestThreeDirectoriesGarbage) {
  PowerPrefs prefs(paths_);

  for (int i = 0; i < kNumPrefDirectories; ++i) {
    const FilePath& path = paths_[i];
    // Make sure the files didn't exist already.
    EXPECT_FALSE(file_util::PathExists(path.Append(kIntTestFileName)));
    EXPECT_FALSE(file_util::PathExists(path.Append(kDoubleTestFileName)));

    // Earlier directories contain garbage.
    // The last one contains valid values.
    std::string int_string;
    std::string double_string;
    if (i < kNumPrefDirectories - 1) {
      int_string = kGarbageString;
      double_string = kGarbageString;
    } else {
      int_string = base::Int64ToString(kIntTestValue);
      double_string = base::DoubleToString(kDoubleTestValue);
    }
    EXPECT_EQ(int_string.size(),
              file_util::WriteFile(path.Append(kIntTestFileName),
                                   int_string.data(),
                                   int_string.size()));
    EXPECT_TRUE(file_util::PathExists(path.Append(kIntTestFileName)));
    EXPECT_EQ(double_string.size(),
              file_util::WriteFile(path.Append(kDoubleTestFileName),
                                   double_string.data(),
                                   double_string.size()));
    EXPECT_TRUE(file_util::PathExists(path.Append(kDoubleTestFileName)));
  }

  // Read the pref files and make sure the right value was read.
  int64 int_value = -1;
  double double_value = -1;
  EXPECT_TRUE(prefs.GetInt64(kIntTestFileName, &int_value));
  EXPECT_TRUE(prefs.GetDouble(kDoubleTestFileName, &double_value));
  EXPECT_EQ(kIntTestValue, int_value);
  EXPECT_EQ(kDoubleTestValue, double_value);
}

// Make sure that PowerPrefs correctly notifies about changes to pref files.
TEST_F(PowerPrefsTest, WatchPrefs) {
  const char kPrefName[] = "foo";
  const char kPrefValue[] = "1";
  const FilePath kFilePath = paths_[0].Append(kPrefName);

  // Create a pref file.
  PowerPrefs prefs(paths_);
  TestPrefObserver observer;
  prefs.StartPrefWatching(&TestPrefObserver::OnPrefChangedThunk, &observer);
  EXPECT_EQ(strlen(kPrefValue),
            file_util::WriteFile(kFilePath, kPrefValue, strlen(kPrefValue)));
  EXPECT_EQ(kPrefName, observer.RunUntilPrefChanged());

  // Write to the file again.
  EXPECT_EQ(strlen(kPrefValue),
            file_util::WriteFile(kFilePath, kPrefValue, strlen(kPrefValue)));
  EXPECT_EQ(kPrefName, observer.RunUntilPrefChanged());

  // Remove the file.
  EXPECT_TRUE(file_util::Delete(kFilePath, false));
  EXPECT_EQ(kPrefName, observer.RunUntilPrefChanged());
}

}  // namespace power_manager
