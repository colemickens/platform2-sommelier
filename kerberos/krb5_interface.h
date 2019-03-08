// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_KRB5_INTERFACE_H_
#define KERBEROS_KRB5_INTERFACE_H_

#include <base/compiler_specific.h>

#include <string>

#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace base {
class FilePath;
}

namespace kerberos {

class Krb5Interface {
 public:
  Krb5Interface();
  ~Krb5Interface();

  // Ticket-granting-ticket status, see GetTgtStatus().
  struct TgtStatus {
    // For how many seconds the ticket is still valid.
    int64_t validity_seconds = 0;

    // For how many seconds the ticket can be renewed.
    int64_t renewal_seconds = 0;
  };

  // Gets a Kerberos ticket-granting-ticket for the given |principal_name|
  // (user@REALM.COM). |password| is the password for the Kerberos account.
  // |krb5cc_path| is the file path where the Kerberos credential cache (i.e.
  // the TGT) is written to. |krb5conf_path| is the path to a Kerberos
  // configuration file (krb5.conf).
  ErrorType AcquireTgt(const std::string& principal_name,
                       const std::string& password,
                       const base::FilePath& krb5cc_path,
                       const base::FilePath& krb5conf_path) WARN_UNUSED_RESULT;

  // Renews an existing Kerberos ticket-granting-ticket for the given
  // |principal_name| (user@REALM.COM). |krb5cc_path| is the file path of the
  // Kerberos credential cache. |krb5conf_path| is the path to a Kerberos
  // configuration file (krb5.conf).
  ErrorType RenewTgt(const std::string& principal_name,
                     const base::FilePath& krb5cc_path,
                     const base::FilePath& krb5conf_path) WARN_UNUSED_RESULT;

  // Gets some stats about the ticket-granting-ticket in the credential cache
  // at |krb5cc_path|.
  ErrorType GetTgtStatus(const base::FilePath& krb5cc_path, TgtStatus* status);
};

}  // namespace kerberos

#endif  // KERBEROS_KRB5_INTERFACE_H_
