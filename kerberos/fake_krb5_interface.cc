// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/fake_krb5_interface.h"

#include <base/files/file_path.h>
#include <krb5.h>

namespace kerberos {

FakeKrb5Interface::FakeKrb5Interface() = default;

FakeKrb5Interface::~FakeKrb5Interface() = default;

ErrorType FakeKrb5Interface::AcquireTgt(const std::string& principal_name,
                                        const std::string& password,
                                        const base::FilePath& krb5cc_path,
                                        const base::FilePath& krb5conf_path) {
  return ERROR_NONE;
}

ErrorType FakeKrb5Interface::RenewTgt(const std::string& principal_name,
                                      const base::FilePath& krb5cc_path,
                                      const base::FilePath& krb5conf_path) {
  return ERROR_NONE;
}

ErrorType FakeKrb5Interface::GetTgtStatus(const base::FilePath& krb5cc_path,
                                          TgtStatus* status) {
  return ERROR_NONE;
}

}  // namespace kerberos
