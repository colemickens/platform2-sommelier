// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <base/command_line.h>
#include <base/logging.h>

int main(int argc, char** argv) {
  ::g_type_init();

  CommandLine::Init(argc, argv);

  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  LOG(INFO) << "initializing gmock and gtest";
  ::testing::InitGoogleMock(&argc, argv);

  LOG(INFO) << "running unit tests";
  int test_result = RUN_ALL_TESTS();
  LOG(INFO) << "unittest return value: " << test_result;
  return test_result;
}
