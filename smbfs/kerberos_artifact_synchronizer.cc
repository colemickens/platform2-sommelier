// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/kerberos_artifact_synchronizer.h"

#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <dbus/authpolicy/dbus-constants.h>
#include <dbus/message.h>

namespace smbfs {

KerberosArtifactSynchronizer::KerberosArtifactSynchronizer(
    const base::FilePath& krb5_conf_path,
    const base::FilePath& krb5_ccache_path,
    const std::string& object_guid,
    std::unique_ptr<KerberosArtifactClientInterface> client)
    : krb5_conf_path_(krb5_conf_path),
      krb5_ccache_path_(krb5_ccache_path),
      object_guid_(object_guid),
      client_(std::move(client)) {}

void KerberosArtifactSynchronizer::SetupKerberos(
    SetupKerberosCallback callback) {
  DCHECK(!setup_called_);
  setup_called_ = true;

  GetFiles(std::move(callback));
}

void KerberosArtifactSynchronizer::GetFiles(SetupKerberosCallback callback) {
  client_->GetUserKerberosFiles(
      object_guid_,
      base::BindOnce(&KerberosArtifactSynchronizer::OnGetFilesResponse,
                     base::Unretained(this), std::move(callback)));
}

void KerberosArtifactSynchronizer::OnGetFilesResponse(
    SetupKerberosCallback callback,
    authpolicy::ErrorType error,
    const authpolicy::KerberosFiles& kerberos_files) {
  if (error != authpolicy::ERROR_NONE) {
    LOG(ERROR) << "KerberosArtifactSynchronizer failed to get Kerberos files";
    if (callback) {
      std::move(callback).Run(false /* setup_success */);
    }
    return;
  }

  WriteFiles(kerberos_files, std::move(callback));
}

void KerberosArtifactSynchronizer::WriteFiles(
    const authpolicy::KerberosFiles& kerberos_files,
    SetupKerberosCallback callback) {
  bool success = kerberos_files.has_krb5cc() && kerberos_files.has_krb5conf() &&
                 WriteFile(krb5_conf_path_, kerberos_files.krb5conf()) &&
                 WriteFile(krb5_ccache_path_, kerberos_files.krb5cc());

  if (!callback) {
    // If no callback was passed, this process was initiated by a
    // KerberosFilesChanged signal. No need to re-connect to the signal.
    LOG_IF(ERROR, !success) << "KerberosArtifactSynchronizer: failed to write "
                            << "updated Keberos Files";
    return;
  }

  if (!success) {
    // Failed to write the Kerberos files so return error to caller.
    LOG(ERROR) << "KerberosArtifactSynchronizer: failed to write initial "
                  "Keberos Files";
    DCHECK(callback);
    std::move(callback).Run(false /* setup_success */);
    return;
  }

  ConnectToKerberosFilesChangedSignal(std::move(callback));
}

void KerberosArtifactSynchronizer::ConnectToKerberosFilesChangedSignal(
    SetupKerberosCallback callback) {
  // TODO(crbug.com/993857): Switch to BindOnce when libchrome is updated.
  client_->ConnectToKerberosFilesChangedSignal(
      base::Bind(&KerberosArtifactSynchronizer::OnKerberosFilesChanged,
                 base::Unretained(this)),
      base::Bind(
          &KerberosArtifactSynchronizer::OnKerberosFilesChangedSignalConnected,
          base::Unretained(this), base::Passed(&callback)));
}

void KerberosArtifactSynchronizer::OnKerberosFilesChanged(
    dbus::Signal* signal) {
  DCHECK(signal);
  DCHECK_EQ(signal->GetInterface(), authpolicy::kAuthPolicyInterface);
  DCHECK_EQ(signal->GetMember(), authpolicy::kUserKerberosFilesChangedSignal);

  GetFiles({});
}

void KerberosArtifactSynchronizer::OnKerberosFilesChangedSignalConnected(
    SetupKerberosCallback callback,
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK_EQ(interface_name, authpolicy::kAuthPolicyInterface);
  DCHECK_EQ(signal_name, authpolicy::kUserKerberosFilesChangedSignal);
  DCHECK(success);

  std::move(callback).Run(true /* setup_success */);
}

bool KerberosArtifactSynchronizer::WriteFile(const base::FilePath& path,
                                             const std::string& blob) {
  if (base::WriteFile(path, blob.c_str(), blob.size()) != blob.size()) {
    LOG(ERROR) << "Failed to write file " << path.value();
    return false;
  }
  return true;
}

}  // namespace smbfs
