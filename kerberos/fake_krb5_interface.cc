// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/fake_krb5_interface.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <krb5.h>

namespace kerberos {
namespace {

// Fake Kerberos credential cache.
constexpr char kFakeKrb5cc[] = "I'm authenticated, trust me!";

}  // namespace

FakeKrb5Interface::FakeKrb5Interface() = default;

FakeKrb5Interface::~FakeKrb5Interface() = default;

ErrorType FakeKrb5Interface::AcquireTgt(const std::string& principal_name,
                                        const std::string& password,
                                        const base::FilePath& krb5cc_path,
                                        const base::FilePath& krb5conf_path) {
  if (password.empty())
    return ERROR_BAD_PASSWORD;

  if (!expected_password_.empty() && password != expected_password_)
    return ERROR_BAD_PASSWORD;

  const int size = strlen(kFakeKrb5cc);
  CHECK(base::WriteFile(krb5cc_path, kFakeKrb5cc, strlen(kFakeKrb5cc)) == size);
  return acquire_tgt_error_;
}

ErrorType FakeKrb5Interface::RenewTgt(const std::string& principal_name,
                                      const base::FilePath& krb5cc_path,
                                      const base::FilePath& krb5conf_path) {
  return renew_tgt_error_;
}

ErrorType FakeKrb5Interface::GetTgtStatus(const base::FilePath& krb5cc_path,
                                          TgtStatus* status) {
  *status = tgt_status_;
  return get_tgt_status_error_;
}

}  // namespace kerberos
