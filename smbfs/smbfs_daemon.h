// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_SMBFS_DAEMON_H_
#define SMBFS_SMBFS_DAEMON_H_

#include <fuse_lowlevel.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>
#include <mojo/core/embedder/scoped_ipc_support.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "smbfs/mojom/smbfs.mojom.h"

namespace smbfs {

class Filesystem;
class FuseSession;
struct Options;
struct SmbCredential;

class SmbFsDaemon : public brillo::DBusDaemon, public mojom::SmbFsBootstrap {
 public:
  SmbFsDaemon(fuse_chan* chan, const Options& options);
  ~SmbFsDaemon() override;

 protected:
  // brillo::Daemon overrides.
  int OnInit() override;
  int OnEventLoopStarted() override;

  // mojom::SmbFsBootstrap overrides.
  void MountShare(mojom::MountOptionsPtr options,
                  mojom::SmbFsDelegatePtr delegate,
                  const MountShareCallback& callback) override;

 private:
  // Starts the fuse session using the filesystem |fs|. Returns true if the
  // session is successfully started.
  bool StartFuseSession(std::unique_ptr<Filesystem> fs);

  // Set up libsmbclient configuration files.
  bool SetupSmbConf();

  // Returns the full path to the given kerberos configuration file.
  base::FilePath KerberosConfFilePath(const std::string& file_name);

  // Initialise Mojo IPC system.
  bool InitMojo();

  // Mojo connection error handler.
  void OnConnectionError();

  fuse_chan* chan_;
  const bool use_test_fs_;
  const std::string share_path_;
  const uid_t uid_;
  const gid_t gid_;
  const std::string mojo_id_;
  std::unique_ptr<FuseSession> session_;
  std::unique_ptr<Filesystem> fs_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  mojo::Binding<mojom::SmbFsBootstrap> bootstrap_binding_{this};
  std::unique_ptr<mojo::Binding<mojom::SmbFs>> smbfs_binding_;
  mojom::SmbFsDelegatePtr delegate_;

  std::unique_ptr<SmbCredential> credential_;

  DISALLOW_COPY_AND_ASSIGN(SmbFsDaemon);
};

}  // namespace smbfs

#endif  // SMBFS_SMBFS_DAEMON_H_
