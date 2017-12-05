// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/constants.h"

namespace smbprovider {

const char kEntryParent[] = "..";
const char kEntrySelf[] = ".";
const char kHomeEnvironmentVariable[] = "HOME";
const char kSmbProviderHome[] = "/tmp/smbproviderd";
const char kSmbConfLocation[] = "/.smb";
const char kSmbConfFile[] = "/smb.conf";
const char kSmbConfData[] =
    "[global]\n"
    "\tclient min protocol = SMB2\n"
    "\tclient max protocol = SMB3\n";

}  // namespace smbprovider
