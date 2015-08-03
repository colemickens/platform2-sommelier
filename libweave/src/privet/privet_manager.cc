// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/privet_manager.h"

#include <memory>
#include <set>
#include <string>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/memory/weak_ptr.h>
#include <base/scoped_observer.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/http/http_request.h>
#include <chromeos/key_value_store.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/syslog_logging.h>
#include <libwebserv/protocol_handler.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

#include "buffet/dbus_constants.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/privet/cloud_delegate.h"
#include "libweave/src/privet/constants.h"
#include "libweave/src/privet/device_delegate.h"
#include "libweave/src/privet/privet_handler.h"
#include "libweave/src/privet/publisher.h"
#include "weave/network.h"

namespace weave {
namespace privet {

namespace {

using chromeos::dbus_utils::AsyncEventSequencer;

}  // namespace

Manager::Manager() {
}

Manager::~Manager() {
}

void Manager::Start(const Device::Options& options,
                    const scoped_refptr<dbus::Bus>& bus,
                    Network* network,
                    Mdns* mdns,
                    HttpServer* http_server,
                    DeviceRegistrationInfo* device,
                    CommandManager* command_manager,
                    StateManager* state_manager,
                    AsyncEventSequencer* sequencer) {
  disable_security_ = options.disable_security;

  device_ = DeviceDelegate::CreateDefault();
  cloud_ = CloudDelegate::CreateDefault(device, command_manager, state_manager);
  cloud_observer_.Add(cloud_.get());
  security_.reset(new SecurityManager(device->GetConfig().pairing_modes(),
                                      device->GetConfig().embedded_code_path(),
                                      disable_security_));
  network->AddOnConnectionChangedCallback(
      base::Bind(&Manager::OnConnectivityChanged, base::Unretained(this)));

  if (device->GetConfig().wifi_auto_setup_enabled()) {
    VLOG(1) << "Enabling WiFi bootstrapping.";
    wifi_bootstrap_manager_.reset(new WifiBootstrapManager(
        device->GetConfig().last_configured_ssid(), options.test_privet_ssid,
        device->GetConfig().ble_setup_enabled(), network, cloud_.get()));
    wifi_bootstrap_manager_->Init();
  }

  publisher_.reset(new Publisher(device_.get(), cloud_.get(),
                                 wifi_bootstrap_manager_.get(), mdns));

  privet_handler_.reset(
      new PrivetHandler(cloud_.get(), device_.get(), security_.get(),
                        wifi_bootstrap_manager_.get(), publisher_.get()));

  http_server->AddOnStateChangedCallback(base::Bind(
      &Manager::OnHttpServerStatusChanged, weak_ptr_factory_.GetWeakPtr()));
  http_server->AddRequestHandler("/privet/",
                                 base::Bind(&Manager::PrivetRequestHandler,
                                            weak_ptr_factory_.GetWeakPtr()));
  if (options.enable_ping) {
    http_server->AddRequestHandler("/privet/ping",
                                   base::Bind(&Manager::HelloWorldHandler,
                                              weak_ptr_factory_.GetWeakPtr()));
  }
}

std::string Manager::GetCurrentlyConnectedSsid() const {
  return wifi_bootstrap_manager_
             ? wifi_bootstrap_manager_->GetCurrentlyConnectedSsid()
             : "";
}

void Manager::AddOnWifiSetupChangedCallback(
    const WifiBootstrapManager::StateListener& callback) {
  if (wifi_bootstrap_manager_)
    wifi_bootstrap_manager_->RegisterStateListener(callback);
  else
    callback.Run(WifiSetupState::kDisabled);
}

void Manager::AddOnPairingChangedCallbacks(
    const SecurityManager::PairingStartListener& on_start,
    const SecurityManager::PairingEndListener& on_end) {
  security_->RegisterPairingListeners(on_start, on_end);
}

void Manager::OnDeviceInfoChanged() {
  OnChanged();
}

void Manager::PrivetRequestHandler(
    const HttpServer::Request& request,
    const HttpServer::OnReplyCallback& callback) {
  std::string auth_header =
      request.GetFirstHeader(chromeos::http::request_header::kAuthorization);
  if (auth_header.empty() && disable_security_)
    auth_header = "Privet anonymous";
  std::string data(request.GetData().begin(), request.GetData().end());
  VLOG(3) << "Input: " << data;

  base::DictionaryValue empty;
  std::unique_ptr<base::Value> value;
  const base::DictionaryValue* dictionary = &empty;

  std::string content_type = chromeos::mime::RemoveParameters(
      request.GetFirstHeader(chromeos::http::request_header::kContentType));
  if (content_type == chromeos::mime::application::kJson) {
    value.reset(base::JSONReader::Read(data).release());
    if (value)
      value->GetAsDictionary(&dictionary);
  }

  privet_handler_->HandleRequest(
      request.GetPath(), auth_header, dictionary,
      base::Bind(&Manager::PrivetResponseHandler,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void Manager::PrivetResponseHandler(const HttpServer::OnReplyCallback& callback,
                                    int status,
                                    const base::DictionaryValue& output) {
  VLOG(3) << "status: " << status << ", Output: " << output;
  std::string data;
  base::JSONWriter::WriteWithOptions(
      output, base::JSONWriter::OPTIONS_PRETTY_PRINT, &data);
  callback.Run(status, data, chromeos::mime::application::kJson);
}

void Manager::HelloWorldHandler(const HttpServer::Request& request,
                                const HttpServer::OnReplyCallback& callback) {
  callback.Run(chromeos::http::status_code::Ok, "Hello, world!",
               chromeos::mime::text::kPlain);
}

void Manager::OnChanged() {
  if (publisher_)
    publisher_->Update();
}

void Manager::OnConnectivityChanged(bool online) {
  OnChanged();
}

void Manager::OnHttpServerStatusChanged(const HttpServer& server) {
  if (device_->GetHttpEnpoint().first != server.GetHttpPort()) {
    device_->SetHttpPort(server.GetHttpPort());
    if (publisher_)  // Only HTTP port is published
      publisher_->Update();
  }
  device_->SetHttpsPort(server.GetHttpsPort());
  security_->SetCertificateFingerprint(server.GetHttpsCertificateFingerprint());
}

}  // namespace privet
}  // namespace weave
