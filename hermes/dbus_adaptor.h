// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_DBUS_ADAPTOR_H_
#define HERMES_DBUS_ADAPTOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <brillo/errors/error.h>
#include <google-lpa/lpa/core/lpa.h>
#include <google-lpa/lpa/data/proto/profile_info_list.pb.h>

#include "hermes/dbus_bindings/org.chromium.Hermes.h"
#include "hermes/executor.h"

namespace hermes {

class DBusAdaptor : public org::chromium::HermesInterface,
                    public org::chromium::HermesAdaptor {
 public:
  using ByteArray = std::vector<uint8_t>;

  template <typename... T>
  using DBusResponse = brillo::dbus_utils::DBusMethodResponse<T...>;

  DBusAdaptor(lpa::core::Lpa* lpa, Executor* executor);

  // org::chromium::HermesInterface overrides.
  // Install a profile. An empty activation code will cause the default profile
  // to be installed.
  void InstallProfile(
    std::unique_ptr<DBusResponse<lpa::proto::ProfileInfo>> response,
    const std::string& in_activation_code) override;
  void UninstallProfile(std::unique_ptr<DBusResponse<>> response,
                        const std::string& in_iccid) override;
  void EnableProfile(std::unique_ptr<DBusResponse<>> response,
                     const std::string& in_iccid) override;
  void DisableProfile(std::unique_ptr<DBusResponse<>> response,
                      const std::string& in_iccid) override;
  void SetProfileNickname(std::unique_ptr<DBusResponse<>> response,
                          const std::string& in_iccid,
                          const std::string& in_nickname) override;
  // Get a list of the ICCIDs of all profiles installed on the eUICC.
  void GetInstalledProfiles(
    std::unique_ptr<DBusResponse<lpa::proto::ProfileInfoList>> resp) override;
  // Set/unset test mode. Normally, only production profiles may be
  // downloaded. In test mode, only test profiles may be downloaded.
  void SetTestMode(bool in_is_test_mode) override;

 private:
  lpa::core::Lpa* lpa_;
  Executor* executor_;

  std::map<int, std::unique_ptr<brillo::Error>> error_map_;

  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace hermes

#endif  // HERMES_DBUS_ADAPTOR_H_
