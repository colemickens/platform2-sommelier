// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of Samba net. Does not talk to server, but simply returns
// fixed responses to predefined input.

#include <string>

#include <base/logging.h>

#include "authpolicy/stub_common.h"

namespace {

const char kStubKeytab[] = "Stub keytab file";

const char kStubInfo[] = R"__INFO__(LDAP server: 111.222.33.44
LDAP server name: dcname.realm.com
Realm: REALM.COM
Bind Path: dc=REALM,dc=COM
LDAP port: 389
Server time: Fri, 03 Feb 2017 05:24:05 PST
KDC server: 111.222.33.44
Server time offset: -91
Last machine account password change:
Wed, 31 Dec 1969 16:00:00 PST)__INFO__";

const char kStubGpoList[] = R"__GPO__(---------------------
name:   Local Policy
displayname:  Local Policy
version:  0 (0x00000000)
version_user:  0 (0x0000)
version_machine: 0 (0x0000)
filesyspath:  (null)
dspath:  (null)
options:  0 GPFLAGS_ALL_ENABLED
link:   (null)
link_type:  5 machine_extensions: (null)
user_extensions: (null))__GPO__";

}  // namespace

namespace authpolicy {

int HandleCommandLine(const std::string command_line) {
  if (StartsWithCaseSensitive(command_line, "ads workgroup")) {
    // Return a fake workgroup.
    WriteOutput("Workgroup: WOKGROUP", "");
    return kExitCodeOk;
  }

  if (StartsWithCaseSensitive(command_line, "ads join")) {
    // Write a stub keytab file.
    std::string keytab_path = GetKeytabFilePath();
    CHECK(!keytab_path.empty());
    // Note: base::WriteFile triggers a seccomp failure, so do it old-school.
    FILE* kt_file = fopen(keytab_path.c_str(), "w");
    CHECK(kt_file);
    CHECK_EQ(1U, fwrite(kStubKeytab, strlen(kStubKeytab), 1, kt_file));
    CHECK_EQ(0, fclose(kt_file));
    return kExitCodeOk;
  }

  if (StartsWithCaseSensitive(command_line, "ads info")) {
    WriteOutput(kStubInfo, "");
    return kExitCodeOk;
  }

  if (StartsWithCaseSensitive(command_line, "ads gpo list")) {
    WriteOutput("", kStubGpoList);
    return kExitCodeOk;
  }

  LOG(ERROR) << "UNHANDLED COMMAND LINE " << command_line;
  return kExitCodeError;
}

}  // namespace authpolicy

int main(int argc, char* argv[]) {
  const std::string command_line = authpolicy::GetCommandLine(argc, argv);
  return authpolicy::HandleCommandLine(command_line);
}
