// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome.h"

#include <openssl/sha.h>

#include <limits>

#include <base/file_util.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>

namespace chromeos {
namespace cryptohome {
namespace home {

static const char *kUserHomePrefix = "/home/user/";
static const char *kRootHomePrefix = "/home/root/";
static const char *kSystemSaltPath = "/home/.shadow/salt";

static std::string* salt = NULL;

static bool EnsureSystemSaltIsLoaded() {
  if (salt && !salt->empty())
    return true;
  FilePath salt_path(kSystemSaltPath);
  int64 file_size;
  if (!file_util::GetFileSize(salt_path, &file_size)) {
    PLOG(ERROR) << "Could not get size of system salt: " <<  kSystemSaltPath;
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << "System salt too large: " << file_size;
    return false;
  }
  std::vector<char> buf;
  buf.resize(file_size);
  unsigned int data_read = file_util::ReadFile(salt_path,
                                               &buf.front(),
                                               file_size);
  if (data_read != file_size) {
    PLOG(ERROR) << "Could not read entire file: " << data_read << " != "
                << file_size;
    return false;
  }

  salt = new std::string();
  salt->assign(&buf.front(), file_size);
  return true;
}

std::string SanitizeUserName(const std::string& username) {
  unsigned char binmd[SHA_DIGEST_LENGTH];
  const char* name = username.c_str();
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, salt->data(), salt->size());
  SHA1_Update(&ctx, username.data(), username.size());
  SHA1_Final(binmd, &ctx);
  return base::HexEncode(binmd, sizeof(binmd));
}

FilePath GetUserPath(const std::string& username) {
  if (!EnsureSystemSaltIsLoaded())
    return FilePath("");
  return FilePath(base::StringPrintf("%s%s", kUserHomePrefix,
                                     SanitizeUserName(username).c_str()));
}

FilePath GetRootPath(const std::string& username) {
  if (!EnsureSystemSaltIsLoaded())
    return FilePath("");
  return FilePath(base::StringPrintf("%s%s", kRootHomePrefix,
                                     SanitizeUserName(username).c_str()));
}

FilePath GetDaemonPath(const std::string& username, const std::string& daemon) {
  if (!EnsureSystemSaltIsLoaded())
    return FilePath("");
  return GetRootPath(username).Append(daemon);
}

} // namespace home
} // namespace cryptohome
} // namespace chromeos
