// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_FAKE_KRB5_INTERFACE_H_
#define KERBEROS_FAKE_KRB5_INTERFACE_H_

#include <base/compiler_specific.h>
#include <base/macros.h>

#include <string>

#include "kerberos/krb5_interface.h"
#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace base {
class FilePath;
}

namespace kerberos {

class FakeKrb5Interface : public Krb5Interface {
 public:
  FakeKrb5Interface();
  ~FakeKrb5Interface() override;

  // Krb5Interface:
  ErrorType AcquireTgt(const std::string& principal_name,
                       const std::string& password,
                       const base::FilePath& krb5cc_path,
                       const base::FilePath& krb5conf_path) override;

  // Krb5Interface:
  ErrorType RenewTgt(const std::string& principal_name,
                     const base::FilePath& krb5cc_path,
                     const base::FilePath& krb5conf_path) override;

  // Krb5Interface:
  ErrorType GetTgtStatus(const base::FilePath& krb5cc_path,
                         TgtStatus* status) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeKrb5Interface);
};

}  // namespace kerberos

#endif  // KERBEROS_FAKE_KRB5_INTERFACE_H_
