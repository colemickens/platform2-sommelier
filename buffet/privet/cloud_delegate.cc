// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/cloud_delegate.h"

#include <map>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/values.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>

#include "buffet/buffet_config.h"
#include "buffet/commands/command_manager.h"
#include "buffet/dbus-proxies.h"
#include "buffet/device_registration_info.h"
#include "buffet/privet/constants.h"
#include "buffet/states/state_manager.h"

namespace privetd {

namespace {

using chromeos::ErrorPtr;
using chromeos::VariantDictionary;
using org::chromium::Buffet::ManagerProxy;
using org::chromium::Buffet::ObjectManagerProxy;

const int kMaxSetupRetries = 5;
const int kFirstRetryTimeoutSec = 1;

buffet::CommandInstance* ReturnNotFound(const std::string& command_id,
                                        chromeos::ErrorPtr* error) {
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::kDomain,
                               errors::kNotFound, "Command not found, ID='%s'",
                               command_id.c_str());
  return nullptr;
}

class CloudDelegateImpl : public CloudDelegate {
 public:
  CloudDelegateImpl(bool is_gcd_setup_enabled,
                    buffet::DeviceRegistrationInfo* device,
                    buffet::CommandManager* command_manager,
                    buffet::StateManager* state_manager)
      : is_gcd_setup_enabled_(is_gcd_setup_enabled),
        device_{device},
        command_manager_{command_manager},
        state_manager_{state_manager} {
    device_->AddOnConfigChangedCallback(base::Bind(
        &CloudDelegateImpl::OnConfigChanged, weak_factory_.GetWeakPtr()));
    device_->AddOnRegistrationChangedCallback(base::Bind(
        &CloudDelegateImpl::OnRegistrationChanged, weak_factory_.GetWeakPtr()));

    command_manager_->AddOnCommandDefChanged(base::Bind(
        &CloudDelegateImpl::OnCommandDefChanged, weak_factory_.GetWeakPtr()));
    command_manager_->AddOnCommandAddedCallback(base::Bind(
        &CloudDelegateImpl::OnCommandAdded, weak_factory_.GetWeakPtr()));
    command_manager_->AddOnCommandAddedCallback(base::Bind(
        &CloudDelegateImpl::OnCommandRemoved, weak_factory_.GetWeakPtr()));

    state_manager_->AddOnChangedCallback(base::Bind(
        &CloudDelegateImpl::OnStateChanged, weak_factory_.GetWeakPtr()));
  }

  ~CloudDelegateImpl() override = default;

  bool GetModelId(std::string* id, chromeos::ErrorPtr* error) const override {
    if (device_->GetConfig().model_id().size() != 5) {
      chromeos::Error::AddToPrintf(
          error, FROM_HERE, errors::kDomain, errors::kInvalidState,
          "Model ID is invalid: %s", device_->GetConfig().model_id().c_str());
      return false;
    }
    *id = device_->GetConfig().model_id();
    return true;
  }

  bool GetName(std::string* name, chromeos::ErrorPtr* error) const override {
    *name = device_->GetConfig().name();
    return true;
  }

  std::string GetDescription() const override {
    return device_->GetConfig().description();
  }

  std::string GetLocation() const override {
    return device_->GetConfig().location();
  }

  void UpdateDeviceInfo(const std::string& name,
                        const std::string& description,
                        const std::string& location,
                        const base::Closure& success_callback,
                        const ErrorCallback& error_callback) override {
    chromeos::ErrorPtr error;
    if (!device_->UpdateDeviceInfo(name, description, location, &error))
      return error_callback.Run(error.get());
    success_callback.Run();
  }

  std::string GetOemName() const override {
    return device_->GetConfig().oem_name();
  }

  std::string GetModelName() const override {
    return device_->GetConfig().model_name();
  }

  std::set<std::string> GetServices() const override {
    std::set<std::string> result;
    for (base::DictionaryValue::Iterator it{command_defs_}; !it.IsAtEnd();
         it.Advance()) {
      result.emplace(it.key());
    }
    return result;
  }

  AuthScope GetAnonymousMaxScope() const override {
    AuthScope scope;
    if (StringToAuthScope(device_->GetConfig().local_anonymous_access_role(),
                          &scope)) {
      return scope;
    }
    return AuthScope::kNone;
  }

  const ConnectionState& GetConnectionState() const override {
    return connection_state_;
  }

  const SetupState& GetSetupState() const override { return setup_state_; }

  bool Setup(const std::string& ticket_id,
             const std::string& user,
             chromeos::ErrorPtr* error) override {
    if (!is_gcd_setup_enabled_) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                             errors::kSetupUnavailable,
                             "GCD setup unavailible");
      return false;
    }
    if (setup_state_.IsStatusEqual(SetupState::kInProgress)) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                             errors::kDeviceBusy, "Setup in progress");
      return false;
    }
    VLOG(1) << "GCD Setup started. ticket_id: " << ticket_id
            << ", user:" << user;
    setup_state_ = SetupState(SetupState::kInProgress);
    setup_weak_factory_.InvalidateWeakPtrs();
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&CloudDelegateImpl::CallManagerRegisterDevice,
                              setup_weak_factory_.GetWeakPtr(), ticket_id, 0),
        base::TimeDelta::FromSeconds(kSetupDelaySeconds));
    // Return true because we tried setup.
    return true;
  }

  std::string GetCloudId() const override {
    return device_->GetConfig().device_id();
  }

  const base::DictionaryValue& GetState() const override { return state_; }

  const base::DictionaryValue& GetCommandDef() const override {
    return command_defs_;
  }

  void AddCommand(const base::DictionaryValue& command,
                  const UserInfo& user_info,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override {
    CHECK(user_info.scope() != AuthScope::kNone);
    CHECK_NE(user_info.user_id(), 0u);

    chromeos::ErrorPtr error;
    buffet::UserRole role;
    if (!FromString(AuthScopeToString(user_info.scope()), &role, &error))
      return error_callback.Run(error.get());

    std::string id;
    if (!command_manager_->AddCommand(command, role, &id, &error))
      return error_callback.Run(error.get());

    CHECK(command_owners_.emplace(id, user_info.user_id()).second);
    success_callback.Run(*command_manager_->FindCommand(id)->ToJson());
  }

  void GetCommand(const std::string& id,
                  const UserInfo& user_info,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override {
    CHECK(user_info.scope() != AuthScope::kNone);
    chromeos::ErrorPtr error;
    auto command = GetCommandInternal(id, user_info, &error);
    if (!command)
      return error_callback.Run(error.get());
    success_callback.Run(*command->ToJson());
  }

  void CancelCommand(const std::string& id,
                     const UserInfo& user_info,
                     const SuccessCallback& success_callback,
                     const ErrorCallback& error_callback) override {
    CHECK(user_info.scope() != AuthScope::kNone);
    chromeos::ErrorPtr error;
    auto command = GetCommandInternal(id, user_info, &error);
    if (!command)
      return error_callback.Run(error.get());

    command->Cancel();
    success_callback.Run(*command->ToJson());
  }

  void ListCommands(const UserInfo& user_info,
                    const SuccessCallback& success_callback,
                    const ErrorCallback& error_callback) override {
    CHECK(user_info.scope() != AuthScope::kNone);

    base::ListValue list_value;

    for (const auto& it : command_owners_) {
      if (CanAccessCommand(it.second, user_info, nullptr)) {
        list_value.Append(
            command_manager_->FindCommand(it.first)->ToJson().release());
      }
    }

    base::DictionaryValue commands_json;
    commands_json.Set("commands", list_value.DeepCopy());

    success_callback.Run(commands_json);
  }

 private:
  void OnCommandAdded(buffet::CommandInstance* command) {
    // Set to 0 for any new unknown command.
    command_owners_.emplace(command->GetID(), 0);
  }

  void OnCommandRemoved(buffet::CommandInstance* command) {
    CHECK(command_owners_.erase(command->GetID()));
  }

  void OnConfigChanged(const buffet::BuffetConfig&) {
    NotifyOnDeviceInfoChanged();
  }

  void OnRegistrationChanged(buffet::RegistrationStatus status) {
    if (status == buffet::RegistrationStatus::kUnconfigured) {
      connection_state_ = ConnectionState{ConnectionState::kUnconfigured};
    } else if (status == buffet::RegistrationStatus::kConnecting) {
      // TODO(vitalybuka): Find conditions for kOffline.
      connection_state_ = ConnectionState{ConnectionState::kConnecting};
    } else if (status == buffet::RegistrationStatus::kConnected) {
      connection_state_ = ConnectionState{ConnectionState::kOnline};
    } else {
      chromeos::ErrorPtr error;
      chromeos::Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                                   errors::kInvalidState,
                                   "Unexpected buffet status: %s",
                                   buffet::StatusToString(status).c_str());
      connection_state_ = ConnectionState{std::move(error)};
    }
    NotifyOnDeviceInfoChanged();
  }

  void OnStateChanged() {
    state_.Clear();
    auto state = state_manager_->GetStateValuesAsJson(nullptr);
    CHECK(state);
    state_.MergeDictionary(state.get());
    NotifyOnStateChanged();
  }

  void OnCommandDefChanged() {
    command_defs_.Clear();
    auto commands = command_manager_->GetCommandDictionary().GetCommandsAsJson(
        [](const buffet::CommandDefinition* def) {
          return def->GetVisibility().local;
        },
        true, nullptr);
    CHECK(commands);
    command_defs_.MergeDictionary(commands.get());
    NotifyOnCommandDefsChanged();
  }

  void RetryRegister(const std::string& ticket_id,
                     int retries,
                     chromeos::Error* error) {
    if (retries >= kMaxSetupRetries) {
      chromeos::ErrorPtr new_error{error ? error->Clone() : nullptr};
      chromeos::Error::AddTo(&new_error, FROM_HERE, errors::kDomain,
                             errors::kInvalidState,
                             "Failed to register device");
      setup_state_ = SetupState{std::move(new_error)};
      return;
    }
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudDelegateImpl::CallManagerRegisterDevice,
                   setup_weak_factory_.GetWeakPtr(), ticket_id, retries + 1),
        base::TimeDelta::FromSeconds(kFirstRetryTimeoutSec << retries));
  }

  void OnRegisterSuccess(const std::string& device_id) {
    VLOG(1) << "Device registered: " << device_id;
    setup_state_ = SetupState(SetupState::kSuccess);
  }

  void CallManagerRegisterDevice(const std::string& ticket_id, int retries) {
    chromeos::ErrorPtr error;
    if (device_->RegisterDevice(ticket_id, &error).empty())
      RetryRegister(ticket_id, retries, error.get());
  }

  buffet::CommandInstance* GetCommandInternal(const std::string& command_id,
                                              const UserInfo& user_info,
                                              chromeos::ErrorPtr* error) const {
    if (user_info.scope() != AuthScope::kOwner) {
      auto it = command_owners_.find(command_id);
      if (it == command_owners_.end())
        return ReturnNotFound(command_id, error);
      if (CanAccessCommand(it->second, user_info, error))
        return nullptr;
    }

    auto command = command_manager_->FindCommand(command_id);
    if (!command)
      return ReturnNotFound(command_id, error);

    return command;
  }

  bool CanAccessCommand(uint64_t owner_id,
                        const UserInfo& user_info,
                        chromeos::ErrorPtr* error) const {
    CHECK(user_info.scope() != AuthScope::kNone);
    CHECK_NE(user_info.user_id(), 0u);

    if (user_info.scope() == AuthScope::kOwner ||
        owner_id == user_info.user_id()) {
      return true;
    }

    chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                           errors::kAccessDenied,
                           "Need to be owner of the command.");
    return false;
  }

  bool is_gcd_setup_enabled_{false};

  buffet::DeviceRegistrationInfo* device_{nullptr};
  buffet::CommandManager* command_manager_{nullptr};
  buffet::StateManager* state_manager_{nullptr};

  // Primary state of GCD.
  ConnectionState connection_state_{ConnectionState::kDisabled};

  // State of the current or last setup.
  SetupState setup_state_{SetupState::kNone};

  // Current device state.
  base::DictionaryValue state_;

  // Current commands definitions.
  base::DictionaryValue command_defs_;

  // Map of command IDs to user IDs.
  std::map<std::string, uint64_t> command_owners_;

  // |setup_weak_factory_| tracks the lifetime of callbacks used in connection
  // with a particular invocation of Setup().
  base::WeakPtrFactory<CloudDelegateImpl> setup_weak_factory_{this};
  // |weak_factory_| tracks the lifetime of |this|.
  base::WeakPtrFactory<CloudDelegateImpl> weak_factory_{this};
};

}  // namespace

CloudDelegate::CloudDelegate() {
}

CloudDelegate::~CloudDelegate() {
}

// static
std::unique_ptr<CloudDelegate> CloudDelegate::CreateDefault(
    bool is_gcd_setup_enabled,
    buffet::DeviceRegistrationInfo* device,
    buffet::CommandManager* command_manager,
    buffet::StateManager* state_manager) {
  return std::unique_ptr<CloudDelegateImpl>{new CloudDelegateImpl{
      is_gcd_setup_enabled, device, command_manager, state_manager}};
}

void CloudDelegate::NotifyOnDeviceInfoChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceInfoChanged());
}

void CloudDelegate::NotifyOnCommandDefsChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnCommandDefsChanged());
}

void CloudDelegate::NotifyOnStateChanged() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStateChanged());
}

}  // namespace privetd
