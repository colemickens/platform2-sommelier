// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_KERBEROS_ARTIFACT_CLIENT_H_
#define SMBFS_KERBEROS_ARTIFACT_CLIENT_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>

#include "smbfs/kerberos_artifact_client_interface.h"

namespace smbfs {

// KerberosArtifactClient is used to communicate with the
// org.chromium.AuthPolicy service.
class KerberosArtifactClient : public KerberosArtifactClientInterface {
 public:
  explicit KerberosArtifactClient(scoped_refptr<dbus::Bus> bus);

  // KerberosArtifactClientInterface overrides.
  void GetUserKerberosFiles(const std::string& object_guid,
                            GetUserKerberosFilesCallback callback) override;
  void ConnectToKerberosFilesChangedSignal(
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override;

 private:
  void HandleGetUserKeberosFiles(GetUserKerberosFilesCallback callback,
                                 dbus::Response* response);

  dbus::ObjectProxy* auth_policy_object_proxy_ = nullptr;
  base::WeakPtrFactory<KerberosArtifactClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KerberosArtifactClient);
};

}  // namespace smbfs

#endif  // SMBFS_KERBEROS_ARTIFACT_CLIENT_H_
