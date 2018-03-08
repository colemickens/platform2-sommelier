// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_SELINUX_VIOLATION_COLLECTOR_H_
#define CRASH_REPORTER_SELINUX_VIOLATION_COLLECTOR_H_

#include <map>
#include <string>

#include <base/files/file_util.h>
#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "crash-reporter/crash_collector.h"

// SELinux violation collector.
class SELinuxViolationCollector : public CrashCollector {
 public:
  SELinuxViolationCollector();

  ~SELinuxViolationCollector() override;

  // Collects warning.
  bool Collect();

 protected:
  void set_violation_report_path_for_testing(const base::FilePath& file_path) {
    violation_report_path_ = file_path;
  }

 private:
  friend class SELinuxViolationCollectorTest;
  FRIEND_TEST(SELinuxViolationCollectorTest, CollectOK);

  base::FilePath violation_report_path_;
  bool LoadSELinuxViolation(std::string* content,
                            std::string* signature,
                            std::map<std::string, std::string>* extra_metadata);

  DISALLOW_COPY_AND_ASSIGN(SELinuxViolationCollector);
};

#endif  // CRASH_REPORTER_SELINUX_VIOLATION_COLLECTOR_H_
