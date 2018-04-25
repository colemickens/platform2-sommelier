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
  DCHECK(!is_kerberos_setup_);

  setup_result_callback_ = std::move(callback);
  object_guid_ = object_guid;

  GetFiles();
}

void KerberosArtifactSynchronizer::GetFiles() {
  client_->GetUserKerberosFiles(
      object_guid_,
      base::Bind(&KerberosArtifactSynchronizer::OnGetFilesResponse,
                 base::Unretained(this)));
}

void KerberosArtifactSynchronizer::OnGetFilesResponse(
    authpolicy::ErrorType error,
    const authpolicy::KerberosFiles& kerberos_files) {
  if (error != authpolicy::ERROR_NONE) {
    LOG(ERROR) << "KerberosArtifactSynchronizer failed to get Kerberos files";
    DCHECK(!is_kerberos_setup_);
    setup_result_callback_.Run(false /* setup_success */);
    return;
  }

  WriteFiles(kerberos_files);
}

void KerberosArtifactSynchronizer::WriteFiles(
    const authpolicy::KerberosFiles& kerberos_files) {
  bool success = kerberos_files.has_krb5cc() && kerberos_files.has_krb5conf() &&
                 WriteFile(krb5_conf_path_, kerberos_files.krb5conf()) &&
                 WriteFile(krb5_ccache_path_, kerberos_files.krb5cc());

  if (is_kerberos_setup_) {
    // Signal is already setup so return regardless of result but log failure.
    if (!success) {
      LOG(ERROR) << "KerberosArtifactSynchronizer: failed to write updated "
                    "Keberos Files";
    }
    return;
  }

  if (!success) {
    // Failed to write the Kerberos files so return error to caller.
    LOG(ERROR) << "KerberosArtifactSynchronizer: failed to write initial "
                  "Keberos Files";
    DCHECK(!is_kerberos_setup_);
    setup_result_callback_.Run(false /* setup_success */);
    return;
  }

  // Sets is_kerberos_setup_ to true on successful signal connection.
  ConnectToKerberosFilesChangedSignal();
}

void KerberosArtifactSynchronizer::ConnectToKerberosFilesChangedSignal() {
  client_->ConnectToKerberosFilesChangedSignal(
      base::Bind(&KerberosArtifactSynchronizer::OnKerberosFilesChanged,
                 base::Unretained(this)),
      base::Bind(
          &KerberosArtifactSynchronizer::OnKerberosFilesChangedSignalConnected,
          base::Unretained(this)));
}

void KerberosArtifactSynchronizer::OnKerberosFilesChanged(
    dbus::Signal* signal) {
  DCHECK(signal);
  DCHECK_EQ(signal->GetInterface(), authpolicy::kAuthPolicyInterface);
  DCHECK_EQ(signal->GetMember(), authpolicy::kUserKerberosFilesChangedSignal);

  GetFiles();
}

void KerberosArtifactSynchronizer::OnKerberosFilesChangedSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK_EQ(interface_name, authpolicy::kAuthPolicyInterface);
  DCHECK_EQ(signal_name, authpolicy::kUserKerberosFilesChangedSignal);
  DCHECK(success);

  DCHECK(!is_kerberos_setup_);
  is_kerberos_setup_ = true;
  setup_result_callback_.Run(true /* setup_success */);
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
