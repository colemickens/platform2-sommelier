// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/manager.h"

#include <map>
#include <set>
#include <string>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/http_transport.h>
#include <chromeos/key_value_store.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

namespace {

const char kPairingSessionIdKey[] = "sessionId";
const char kPairingModeKey[] = "mode";
const char kPairingCodeKey[] = "code";

const char kErrorDomain[] = "buffet";
const char kNotImplemented[] = "notImplemented";

std::string StatusToString(weave::RegistrationStatus status) {
  switch (status) {
    case weave::RegistrationStatus::kUnconfigured:
      return "unconfigured";
    case weave::RegistrationStatus::kConnecting:
      return "connecting";
    case weave::RegistrationStatus::kConnected:
      return "connected";
    case weave::RegistrationStatus::kInvalidCredentials:
      return "invalid_credentials";
  }
  CHECK(0) << "Unknown status";
  return "unknown";
}

}  // anonymous namespace

Manager::Manager(const base::WeakPtr<ExportedObjectManager>& object_manager)
    : dbus_object_(object_manager.get(),
                   object_manager->GetBus(),
                   org::chromium::Buffet::ManagerAdaptor::GetObjectPath()) {
}

Manager::~Manager() {
}

void Manager::Start(const weave::Device::Options& options,
                    AsyncEventSequencer* sequencer) {
  device_ = weave::Device::Create();
  device_->Start(options, &dbus_object_, sequencer);

  device_->GetCommands()->AddOnCommandDefChanged(base::Bind(
      &Manager::OnCommandDefsChanged, weak_ptr_factory_.GetWeakPtr()));

  device_->GetState()->AddOnChangedCallback(
      base::Bind(&Manager::OnStateChanged, weak_ptr_factory_.GetWeakPtr()));

  device_->GetConfig()->AddOnChangedCallback(
      base::Bind(&Manager::OnConfigChanged, weak_ptr_factory_.GetWeakPtr()));

  device_->GetCloud()->AddOnRegistrationChangedCallback(base::Bind(
      &Manager::OnRegistrationChanged, weak_ptr_factory_.GetWeakPtr()));

  if (device_->GetPrivet()) {
    device_->GetPrivet()->AddOnWifiSetupChangedCallback(base::Bind(
        &Manager::UpdateWiFiBootstrapState, weak_ptr_factory_.GetWeakPtr()));

    device_->GetPrivet()->AddOnPairingChangedCallbacks(
        base::Bind(&Manager::OnPairingStart, weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&Manager::OnPairingEnd, weak_ptr_factory_.GetWeakPtr()));
  } else {
    UpdateWiFiBootstrapState(weave::WifiSetupState::kDisabled);
  }

  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
}

void Manager::Stop() {
  device_.reset();
}

// TODO(vitalybuka): Remove, it's just duplicate of property.
void Manager::CheckDeviceRegistered(
    DBusMethodResponsePtr<std::string> response) {
  LOG(INFO) << "Received call to Manager.CheckDeviceRegistered()";
  response->Return(dbus_adaptor_.GetDeviceId());
}

// TODO(vitalybuka): Remove or rename to leave for testing.
void Manager::GetDeviceInfo(DBusMethodResponsePtr<std::string> response) {
  LOG(INFO) << "Received call to Manager.GetDeviceInfo()";
  std::shared_ptr<DBusMethodResponse<std::string>> shared_response =
      std::move(response);

  device_->GetCloud()->GetDeviceInfo(
      base::Bind(&Manager::OnGetDeviceInfoSuccess,
                 weak_ptr_factory_.GetWeakPtr(), shared_response),
      base::Bind(&Manager::OnGetDeviceInfoError, weak_ptr_factory_.GetWeakPtr(),
                 shared_response));
}

void Manager::OnGetDeviceInfoSuccess(
    const std::shared_ptr<DBusMethodResponse<std::string>>& response,
    const base::DictionaryValue& device_info) {
  std::string device_info_str;
  base::JSONWriter::WriteWithOptions(
      device_info, base::JSONWriter::OPTIONS_PRETTY_PRINT, &device_info_str);
  response->Return(device_info_str);
}

void Manager::OnGetDeviceInfoError(
    const std::shared_ptr<DBusMethodResponse<std::string>>& response,
    const chromeos::Error* error) {
  response->ReplyWithError(error);
}

void Manager::RegisterDevice(DBusMethodResponsePtr<std::string> response,
                             const std::string& ticket_id) {
  LOG(INFO) << "Received call to Manager.RegisterDevice()";

  chromeos::ErrorPtr error;
  std::string device_id =
      device_->GetCloud()->RegisterDevice(ticket_id, &error);
  if (!device_id.empty()) {
    response->Return(device_id);
    return;
  }
  CHECK(error);
  response->ReplyWithError(error.get());
}

void Manager::UpdateState(DBusMethodResponsePtr<> response,
                          const chromeos::VariantDictionary& property_set) {
  chromeos::ErrorPtr error;
  if (!device_->GetState()->SetProperties(property_set, &error))
    response->ReplyWithError(error.get());
  else
    response->Return();
}

bool Manager::GetState(chromeos::ErrorPtr* error, std::string* state) {
  auto json = device_->GetState()->GetStateValuesAsJson(error);
  if (!json)
    return false;
  base::JSONWriter::WriteWithOptions(
      *json, base::JSONWriter::OPTIONS_PRETTY_PRINT, state);
  return true;
}

void Manager::AddCommand(DBusMethodResponsePtr<std::string> response,
                         const std::string& json_command,
                         const std::string& in_user_role) {
  std::string error_message;
  std::unique_ptr<base::Value> value(
      base::JSONReader::ReadAndReturnError(json_command, base::JSON_PARSE_RFC,
                                           nullptr, &error_message)
          .release());
  const base::DictionaryValue* command{nullptr};
  if (!value || !value->GetAsDictionary(&command)) {
    return response->ReplyWithError(FROM_HERE, chromeos::errors::json::kDomain,
                                    chromeos::errors::json::kParseError,
                                    error_message);
  }

  chromeos::ErrorPtr error;
  weave::UserRole role;
  if (!FromString(in_user_role, &role, &error))
    return response->ReplyWithError(error.get());

  std::string id;
  if (!device_->GetCommands()->AddCommand(*command, role, &id, &error))
    return response->ReplyWithError(error.get());

  response->Return(id);
}

void Manager::GetCommand(DBusMethodResponsePtr<std::string> response,
                         const std::string& id) {
  const weave::CommandInstance* command =
      device_->GetCommands()->FindCommand(id);
  if (!command) {
    response->ReplyWithError(FROM_HERE, kErrorDomain, "unknown_command",
                             "Can't find command with id: " + id);
    return;
  }
  std::string command_str;
  base::JSONWriter::WriteWithOptions(
      *command->ToJson(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &command_str);
  response->Return(command_str);
}

void Manager::SetCommandVisibility(DBusMethodResponsePtr<> response,
                                   const std::vector<std::string>& in_names,
                                   const std::string& in_visibility) {
  weave::CommandDefinition::Visibility visibility;
  chromeos::ErrorPtr error;
  if (!visibility.FromString(in_visibility, &error)) {
    response->ReplyWithError(error.get());
    return;
  }
  if (!device_->GetCommands()->SetCommandVisibility(in_names, visibility,
                                                    &error)) {
    response->ReplyWithError(error.get());
    return;
  }
  response->Return();
}

std::string Manager::TestMethod(const std::string& message) {
  LOG(INFO) << "Received call to test method: " << message;
  return message;
}

bool Manager::EnableWiFiBootstrapping(
    chromeos::ErrorPtr* error,
    const dbus::ObjectPath& in_listener_path,
    const chromeos::VariantDictionary& in_options) {
  chromeos::Error::AddTo(error, FROM_HERE, kErrorDomain, kNotImplemented,
                         "Manual WiFi bootstrapping is not implemented");
  return false;
}

bool Manager::DisableWiFiBootstrapping(chromeos::ErrorPtr* error) {
  chromeos::Error::AddTo(error, FROM_HERE, kErrorDomain, kNotImplemented,
                         "Manual WiFi bootstrapping is not implemented");
  return false;
}

bool Manager::EnableGCDBootstrapping(
    chromeos::ErrorPtr* error,
    const dbus::ObjectPath& in_listener_path,
    const chromeos::VariantDictionary& in_options) {
  chromeos::Error::AddTo(error, FROM_HERE, kErrorDomain, kNotImplemented,
                         "Manual GCD bootstrapping is not implemented");
  return false;
}

bool Manager::DisableGCDBootstrapping(chromeos::ErrorPtr* error) {
  chromeos::Error::AddTo(error, FROM_HERE, kErrorDomain, kNotImplemented,
                         "Manual GCD bootstrapping is not implemented");
  return false;
}

bool Manager::UpdateDeviceInfo(chromeos::ErrorPtr* error,
                               const std::string& name,
                               const std::string& description,
                               const std::string& location) {
  return device_->GetCloud()->UpdateDeviceInfo(name, description, location,
                                               error);
}

bool Manager::UpdateServiceConfig(chromeos::ErrorPtr* error,
                                  const std::string& client_id,
                                  const std::string& client_secret,
                                  const std::string& api_key,
                                  const std::string& oauth_url,
                                  const std::string& service_url) {
  return device_->GetCloud()->UpdateServiceConfig(
      client_id, client_secret, api_key, oauth_url, service_url, error);
}

void Manager::OnCommandDefsChanged() {
  // Limit only to commands that are visible to the local clients.
  auto commands =
      device_->GetCommands()->GetCommandDictionary().GetCommandsAsJson(
          [](const weave::CommandDefinition* def) {
            return def->GetVisibility().local;
          },
          true, nullptr);
  CHECK(commands);
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *commands, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  dbus_adaptor_.SetCommandDefs(json);
}

void Manager::OnStateChanged() {
  auto state = device_->GetState()->GetStateValuesAsJson(nullptr);
  CHECK(state);
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *state, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  dbus_adaptor_.SetState(json);
}

void Manager::OnRegistrationChanged(weave::RegistrationStatus status) {
  dbus_adaptor_.SetStatus(StatusToString(status));
}

void Manager::OnConfigChanged(const weave::BuffetConfig& config) {
  dbus_adaptor_.SetDeviceId(config.device_id());
  dbus_adaptor_.SetOemName(config.oem_name());
  dbus_adaptor_.SetModelName(config.model_name());
  dbus_adaptor_.SetModelId(config.model_id());
  dbus_adaptor_.SetName(config.name());
  dbus_adaptor_.SetDescription(config.description());
  dbus_adaptor_.SetLocation(config.location());
  dbus_adaptor_.SetAnonymousAccessRole(config.local_anonymous_access_role());
}

void Manager::UpdateWiFiBootstrapState(
    weave::privet::WifiBootstrapManager::State state) {
  switch (state) {
    case weave::WifiSetupState::kDisabled:
      dbus_adaptor_.SetWiFiBootstrapState("disabled");
      break;
    case weave::WifiSetupState::kBootstrapping:
      dbus_adaptor_.SetWiFiBootstrapState("waiting");
      break;
    case weave::WifiSetupState::kMonitoring:
      dbus_adaptor_.SetWiFiBootstrapState("monitoring");
      break;
    case weave::WifiSetupState::kConnecting:
      dbus_adaptor_.SetWiFiBootstrapState("connecting");
      break;
  }
}

void Manager::OnPairingStart(const std::string& session_id,
                             weave::PairingType pairing_type,
                             const std::vector<uint8_t>& code) {
  // For now, just overwrite the exposed PairInfo with
  // the most recent pairing attempt.
  dbus_adaptor_.SetPairingInfo(chromeos::VariantDictionary{
      {kPairingSessionIdKey, session_id},
      {kPairingModeKey, weave::privet::PairingTypeToString(pairing_type)},
      {kPairingCodeKey, code},
  });
}

void Manager::OnPairingEnd(const std::string& session_id) {
  auto exposed_pairing_attempt = dbus_adaptor_.GetPairingInfo();
  auto it = exposed_pairing_attempt.find(kPairingSessionIdKey);
  if (it == exposed_pairing_attempt.end()) {
    return;
  }
  std::string exposed_session{it->second.TryGet<std::string>()};
  if (exposed_session == session_id) {
    dbus_adaptor_.SetPairingInfo(chromeos::VariantDictionary{});
  }
}

}  // namespace buffet
