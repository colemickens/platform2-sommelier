// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/path_service.h"

#include <utility>

#include <base/logging.h>

namespace authpolicy {
namespace {

// Base directories.
const char kAuthPolicyTempDir[] = "/tmp/authpolicyd";
const char kAuthPolicyStateDir[] = "/var/lib/authpolicyd";

// Relative Samba directories.
const char kSambaDir[] = "/samba";
const char kLockDir[] = "/lock";
const char kCacheDir[] = "/cache";
const char kStateDir[] = "/state";
const char kPrivateDir[] = "/private";
const char kGpoCacheDir[] = "/gpo_cache";

// Configuration files.
const char kConfigPath[] = "/config.dat";
const char kKrb5ConfPath[] = "/krb5.conf";
const char kSmbConfPath[] = "/smb.conf";

// Machine keytab.
const char kMachineKeyTabPath[] = "/krb5_machine.keytab";
// Debug flags.
const char kDebugFlagsPath[] = "/etc/authpolicyd_flags";
// kinit trace logs.
const char kKrb5TracePath[] = "/krb5_trace";

// Executable paths.
const char kKInitPath[] = "/usr/bin/kinit";
const char kNetPath[] = "/usr/bin/net";
const char kParserPath[] = "/usr/sbin/authpolicy_parser";
const char kSmbClientPath[] = "/usr/bin/smbclient";

// Seccomp filters.
const char kKInitSeccompFilter[] =
    "/usr/share/policy/kinit-seccomp.policy";
const char kNetAdsSeccompFilter[] =
    "/usr/share/policy/net_ads-seccomp.policy";
const char kParserSeccompFilter[] =
    "/usr/share/policy/authpolicy_parser-seccomp.policy";
const char kSmbClientSeccompFilter[] =
    "/usr/share/policy/smbclient-seccomp.policy";

}  // namespace

PathService::PathService() : PathService(true) {
}

PathService::PathService(bool initialize) {
  if (initialize)
    Initialize();
}

PathService::~PathService() {
}

void PathService::Initialize() {
  // Set paths. Note: Won't override paths that are already set by a more
  // derived version of this method.
  Insert(Path::TEMP_DIR, kAuthPolicyTempDir);
  Insert(Path::STATE_DIR, kAuthPolicyStateDir);

  const std::string& temp_dir = Get(Path::TEMP_DIR);
  const std::string& state_dir = Get(Path::STATE_DIR);
  Insert(Path::SAMBA_DIR, temp_dir + kSambaDir);
  Insert(Path::SAMBA_LOCK_DIR, temp_dir + kSambaDir + kLockDir);
  Insert(Path::SAMBA_CACHE_DIR, temp_dir + kSambaDir + kCacheDir);
  Insert(Path::SAMBA_STATE_DIR, temp_dir + kSambaDir + kStateDir);
  Insert(Path::SAMBA_PRIVATE_DIR, temp_dir + kSambaDir + kPrivateDir);
  Insert(Path::GPO_LOCAL_DIR, temp_dir + kSambaDir + kCacheDir + kGpoCacheDir);

  Insert(Path::CONFIG_DAT, state_dir + kConfigPath);
  Insert(Path::KRB5_CONF, temp_dir + kKrb5ConfPath);
  Insert(Path::SMB_CONF, temp_dir + kSmbConfPath);

  const std::string& samba_dir = Get(Path::SAMBA_DIR);
  Insert(Path::MACHINE_KT_STATE, state_dir + kMachineKeyTabPath);
  Insert(Path::MACHINE_KT_TEMP, samba_dir + kMachineKeyTabPath);

  Insert(Path::KINIT, kKInitPath);
  Insert(Path::NET, kNetPath);
  Insert(Path::PARSER, kParserPath);
  Insert(Path::SMBCLIENT, kSmbClientPath);

  Insert(Path::KINIT_SECCOMP, kKInitSeccompFilter);
  Insert(Path::NET_ADS_SECCOMP, kNetAdsSeccompFilter);
  Insert(Path::PARSER_SECCOMP, kParserSeccompFilter);
  Insert(Path::SMBCLIENT_SECCOMP, kSmbClientSeccompFilter);

  Insert(Path::DEBUG_FLAGS, kDebugFlagsPath);
  Insert(Path::KRB5_TRACE, temp_dir + kKrb5TracePath);
}

const std::string& PathService::Get(Path path_key) const {
  auto iter = paths_.find(path_key);
  DCHECK(iter != paths_.end());
  return iter->second;
}

void PathService::Insert(Path path_key, const std::string& path) {
  paths_.insert(std::make_pair(path_key, path));
}

}  // namespace authpolicy
