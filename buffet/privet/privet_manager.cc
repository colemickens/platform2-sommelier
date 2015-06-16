// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/privet_manager.h"

#include <memory>
#include <set>
#include <string>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/json/json_reader.h>
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
#include "buffet/privet/ap_manager_client.h"
#include "buffet/privet/cloud_delegate.h"
#include "buffet/privet/constants.h"
#include "buffet/privet/daemon_state.h"
#include "buffet/privet/device_delegate.h"
#include "buffet/privet/peerd_client.h"
#include "buffet/privet/privet_handler.h"
#include "buffet/privet/privetd_conf_parser.h"
#include "buffet/privet/security_manager.h"
#include "buffet/privet/shill_client.h"
#include "buffet/privet/wifi_bootstrap_manager.h"

namespace privetd {

namespace {

using chromeos::dbus_utils::AsyncEventSequencer;
using libwebserv::ProtocolHandler;
using libwebserv::Request;
using libwebserv::Response;

const char kDefaultStateFilePath[] = "/var/lib/privetd/privetd.state";

std::string GetFirstHeader(const Request& request, const std::string& name) {
  std::vector<std::string> headers = request.GetHeader(name);
  return headers.empty() ? std::string() : headers.front();
}

}  // namespace

Manager::Manager() {
}

Manager::~Manager() {
}

void Manager::Start(const Options& options,
                    const scoped_refptr<dbus::Bus>& bus,
                    buffet::DeviceRegistrationInfo* device,
                    buffet::CommandManager* command_manager,
                    buffet::StateManager* state_manager,
                    AsyncEventSequencer* sequencer) {
  disable_security_ = options.disable_security;

  // TODO(vitalybuka): switch to BuffetConfig.
  base::FilePath state_path{privetd::kDefaultStateFilePath};

  state_store_.reset(new DaemonState(state_path));
  parser_.reset(new PrivetdConfigParser);

  chromeos::KeyValueStore config_store;
  if (!config_store.Load(options.config_path)) {
    LOG(ERROR) << "Failed to read privetd config file from "
               << options.config_path.value();
  } else {
    CHECK(parser_->Parse(config_store)) << "Failed to read configuration file.";
  }
  state_store_->Init();

  std::set<std::string> device_whitelist{options.device_whitelist};
  // This state store key doesn't exist naturally, but developers
  // sometime put it in their state store to cause the device to bring
  // up WiFi bootstrapping while being connected to an ethernet interface.
  std::string test_device_whitelist;
  if (device_whitelist.empty() &&
      state_store_->GetString(kWiFiBootstrapInterfaces,
                              &test_device_whitelist)) {
    auto interfaces =
        chromeos::string_utils::Split(test_device_whitelist, ",", true, true);
    device_whitelist.insert(interfaces.begin(), interfaces.end());
  }
  device_ = DeviceDelegate::CreateDefault();
  cloud_ = CloudDelegate::CreateDefault(
      parser_->gcd_bootstrap_mode() != GcdBootstrapMode::kDisabled, device,
      command_manager, state_manager);
  cloud_observer_.Add(cloud_.get());
  security_.reset(new SecurityManager(parser_->pairing_modes(),
                                      parser_->embedded_code_path(),
                                      disable_security_));
  shill_client_.reset(new ShillClient(
      bus, device_whitelist.empty() ? parser_->automatic_wifi_interfaces()
                                    : device_whitelist));
  shill_client_->RegisterConnectivityListener(
      base::Bind(&Manager::OnConnectivityChanged, base::Unretained(this)));
  ap_manager_client_.reset(new ApManagerClient(bus));

  if (parser_->wifi_bootstrap_mode() != WiFiBootstrapMode::kDisabled) {
    VLOG(1) << "Enabling WiFi bootstrapping.";
    wifi_bootstrap_manager_.reset(new WifiBootstrapManager(
        state_store_.get(), shill_client_.get(), ap_manager_client_.get(),
        cloud_.get(), parser_->connect_timeout_seconds(),
        parser_->bootstrap_timeout_seconds(),
        parser_->monitor_timeout_seconds()));
    wifi_bootstrap_manager_->Init();
  }

  peerd_client_.reset(new PeerdClient(bus, device_.get(), cloud_.get(),
                                      wifi_bootstrap_manager_.get()));

  privet_handler_.reset(
      new PrivetHandler(cloud_.get(), device_.get(), security_.get(),
                        wifi_bootstrap_manager_.get(), peerd_client_.get()));

  web_server_.reset(new libwebserv::Server);
  web_server_->OnProtocolHandlerConnected(base::Bind(
      &Manager::OnProtocolHandlerConnected, weak_ptr_factory_.GetWeakPtr()));
  web_server_->OnProtocolHandlerDisconnected(base::Bind(
      &Manager::OnProtocolHandlerDisconnected, weak_ptr_factory_.GetWeakPtr()));

  web_server_->Connect(bus, buffet::dbus_constants::kServiceName,
                       sequencer->GetHandler("Server::Connect failed.", true),
                       base::Bind(&base::DoNothing),
                       base::Bind(&base::DoNothing));

  web_server_->GetDefaultHttpHandler()->AddHandlerCallback(
      "/privet/", "",
      base::Bind(&Manager::PrivetRequestHandler, base::Unretained(this)));
  web_server_->GetDefaultHttpsHandler()->AddHandlerCallback(
      "/privet/", "",
      base::Bind(&Manager::PrivetRequestHandler, base::Unretained(this)));
  if (options.enable_ping) {
    web_server_->GetDefaultHttpHandler()->AddHandlerCallback(
        "/privet/ping", chromeos::http::request_type::kGet,
        base::Bind(&Manager::HelloWorldHandler, base::Unretained(this)));
    web_server_->GetDefaultHttpsHandler()->AddHandlerCallback(
        "/privet/ping", chromeos::http::request_type::kGet,
        base::Bind(&Manager::HelloWorldHandler, base::Unretained(this)));
  }
}

void Manager::OnShutdown() {
  web_server_->Disconnect();
}

void Manager::OnDeviceInfoChanged() {
  OnChanged();
}

void Manager::PrivetRequestHandler(std::unique_ptr<Request> request,
                                   std::unique_ptr<Response> response) {
  std::string auth_header =
      GetFirstHeader(*request, chromeos::http::request_header::kAuthorization);
  if (auth_header.empty() && disable_security_)
    auth_header = "Privet anonymous";
  std::string data(request->GetData().begin(), request->GetData().end());
  VLOG(3) << "Input: " << data;

  base::DictionaryValue empty;
  std::unique_ptr<base::Value> value;
  const base::DictionaryValue* dictionary = nullptr;

  if (data.empty()) {
    dictionary = &empty;
  } else {
    std::string content_type = chromeos::mime::RemoveParameters(
        GetFirstHeader(*request, chromeos::http::request_header::kContentType));
    if (content_type == chromeos::mime::application::kJson) {
      value.reset(base::JSONReader::Read(data).release());
      if (value)
        value->GetAsDictionary(&dictionary);
    }
  }

  privet_handler_->HandleRequest(
      request->GetPath(), auth_header, dictionary,
      base::Bind(&Manager::PrivetResponseHandler, base::Unretained(this),
                 base::Passed(&response)));
}

void Manager::PrivetResponseHandler(std::unique_ptr<Response> response,
                                    int status,
                                    const base::DictionaryValue& output) {
  VLOG(3) << "status: " << status << ", Output: " << output;
  response->ReplyWithJson(status, &output);
}

void Manager::HelloWorldHandler(std::unique_ptr<Request> request,
                                std::unique_ptr<Response> response) {
  response->ReplyWithText(chromeos::http::status_code::Ok, "Hello, world!",
                          chromeos::mime::text::kPlain);
}

void Manager::OnChanged() {
  if (peerd_client_)
    peerd_client_->Update();
}

void Manager::OnConnectivityChanged(bool online) {
  OnChanged();
}

void Manager::OnProtocolHandlerConnected(ProtocolHandler* protocol_handler) {
  if (protocol_handler->GetName() == ProtocolHandler::kHttp) {
    device_->SetHttpPort(*protocol_handler->GetPorts().begin());
    if (peerd_client_)
      peerd_client_->Update();
  } else if (protocol_handler->GetName() == ProtocolHandler::kHttps) {
    device_->SetHttpsPort(*protocol_handler->GetPorts().begin());
    security_->SetCertificateFingerprint(
        protocol_handler->GetCertificateFingerprint());
  }
}

void Manager::OnProtocolHandlerDisconnected(ProtocolHandler* protocol_handler) {
  if (protocol_handler->GetName() == ProtocolHandler::kHttp) {
    device_->SetHttpPort(0);
    if (peerd_client_)
      peerd_client_->Update();
  } else if (protocol_handler->GetName() == ProtocolHandler::kHttps) {
    device_->SetHttpsPort(0);
    security_->SetCertificateFingerprint({});
  }
}

}  // namespace privetd
