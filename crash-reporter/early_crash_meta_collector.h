// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_EARLY_CRASH_META_COLLECTOR_H_
#define CRASH_REPORTER_EARLY_CRASH_META_COLLECTOR_H_

#include <base/files/file_path.h>

#include "crash-reporter/crash_collector.h"

// Early user collector is more of a meta-collector: we use this to persist
// collected crashes into the encrypted stateful partition.
class EarlyCrashMetaCollector : public CrashCollector {
 public:
  EarlyCrashMetaCollector();
  ~EarlyCrashMetaCollector() override = default;

  void Initialize(IsFeedbackAllowedFunction is_feedback_allowed_function,
                  bool preserve_across_clobber);

  // Collect early crashes collected into /run/crash_reporter/crash
  bool Collect();

 private:
  bool early_;
  base::FilePath source_directory_;
  friend class EarlyCrashMetaCollectorTest;

  DISALLOW_COPY_AND_ASSIGN(EarlyCrashMetaCollector);
};

#endif  // CRASH_REPORTER_EARLY_CRASH_META_COLLECTOR_H_
