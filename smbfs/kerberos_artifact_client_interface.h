// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_KERBEROS_ARTIFACT_CLIENT_INTERFACE_H_
#define SMBFS_KERBEROS_ARTIFACT_CLIENT_INTERFACE_H_

#include <string>

#include <authpolicy/proto_bindings/active_directory_info.pb.h>
#include <base/callback.h>
#include <dbus/object_proxy.h>

namespace smbfs {

class KerberosArtifactClientInterface {
 public:
  using GetUserKerberosFilesCallback =
      base::OnceCallback<void(authpolicy::ErrorType error,
                              const authpolicy::KerberosFiles& kerberos_files)>;

  virtual ~KerberosArtifactClientInterface() = default;

  // Calls GetUserKerberosFiles. If authpolicyd has Kerberos files for the user
  // specified by |object_guid| it sends them in response: credential cache and
  // krb5 config files.
  virtual void GetUserKerberosFiles(const std::string& object_guid,
                                    GetUserKerberosFilesCallback callback) = 0;

  // Connects callbacks to OnKerberosFilesChanged D-Bus signal sent by
  // authpolicyd.
  virtual void ConnectToKerberosFilesChangedSignal(
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;

 protected:
  KerberosArtifactClientInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(KerberosArtifactClientInterface);
};

}  // namespace smbfs

#endif  // SMBFS_KERBEROS_ARTIFACT_CLIENT_INTERFACE_H_
