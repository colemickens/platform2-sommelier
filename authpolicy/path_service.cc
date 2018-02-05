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
const char kConfig[] = "/config.dat";
const char kUserSmbConf[] = "/smb_user.conf";
const char kDeviceSmbConf[] = "/smb_device.conf";
const char kUserKrb5Conf[] = "/krb5_user.conf";
const char kDeviceKrb5Conf[] = "/krb5_device.conf";

// Credential caches.
const char kUserCredentialCache[] = "/krb5cc_user";
const char kDeviceCredentialCache[] = "/krb5cc_device";

// Machine keytab.
const char kMachineKeyTab[] = "/krb5_machine.keytab";

// Executables.
const char kKInitPath[] = "/usr/bin/kinit";
const char kKListPath[] = "/usr/bin/klist";
const char kNetPath[] = "/usr/bin/net";
const char kParserPath[] = "/usr/sbin/authpolicy_parser";
const char kSmbClientPath[] = "/usr/bin/smbclient";

// Seccomp filters.
const char kKInitSeccompFilterPath[] = "/usr/share/policy/kinit-seccomp.policy";
const char kKListSeccompFilterPath[] = "/usr/share/policy/klist-seccomp.policy";
const char kNetAdsSeccompFilterPath[] =
    "/usr/share/policy/net_ads-seccomp.policy";
const char kParserSeccompFilterPath[] =
    "/usr/share/policy/authpolicy_parser-seccomp.policy";
const char kSmbClientSeccompFilterPath[] =
    "/usr/share/policy/smbclient-seccomp.policy";

// Debug flags.
const char kDebugFlagsPath[] = "/etc/authpolicyd_flags";
// Flags default level.
const char kFlagsDefaultLevelPath[] = "/run/authpolicyd/flags_default_level";
// kinit trace logs.
const char kKrb5Trace[] = "/krb5_trace";

}  // namespace

PathService::PathService() : PathService(true) {}

PathService::PathService(bool initialize) {
  if (initialize)
    Initialize();
}

PathService::~PathService() {}

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

  Insert(Path::CONFIG_DAT, state_dir + kConfig);
  Insert(Path::USER_SMB_CONF, temp_dir + kUserSmbConf);
  Insert(Path::DEVICE_SMB_CONF, temp_dir + kDeviceSmbConf);
  Insert(Path::USER_KRB5_CONF, temp_dir + kUserKrb5Conf);
  Insert(Path::DEVICE_KRB5_CONF, temp_dir + kDeviceKrb5Conf);

  // Credential caches have to be in a place writable for authpolicyd-exec!
  const std::string& samba_dir = Get(Path::SAMBA_DIR);
  Insert(Path::USER_CREDENTIAL_CACHE, samba_dir + kUserCredentialCache);
  Insert(Path::DEVICE_CREDENTIAL_CACHE, samba_dir + kDeviceCredentialCache);

  Insert(Path::MACHINE_KT_STATE, state_dir + kMachineKeyTab);
  Insert(Path::MACHINE_KT_TEMP, samba_dir + kMachineKeyTab);

  Insert(Path::KINIT, kKInitPath);
  Insert(Path::KLIST, kKListPath);
  Insert(Path::NET, kNetPath);
  Insert(Path::PARSER, kParserPath);
  Insert(Path::SMBCLIENT, kSmbClientPath);

  Insert(Path::KINIT_SECCOMP, kKInitSeccompFilterPath);
  Insert(Path::KLIST_SECCOMP, kKListSeccompFilterPath);
  Insert(Path::NET_ADS_SECCOMP, kNetAdsSeccompFilterPath);
  Insert(Path::PARSER_SECCOMP, kParserSeccompFilterPath);
  Insert(Path::SMBCLIENT_SECCOMP, kSmbClientSeccompFilterPath);

  Insert(Path::DEBUG_FLAGS, kDebugFlagsPath);
  Insert(Path::FLAGS_DEFAULT_LEVEL, kFlagsDefaultLevelPath);
  // Trace has to be in a place writable for authpolicyd-exec!
  Insert(Path::KRB5_TRACE, samba_dir + kKrb5Trace);
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
