// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/dbus_adaptor.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/callback.h>
#include <chromeos/dbus/service_constants.h>
#include <google-lpa/lpa/core/lpa.h>

namespace {

const char kErrorDomain[] = "Hermes";

const char kErrorCodeUnknown[] = "Unknown";
const char kErrorCodeWrongState[] = "WrongState";
const char kErrorCodeInvalidIccid[] = "InvalidIccid";
const char kErrorCodeProfileNotEnabled[] = "ProfileNotEnabled";
const char kErrorCodeNeedConfirmationCode[] = "NeedConfirmationCode";
const char kErrorCodeInvalidActivationCode[] = "InvalidActivationCode";
const char kErrorCodeSendNotificationError[] = "SendNotificationError";

bool HandleLpaError(
    int error,
    const std::map<int, std::unique_ptr<brillo::Error>>* error_map,
    brillo::dbus_utils::DBusMethodResponseBase* response) {
  CHECK(error_map);
  if (error != lpa::core::Lpa::kNoError) {
    auto iter = error_map->find(error);
    if (iter != error_map->end()) {
      response->ReplyWithError(iter->second.get());
    } else {
      response->ReplyWithError(FROM_HERE, kErrorDomain, kErrorCodeUnknown,
                               "Unknown error");
    }
    return false;
  }
  return true;
}

// Function to return from a D-Bus hermes method that has no output parameters.
//
// Note the use of a shared_ptr rather than unique_ptr. The google-lpa API takes
// std::function parameters as callbacks. Since the standard states that
// std::functions must be CopyConstructible, bind states or lambdas that gain
// ownership of a unique_ptr may not be used as a std::function. As the
// org::chromium::HermesInterface interface is passed a unique_ptr, the options
// are either to maintain DBusMethodResponse lifetimes separately or to convert
// unique_ptrs to shared_ptrs.
void DefaultCallback(
    const std::map<int, std::unique_ptr<brillo::Error>>* error_map,
    std::shared_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    int error) {
  if (!HandleLpaError(error, error_map, response.get())) {
    return;
  }
  response->Return();
}

}  // namespace

namespace hermes {

using lpa::proto::ProfileInfo;
using lpa::proto::ProfileInfoList;

DBusAdaptor::DBusAdaptor(lpa::core::Lpa* lpa, Executor* executor)
    : org::chromium::HermesAdaptor(this), lpa_(lpa), executor_(executor) {
  // Create mapping from google-lpa error to brillo error.
  error_map_[lpa::core::Lpa::kWrongState] =
      brillo::Error::Create(FROM_HERE, kErrorDomain, kErrorCodeWrongState,
                            "Invalid state for requested method");
  error_map_[lpa::core::Lpa::kIccidNotFound] = brillo::Error::Create(
      FROM_HERE, kErrorDomain, kErrorCodeInvalidIccid, "Invalid iccid");
  error_map_[lpa::core::Lpa::kProfileNotEnabled] = brillo::Error::Create(
      FROM_HERE, kErrorDomain, kErrorCodeProfileNotEnabled,
      "Requested method needs an enabled profile");
  error_map_[lpa::core::Lpa::kNeedConfirmationCode] = brillo::Error::Create(
      FROM_HERE, kErrorDomain, kErrorCodeNeedConfirmationCode,
      "Need confirmation code");
  error_map_[lpa::core::Lpa::kInvalidActivationCode] = brillo::Error::Create(
      FROM_HERE, kErrorDomain, kErrorCodeInvalidActivationCode,
      "Invalid activation code");
  error_map_[lpa::core::Lpa::kFailedToSendNotifications] =
      brillo::Error::Create(FROM_HERE, kErrorDomain,
                            kErrorCodeSendNotificationError,
                            "Failed to send notifications");
}

void DBusAdaptor::InstallProfile(
    std::unique_ptr<DBusResponse<ProfileInfo>> response,
    const std::string& in_activation_code) {
  auto profile_cb =
    [response{std::shared_ptr<DBusResponse<ProfileInfo>>(std::move(response))},
     this](lpa::proto::ProfileInfo& profile, int error) {
      if (!HandleLpaError(error, &error_map_, response.get())) {
        return;
      }
      response->Return(profile);
    };
  if (in_activation_code.empty()) {
    lpa_->GetDefaultProfileFromSmdp("", executor_, std::move(profile_cb));
    return;
  }

  auto simple_cb =
    [response{std::shared_ptr<DBusResponse<ProfileInfo>>(std::move(response))},
     cb{std::move(profile_cb)}, in_activation_code, this](int error) {
      if (!HandleLpaError(error, &error_map_, response.get())) {
        return;
      }
      // TODO(crbug.com/963555) Return valid ProfileInfo
      lpa::proto::ProfileInfo empty;
      cb(empty, error);
    };
  lpa::core::Lpa::DownloadOptions options;
  options.enable_profile = false;
  options.allow_policy_rules = false;
  lpa_->DownloadProfile(in_activation_code, std::move(options), executor_,
                        std::move(simple_cb));
}

void DBusAdaptor::UninstallProfile(std::unique_ptr<DBusResponse<>> response,
                                   const std::string& in_iccid) {
  lpa_->DeleteProfile(
      in_iccid, executor_,
      std::bind(&DefaultCallback, &error_map_,
                std::shared_ptr<DBusResponse<>>(std::move(response)),
                std::placeholders::_1));
}

void DBusAdaptor::EnableProfile(std::unique_ptr<DBusResponse<>> response,
                                const std::string& in_iccid) {
  lpa_->EnableProfile(
      in_iccid, executor_,
      std::bind(&DefaultCallback, &error_map_,
                std::shared_ptr<DBusResponse<>>(std::move(response)),
                std::placeholders::_1));
}

void DBusAdaptor::DisableProfile(std::unique_ptr<DBusResponse<>> response,
                                 const std::string& in_iccid) {
  lpa_->DisableProfile(
      in_iccid, executor_,
      std::bind(&DefaultCallback, &error_map_,
                std::shared_ptr<DBusResponse<>>(std::move(response)),
                std::placeholders::_1));
}

void DBusAdaptor::SetProfileNickname(std::unique_ptr<DBusResponse<>> response,
                                     const std::string& in_iccid,
                                     const std::string& in_nickname) {
  lpa_->SetProfileNickname(
      in_iccid, in_nickname, executor_,
      std::bind(&DefaultCallback, &error_map_,
                std::shared_ptr<DBusResponse<>>(std::move(response)),
                std::placeholders::_1));
}

void DBusAdaptor::GetInstalledProfiles(
    std::unique_ptr<DBusResponse<ProfileInfoList>> response) {
  auto cb = [response{std::shared_ptr<DBusResponse<ProfileInfoList>>(
                 std::move(response))},
             this](std::vector<lpa::proto::ProfileInfo>& profiles, int error) {
    if (!HandleLpaError(error, &error_map_, response.get())) {
      return;
    }

    ProfileInfoList profile_list;
    VLOG(2) << "Installed profiles:";
    for (auto profile : profiles) {
      *profile_list.add_profile_info() = profile;

      VLOG(2) << "";
      VLOG(2) << "    ICCID: " << profile.iccid();
      VLOG_IF(2, profile.has_activation_code()) << "    Activation code: "
                                                << profile.activation_code();
      VLOG_IF(2, profile.has_profile_name()) << "    Profile name: "
                                             << profile.profile_name();
    }
    response->Return(profile_list);
  };
  lpa_->GetInstalledProfiles(executor_, std::move(cb));
}

void DBusAdaptor::SetTestMode(bool /*in_is_test_mode*/) {
  // TODO(akhouderchah) This is a no-op until the Lpa interface allows for
  // switching certificate directory without recreating the Lpa object.
  NOTIMPLEMENTED();
}

}  // namespace hermes
