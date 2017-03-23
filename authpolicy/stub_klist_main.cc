// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of klist. Simply returns fixed responses to predefined
// input.

#include <string>

#include "authpolicy/samba_interface_internal.h"
#include "authpolicy/stub_common.h"

namespace authpolicy {
namespace {

const int kExitCodeTgtValid = 0;

const char kStubList[] = R"!!!(Ticket cache: FILE:/krb5cc
Default principal: TESTCOMP$@EXAMPLE.COM

Valid starting     Expires            Service principal
03/21/17 09:03:04  03/21/17 19:03:04  krbtgt/EXAMPLE.COM@EXAMPLE.COM
         renew until 03/22/17 09:03:04
03/21/17 09:03:04  03/21/17 19:03:04  ldap/server.example.com@EXAMPLE.COM
         renew until 03/22/17 09:03:04
03/21/17 09:03:05  03/21/17 19:03:04  cifs/server.example.com@EXAMPLE.COM
         renew until 03/22/17 09:03:04
)!!!";

int HandleCommandLine(const std::string& command_line) {
  // klist -s just returns 0 if the TGT is valid and 1 otherwise.
  if (internal::Contains(command_line, "-s"))
    return kExitCodeTgtValid;

  WriteOutput(kStubList, "");
  return kExitCodeOk;
}

}  // namespace
}  // namespace authpolicy

int main(int argc, const char* const* argv) {
  std::string command_line = authpolicy::GetCommandLine(argc, argv);
  return authpolicy::HandleCommandLine(command_line);
}
