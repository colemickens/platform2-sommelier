// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/smbfs_daemon.h"

#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <brillo/message_loops/message_loop.h>
#include <brillo/daemons/dbus_daemon.h>

#include "smbfs/fuse_session.h"
#include "smbfs/smb_filesystem.h"
#include "smbfs/smbfs.h"
#include "smbfs/test_filesystem.h"

namespace smbfs {
namespace {

constexpr char kSmbConfDir[] = ".smb";
constexpr char kSmbConfFile[] = "smb.conf";
constexpr char kKerberosConfDir[] = ".krb";
constexpr char kKrb5ConfFile[] = "krb5.conf";
constexpr char kCCacheFile[] = "ccache";
constexpr char kKrbTraceFile[] = "krb_trace.txt";

constexpr char kSmbConfData[] = R"(
[global]
  client min protocol = SMB2
  client max protocol = SMB3
  security = user
)";

bool CreateDirectoryAndLog(const base::FilePath& path) {
  CHECK(path.IsAbsolute());
  base::File::Error error;
  bool success = base::CreateDirectoryAndGetError(path, &error);
  LOG_IF(ERROR, !success) << "Failed to create directory " << path.value()
                          << ": " << base::File::ErrorToString(error);
  return success;
}

}  // namespace

SmbFsDaemon::SmbFsDaemon(fuse_chan* chan, const Options& options)
    : chan_(chan),
      use_test_fs_(options.use_test),
      share_path_(options.share_path),
      uid_(options.uid ? options.uid : getuid()),
      gid_(options.gid ? options.gid : getgid()),
      mojo_id_(options.mojo_id ? options.mojo_id : "") {
  DCHECK(chan_);
}

SmbFsDaemon::~SmbFsDaemon() = default;

int SmbFsDaemon::OnInit() {
  int ret = brillo::DBusDaemon::OnInit();
  if (ret != EX_OK) {
    return ret;
  }

  if (!SetupSmbConf()) {
    return EX_SOFTWARE;
  }

  if (!share_path_.empty()) {
    auto fs = std::make_unique<SmbFilesystem>(share_path_, uid_, gid_);
    SmbFilesystem::ConnectError error = fs->EnsureConnected();
    if (error != SmbFilesystem::ConnectError::kOk) {
      LOG(ERROR) << "Unable to connect to SMB filesystem: " << error;
      return EX_SOFTWARE;
    }
    fs_ = std::move(fs);
  }

  return EX_OK;
}

int SmbFsDaemon::OnEventLoopStarted() {
  int ret = brillo::DBusDaemon::OnEventLoopStarted();
  if (ret != EX_OK) {
    return ret;
  }

  std::unique_ptr<Filesystem> fs;
  if (use_test_fs_) {
    fs = std::make_unique<TestFilesystem>(uid_, gid_);
  } else if (fs_) {
    fs = std::move(fs_);
  } else {
    NOTREACHED();
  }

  session_ = std::make_unique<FuseSession>(std::move(fs), chan_);
  chan_ = nullptr;

  if (!session_->Start(base::BindOnce(&Daemon::Quit, base::Unretained(this)))) {
    return EX_SOFTWARE;
  }

  return EX_OK;
}

base::FilePath SmbFsDaemon::KerberosConfFilePath(const std::string& file_name) {
  DCHECK(temp_dir_.IsValid());
  return temp_dir_.GetPath().Append(kKerberosConfDir).Append(file_name);
}

bool SmbFsDaemon::SetupSmbConf() {
  // Create a temporary "home" directory where configuration files used by
  // libsmbclient will be placed.
  CHECK(temp_dir_.CreateUniqueTempDir());
  PCHECK(setenv("HOME", temp_dir_.GetPath().value().c_str(),
                1 /* overwrite */) == 0);
  PCHECK(setenv("KRB5_CONFIG",
                KerberosConfFilePath(kKrb5ConfFile).value().c_str(),
                1 /* overwrite */) == 0);
  PCHECK(setenv("KRB5CCNAME", KerberosConfFilePath(kCCacheFile).value().c_str(),
                1 /* overwrite */) == 0);
  PCHECK(setenv("KRB5_TRACE",
                KerberosConfFilePath(kKrbTraceFile).value().c_str(),
                1 /* overwrite */) == 0);
  LOG(INFO) << "Storing SMB configuration files in: "
            << temp_dir_.GetPath().value();

  bool success =
      CreateDirectoryAndLog(temp_dir_.GetPath().Append(kSmbConfDir)) &&
      CreateDirectoryAndLog(temp_dir_.GetPath().Append(kKerberosConfDir));
  if (!success) {
    return false;
  }

  // TODO(amistry): Replace with smbc_setOptionProtocols() when Samba is
  // updated.
  return base::WriteFile(
             temp_dir_.GetPath().Append(kSmbConfDir).Append(kSmbConfFile),
             kSmbConfData, sizeof(kSmbConfData)) == sizeof(kSmbConfData);
}

}  // namespace smbfs
