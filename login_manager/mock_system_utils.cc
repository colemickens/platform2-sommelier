// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_system_utils.h"

#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>

#include "login_manager/scoped_dbus_pending_call.h"

namespace login_manager {

MockSystemUtils::MockSystemUtils() {
}

MockSystemUtils::~MockSystemUtils() {
  EXPECT_TRUE(fake_calls_.empty()) << "CallAsyncMethodOnChromium expected"
                                   << " to be called " << fake_calls_.size()
                                   << " more times.";
}

bool MockSystemUtils::GetUniqueFilenameInWriteOnlyTempDir(
    FilePath* temp_file_path) {
  if (!tmpdir_.IsValid())
    EXPECT_TRUE(tmpdir_.CreateUniqueTempDir());
  *temp_file_path = tmpdir_.path().AppendASCII(unique_file_name_);
  return true;
}

void MockSystemUtils::SetUniqueFilename(const std::string& name) {
  unique_file_name_ = name;
}

scoped_ptr<ScopedDBusPendingCall> MockSystemUtils::CallAsyncMethodOnChromium(
    const char* method_name) {
  if (fake_calls_.empty()) {
    ADD_FAILURE() << "CallAsyncMethodOnChromium called too many times!";
    return scoped_ptr<ScopedDBusPendingCall>(NULL);
  }
  ScopedDBusPendingCall* to_return = fake_calls_[0];
  fake_calls_.weak_erase(fake_calls_.begin());
  return scoped_ptr<ScopedDBusPendingCall>(to_return);
}

void MockSystemUtils::EnqueueFakePendingCall(
    scoped_ptr<ScopedDBusPendingCall> fake_call) {
  fake_calls_.push_back(fake_call.release());
}

}  // namespace login_manager
