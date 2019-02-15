// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_KRB5_INTERFACE_H_
#define KERBEROS_KRB5_INTERFACE_H_

#include <string>

#include <base/compiler_specific.h>

#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace kerberos {

class Krb5Interface {
 public:
  Krb5Interface();
  ~Krb5Interface();

  // Gets a Kerberos ticket-granting-ticket for the given |principal_name|
  // (user@REALM.COM). |password| is the password for the Kerberos account.
  // |krb5cc_path| is the file path where the Kerberos credential cache (i.e.
  // the TGT) is written to. |krb5conf_path| is the path to a Kerberos
  // configuration file (krb5.conf).
  ErrorType AcquireTgt(const std::string& principal_name,
                       const std::string& password,
                       const std::string& krb5cc_path,
                       const std::string& krb5conf_path) WARN_UNUSED_RESULT;

  // Renews an existing Kerberos ticket-granting-ticketfor the given
  // |principal_name| (user@REALM.COM). |krb5cc_path| is the file path of the
  // Kerberos credential cache. |krb5conf_path| is the path to a Kerberos
  // configuration file (krb5.conf).
  ErrorType RenewTgt(const std::string& principal_name,
                     const std::string& krb5cc_path,
                     const std::string& krb5conf_path) WARN_UNUSED_RESULT;
};

}  // namespace kerberos

#endif  // KERBEROS_KRB5_INTERFACE_H_
