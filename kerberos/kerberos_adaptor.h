// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_KERBEROS_ADAPTOR_H_
#define KERBEROS_KERBEROS_ADAPTOR_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/optional.h>
#include <brillo/dbus/async_event_sequencer.h>

#include "kerberos/org.chromium.Kerberos.h"

namespace brillo {
namespace dbus_utils {
class DBusObject;
}
}  // namespace brillo

namespace kerberos {

class AccountManager;

// Implementation of the Kerberos D-Bus interface.
class KerberosAdaptor : public org::chromium::KerberosAdaptor,
                        public org::chromium::KerberosInterface {
 public:
  explicit KerberosAdaptor(
      std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object);
  ~KerberosAdaptor();

  // Registers the D-Bus object and interfaces.
  void RegisterAsync(
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  using ByteArray = std::vector<uint8_t>;

  // org::chromium::KerberosInterface: (see org.chromium.Kerberos.xml).
  ByteArray AddAccount(const ByteArray& request_blob) override;
  ByteArray ListAccounts(const ByteArray& request_blob) override;
  ByteArray RemoveAccount(const ByteArray& request_blob) override;
  ByteArray SetConfig(const ByteArray& request_blob) override;
  ByteArray AcquireKerberosTgt(const ByteArray& request_blob,
                               const base::ScopedFD& password_fd) override;
  ByteArray GetKerberosFiles(const ByteArray& request_blob) override;

  AccountManager* GetAccountManagerForTesting() { return manager_.get(); }

  // Overrides the directory where data is stored. Must be called before
  // RegisterAsync().
  void set_storage_dir_for_testing(const base::FilePath& dir) {
    storage_dir_for_testing_ = dir;
  }

 private:
  // Gets triggered by when the Kerberos credential cache or the configuration
  // file changes of the given principal. Triggers the KerberosFilesChanged
  // signal.
  void OnKerberosFilesChanged(const std::string& principal_name);

  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  // Manages Kerberos accounts and tickets.
  std::unique_ptr<AccountManager> manager_;

  // If set, overrides the directory where data is stored.
  base::Optional<base::FilePath> storage_dir_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(KerberosAdaptor);
};

}  // namespace kerberos

#endif  // KERBEROS_KERBEROS_ADAPTOR_H_
