// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_EC_COLLECTOR_H_
#define CRASH_REPORTER_EC_COLLECTOR_H_

#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "crash-reporter/crash_collector.h"

/* From ec/include/panic.h */
/* Byte [2] of panicinfo contains flags */
#define PANIC_DATA_FLAGS_BYTE 2
/* Set to 1 if already returned via host command */
#define PANIC_DATA_FLAG_OLD_HOSTCMD (1 << 2)

// EC crash collector.
class ECCollector : public CrashCollector {
 public:
  ECCollector();

  ~ECCollector() override;

  // Collect any preserved EC panicinfo. Returns true if there was
  // a dump (even if there were problems storing the dump), false otherwise.
  bool Collect();

 private:
  friend class ECCollectorTest;

  base::FilePath debugfs_path_;

  DISALLOW_COPY_AND_ASSIGN(ECCollector);
};

#endif  // CRASH_REPORTER_EC_COLLECTOR_H_
