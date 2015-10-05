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
#include <chromeos/http/http_utils.h>
#include <chromeos/key_value_store.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/mime_utils.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>
#include <dbus/values_util.h>
#include <weave/enum_to_string.h>

#include "buffet/buffet_config.h"
#include "buffet/dbus_command_dispatcher.h"
#include "buffet/dbus_conversion.h"
#include "buffet/http_transport_client.h"
#include "buffet/shill_client.h"
#include "buffet/weave_error_conversion.h"

#ifdef BUFFET_USE_WIFI_BOOTSTRAPPING
#include "buffet/peerd_client.h"
#include "buffet/webserv_client.h"
#endif  // BUFFET_USE_WIFI_BOOTSTRAPPING

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

namespace {

const char kPairingSessionIdKey[] = "sessionId";
const char kPairingModeKey[] = "mode";
const char kPairingCodeKey[] = "code";

const char kErrorDomain[] = "buffet";

}  // anonymous namespace

class Manager::TaskRunner : public weave::provider::TaskRunner {
 public:
  void PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    chromeos::MessageLoop::current()->PostDelayedTask(from_here, task, delay);
  }
};

Manager::Manager(const Options& options,
                 const base::WeakPtr<ExportedObjectManager>& object_manager)
    : options_{options},
      dbus_object_(object_manager.get(),
                   object_manager->GetBus(),
                   org::chromium::Buffet::ManagerAdaptor::GetObjectPath()) {}

Manager::~Manager() {
}

void Manager::Start(AsyncEventSequencer* sequencer) {
  RestartWeave(sequencer);

  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Manager.RegisterAsync() failed.", true));
}

void Manager::RestartWeave(AsyncEventSequencer* sequencer) {
  Stop();

  task_runner_.reset(new TaskRunner{});
  config_.reset(new BuffetConfig{options_.config_options});
  http_client_.reset(new HttpTransportClient);
  shill_client_.reset(new ShillClient{dbus_object_.GetBus(),
                                      options_.device_whitelist,
                                      !options_.xmpp_enabled});
  weave::provider::DnsServiceDiscovery* mdns{nullptr};
  weave::provider::HttpServer* http_server{nullptr};
#ifdef BUFFET_USE_WIFI_BOOTSTRAPPING
  if (!options_.disable_privet) {
    peerd_client_.reset(new PeerdClient{dbus_object_.GetBus()});
    web_serv_client_.reset(new WebServClient{dbus_object_.GetBus(), sequencer});
    mdns = peerd_client_.get();
    http_server = web_serv_client_.get();

    if (options_.enable_ping) {
      http_server->AddRequestHandler(
          "/privet/ping",
          base::Bind(
              [](const weave::provider::HttpServer::Request& request,
                 const weave::provider::HttpServer::OnReplyCallback& callback) {
                callback.Run(chromeos::http::status_code::Ok, "Hello, world!",
                             chromeos::mime::text::kPlain);
              }));
    }
  }
#endif  // BUFFET_USE_WIFI_BOOTSTRAPPING

  device_ = weave::Device::Create(config_.get(), task_runner_.get(),
                                  http_client_.get(), shill_client_.get(), mdns,
                                  http_server, shill_client_.get(), nullptr);

  device_->AddSettingsChangedCallback(
      base::Bind(&Manager::OnConfigChanged, weak_ptr_factory_.GetWeakPtr()));

  command_dispatcher_.reset(
      new DBusCommandDispacher{dbus_object_.GetObjectManager(), device_.get()});

  device_->AddStateChangedCallback(
      base::Bind(&Manager::OnStateChanged, weak_ptr_factory_.GetWeakPtr()));

  device_->AddGcdStateChangedCallback(
      base::Bind(&Manager::OnGcdStateChanged, weak_ptr_factory_.GetWeakPtr()));

  device_->AddPairingChangedCallbacks(
      base::Bind(&Manager::OnPairingStart, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&Manager::OnPairingEnd, weak_ptr_factory_.GetWeakPtr()));
}

void Manager::Stop() {
  command_dispatcher_.reset();
  device_.reset();
#ifdef BUFFET_USE_WIFI_BOOTSTRAPPING
  web_serv_client_.reset();
  peerd_client_.reset();
#endif  // BUFFET_USE_WIFI_BOOTSTRAPPING
  shill_client_.reset();
  http_client_.reset();
  config_.reset();
  task_runner_.reset();
}

// TODO(vitalybuka): Remove, it's just duplicate of property.
void Manager::CheckDeviceRegistered(
    DBusMethodResponsePtr<std::string> response) {
  LOG(INFO) << "Received call to Manager.CheckDeviceRegistered()";
  response->Return(dbus_adaptor_.GetDeviceId());
}

void Manager::RegisterDevice(DBusMethodResponsePtr<std::string> response,
                             const std::string& ticket_id) {
  LOG(INFO) << "Received call to Manager.RegisterDevice()";

  weave::ErrorPtr error;
  std::string device_id = device_->Register(ticket_id, &error);
  if (!device_id.empty()) {
    response->Return(device_id);
    return;
  }
  chromeos::ErrorPtr chromeos_error;
  ConvertError(*error, &chromeos_error);
  response->ReplyWithError(chromeos_error.get());
}

void Manager::UpdateState(DBusMethodResponsePtr<> response,
                          const chromeos::VariantDictionary& property_set) {
  chromeos::ErrorPtr chromeos_error;
  auto properties =
      DictionaryFromDBusVariantDictionary(property_set, &chromeos_error);
  if (!properties)
    return response->ReplyWithError(chromeos_error.get());

  weave::ErrorPtr error;
  if (!device_->SetStateProperties(*properties, &error)) {
    ConvertError(*error, &chromeos_error);
    return response->ReplyWithError(chromeos_error.get());
  }
  response->Return();
}

bool Manager::GetState(chromeos::ErrorPtr* error, std::string* state) {
  auto json = device_->GetState();
  CHECK(json);
  base::JSONWriter::WriteWithOptions(
      *json, base::JSONWriter::OPTIONS_PRETTY_PRINT, state);
  return true;
}

void Manager::AddCommand(DBusMethodResponsePtr<std::string> response,
                         const std::string& json_command) {
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

  std::string id;
  weave::ErrorPtr error;
  if (!device_->AddCommand(*command, &id, &error)) {
    chromeos::ErrorPtr chromeos_error;
    ConvertError(*error, &chromeos_error);
    return response->ReplyWithError(chromeos_error.get());
  }

  response->Return(id);
}

void Manager::GetCommand(DBusMethodResponsePtr<std::string> response,
                         const std::string& id) {
  const weave::Command* command = device_->FindCommand(id);
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

std::string Manager::TestMethod(const std::string& message) {
  LOG(INFO) << "Received call to test method: " << message;
  return message;
}

bool Manager::UpdateDeviceInfo(chromeos::ErrorPtr* chromeos_error,
                               const std::string& name,
                               const std::string& description,
                               const std::string& location) {
  base::DictionaryValue command;
  command.SetString("name", "base.updateDeviceInfo");
  std::unique_ptr<base::DictionaryValue> parameters{new base::DictionaryValue};
  parameters->SetString("name", name);
  parameters->SetString("description", description);
  parameters->SetString("location", location);
  command.Set("parameters", parameters.release());

  std::string id;
  weave::ErrorPtr weave_error;
  if (!device_->AddCommand(command, &id, &weave_error)) {
    ConvertError(*weave_error, chromeos_error);
    return false;
  }
  // TODO(vitalybuka): Wait for command DONE. Currently we know that command
  // will be handled inside of AddCommand. But this could be changed in future.
  CHECK_EQ(device_->GetSettings().name, name);
  CHECK_EQ(device_->GetSettings().description, description);
  CHECK_EQ(device_->GetSettings().location, location);
  return true;
}

bool Manager::UpdateServiceConfig(chromeos::ErrorPtr* chromeos_error,
                                  const std::string& client_id,
                                  const std::string& client_secret,
                                  const std::string& api_key,
                                  const std::string& oauth_url,
                                  const std::string& service_url) {
  if (!dbus_adaptor_.GetDeviceId().empty()) {
    chromeos::Error::AddTo(chromeos_error, FROM_HERE, kErrorDomain,
                           "already_registered",
                           "Unable to change config for registered device");
    return false;
  }

  options_.config_options.client_id = client_id;
  options_.config_options.client_secret = client_secret;
  options_.config_options.api_key = api_key;
  options_.config_options.oauth_url = oauth_url;
  options_.config_options.service_url = service_url;

  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  RestartWeave(sequencer.get());
  return true;
}

void Manager::OnStateChanged() {
  auto state = device_->GetState();
  CHECK(state);
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *state, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  dbus_adaptor_.SetState(json);
}

void Manager::OnGcdStateChanged(weave::GcdState state) {
  dbus_adaptor_.SetStatus(weave::EnumToString(state));
}

void Manager::OnConfigChanged(const weave::Settings& settings) {
  dbus_adaptor_.SetDeviceId(settings.cloud_id);
  dbus_adaptor_.SetOemName(settings.oem_name);
  dbus_adaptor_.SetModelName(settings.model_name);
  dbus_adaptor_.SetModelId(settings.model_id);
  dbus_adaptor_.SetName(settings.name);
  dbus_adaptor_.SetDescription(settings.description);
  dbus_adaptor_.SetLocation(settings.location);
}

void Manager::OnPairingStart(const std::string& session_id,
                             weave::PairingType pairing_type,
                             const std::vector<uint8_t>& code) {
  // For now, just overwrite the exposed PairInfo with
  // the most recent pairing attempt.
  dbus_adaptor_.SetPairingInfo(chromeos::VariantDictionary{
      {kPairingSessionIdKey, session_id},
      {kPairingModeKey, weave::EnumToString(pairing_type)},
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
