// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_KERBEROS_ADAPTOR_H_
#define KERBEROS_KERBEROS_ADAPTOR_H_

#include <memory>
#include <vector>

#include <base/macros.h>
#include <brillo/dbus/async_event_sequencer.h>

#include "kerberos/account_manager.h"
#include "kerberos/org.chromium.Kerberos.h"

namespace brillo {
namespace dbus_utils {
class DBusObject;
}
}  // namespace brillo

namespace kerberos {

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

  // org::chromium::KerberosInterface: (see org.chromium.Kerberos.xml).
  std::vector<uint8_t> AddAccount(
      const std::vector<uint8_t>& request_blob) override;

  std::vector<uint8_t> RemoveAccount(
      const std::vector<uint8_t>& request_blob) override;

  std::vector<uint8_t> AcquireKerberosTgt(
      const std::vector<uint8_t>& request_blob,
      const base::ScopedFD& password_fd) override;

  std::vector<uint8_t> GetKerberosFiles(
      const std::vector<uint8_t>& request_blob) override;

 private:
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  AccountManager manager_;

  DISALLOW_COPY_AND_ASSIGN(KerberosAdaptor);
};

}  // namespace kerberos

#endif  // KERBEROS_KERBEROS_ADAPTOR_H_
