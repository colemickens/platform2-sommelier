// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_CONSTANTS_H_
#define SMBPROVIDER_CONSTANTS_H_

#include <fcntl.h>  /* for file flags */
#include <stddef.h> /* for size_t */
#include <sys/stat.h> /* for mode_t */

namespace smbprovider {

// Buffer size used for reading a directory.
constexpr size_t kBufferSize = 1024 * 32;

// Default flags for created files.
constexpr int kCreateFileFlags = O_CREAT | O_WRONLY | O_TRUNC | O_EXCL;

// Default permissions for created entries.
constexpr mode_t kCreateEntryPermissions = 0755;

// SMB Url scheme
constexpr char kSmbUrlScheme[] = "smb://";

// Entries returned by smbc_getdents() that we ignore.
extern const char kEntryParent[];
extern const char kEntrySelf[];

// $HOME environment variable.
extern const char kHomeEnvironmentVariable[];
// Set as $HOME in order for libsmbclient to read smb.conf file.
extern const char kSmbProviderHome[];

// Location and file name for smb configuration file.
extern const char kSmbConfLocation[];
extern const char kSmbConfFile[];

// Data for smb config file.
extern const char kSmbConfData[];

// Environment variables for Kerberos.
extern const char kKrb5ConfigEnvironmentVariable[];
extern const char kKrb5CCNameEnvironmentVariable[];
extern const char kKrb5TraceEnvironmentVariable[];

// Location and file name for krb5 configuration file.
extern const char kKrb5ConfLocation[];
extern const char kKrb5ConfFile[];

// Location and file name for credential cache file.
extern const char kCCacheLocation[];
extern const char kCCacheFile[];

// Location and file name for the Kerberos trace file.
extern const char kKrbTraceLocation[];
extern const char kKrbTraceFile[];

}  // namespace smbprovider

#endif  // SMBPROVIDER_CONSTANTS_H_
