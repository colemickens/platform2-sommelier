// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/kerberos_artifact_synchronizer.h"

#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/important_file_writer.h>
#include <base/files/file_util.h>
#include <dbus/authpolicy/dbus-constants.h>
#include <dbus/message.h>

namespace smbprovider {

KerberosArtifactSynchronizer::KerberosArtifactSynchronizer(
    const std::string& krb5_conf_path,
    const std::string& krb5_ccache_path,
    std::unique_ptr<KerberosArtifactClientInterface> client)
    : krb5_conf_path_(krb5_conf_path),
      krb5_ccache_path_(krb5_ccache_path),
      client_(std::move(client)) {}

void KerberosArtifactSynchronizer::SetupKerberos(
    const std::string& object_guid, SetupKerberosCallback callback) {
  if (!object_guid_.empty() && object_guid_ != object_guid) {
    // Only support one kerberos user at a time.
    LOG(ERROR) << "Kerberos is already set up for a differnet user";
    std::move(callback).Run(false /* success */);
    return;
  }

  if (object_guid_ == object_guid && is_kerberos_setup_) {
    LOG(WARNING) << "Kerberos already set up the user";
    std::move(callback).Run(true /* success */);
    return;
  }

  object_guid_ = object_guid;
  GetFiles(std::move(callback));
}

void KerberosArtifactSynchronizer::GetFiles(SetupKerberosCallback callback) {
  client_->GetUserKerberosFiles(
      object_guid_,
      base::Bind(&KerberosArtifactSynchronizer::OnGetFilesResponse,
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

  if (is_kerberos_setup_) {
    // Signal is already setup so return regardless of result but log failure.
    if (!success) {
      LOG(ERROR) << "KerberosArtifactSynchronizer: failed to write updated "
                    "Keberos Files";
    }
    if (callback) {
      // If a callback was passed, this is rare case where the browser restarted
      // and SetupKerberos() was called twice in quick succession. If
      // |is_kerberos_setup_| is true, then the first call to SetupKerberos()
      // succeeded, so treat this as a success.
      std::move(callback).Run(true /* success */);
    }
    return;
  }

  if (!success) {
    // Failed to write the Kerberos files so return error to caller.
    LOG(ERROR) << "KerberosArtifactSynchronizer: failed to write initial "
                  "Keberos Files";
    DCHECK(!is_kerberos_setup_);
    DCHECK(callback);
    std::move(callback).Run(false /* setup_success */);
    return;
  }

  // Sets is_kerberos_setup_ to true on successful signal connection.
  ConnectToKerberosFilesChangedSignal(std::move(callback));
}

void KerberosArtifactSynchronizer::ConnectToKerberosFilesChangedSignal(
    SetupKerberosCallback callback) {
  client_->ConnectToKerberosFilesChangedSignal(
      base::Bind(&KerberosArtifactSynchronizer::OnKerberosFilesChanged,
                 base::Unretained(this)),
      base::Bind(
          &KerberosArtifactSynchronizer::OnKerberosFilesChangedSignalConnected,
          base::Unretained(this), std::move(callback)));
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

  if (is_kerberos_setup_) {
    // If SetupKerberos() was called twice in quick succession (i.e. if the
    // browser restarted on login), it's possible for this change signal to be
    // registered twice. The change handler will be run twice, but this
    // shouldn't be an issue.
    LOG(ERROR) << "Duplicate Kerberos file change signals registered";
  }
  is_kerberos_setup_ = true;
  std::move(callback).Run(true /* setup_success */);
}

bool KerberosArtifactSynchronizer::WriteFile(const std::string& path,
                                             const std::string& blob) {
  const base::FilePath file_path(path);
  if (!base::ImportantFileWriter::WriteFileAtomically(file_path, blob)) {
    LOG(ERROR) << "Failed to write file " << file_path.value();
    return false;
  }
  return true;
}

}  // namespace smbprovider
