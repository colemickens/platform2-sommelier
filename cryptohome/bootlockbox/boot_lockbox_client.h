// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_CLIENT_H_
#define CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_CLIENT_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

namespace base {
class FilePath;
}  // namespace base

namespace org {
namespace chromium {
class BootLockboxInterfaceProxy;
}  // namespace chromium
}  // namespace org

namespace cryptohome {

// A class that that manages the communication with BootLockbox.
class BootLockboxClient {
 public:
  // Creates BootLockboxClient. The factory should be called on the same thread
  // that will call ~BootLockboxClient();
  static std::unique_ptr<BootLockboxClient> CreateBootLockboxClient();

  ~BootLockboxClient();

  // Signs |digest| using BootLockbox key. The signature is stored in
  // |signature|.
  bool Sign(const std::string& digest, std::string* signature);

  // Verifies |digest| against |signature|. Returns true if signature
  // verification successed. Returns false if signature is invalid or the
  // operation failed.
  bool Verify(const std::string& digest, const std::string& signature);

  // Locks BootLockboxClient. Signing operation won't be available afterwards.
  bool Finalize();

 private:
  BootLockboxClient(
      std::unique_ptr<org::chromium::BootLockboxInterfaceProxy> bootlockbox,
      scoped_refptr<dbus::Bus> bus);

  std::unique_ptr<org::chromium::BootLockboxInterfaceProxy> bootlockbox_;
  scoped_refptr<dbus::Bus> bus_;

  DISALLOW_COPY_AND_ASSIGN(BootLockboxClient);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_BOOT_LOCKBOX_CLIENT_H_
