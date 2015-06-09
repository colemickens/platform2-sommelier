// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/privet_manager.h"

#include <memory>
#include <set>
#include <string>
#include <sysexits.h>

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

const char kDefaultConfigFilePath[] = "/etc/privetd/privetd.conf";
const char kDefaultStateFilePath[] = "/var/lib/privetd/privetd.state";

std::string GetFirstHeader(const Request& request, const std::string& name) {
  std::vector<std::string> headers = request.GetHeader(name);
  return headers.empty() ? std::string() : headers.front();
}

const char kServiceName[] = "org.chromium.privetd";
const char kRootPath[] = "/org/chromium/privetd";

}  // namespace

Manager::Manager(bool disable_security,
                 bool enable_ping,
                 const std::set<std::string>& device_whitelist,
                 const base::FilePath& config_path,
                 const base::FilePath& state_path)
    : DBusServiceDaemon(kServiceName, kRootPath),
      disable_security_(disable_security),
      enable_ping_(enable_ping),
      device_whitelist_(device_whitelist),
      config_path_(config_path),
      state_store_(new DaemonState(state_path)) {
}

void Manager::RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) {
  parser_.reset(new PrivetdConfigParser);

  chromeos::KeyValueStore config_store;
  if (!config_store.Load(config_path_)) {
    LOG(ERROR) << "Failed to read privetd config file from "
               << config_path_.value();
  } else {
    CHECK(parser_->Parse(config_store)) << "Failed to read configuration file.";
  }
  state_store_->Init();
  // This state store key doesn't exist naturally, but developers
  // sometime put it in their state store to cause the device to bring
  // up WiFi bootstrapping while being connected to an ethernet interface.
  std::string test_device_whitelist;
  if (device_whitelist_.empty() &&
      state_store_->GetString(kWiFiBootstrapInterfaces,
                              &test_device_whitelist)) {
    auto interfaces =
        chromeos::string_utils::Split(test_device_whitelist, ",", true, true);
    device_whitelist_.insert(interfaces.begin(), interfaces.end());
  }
  device_ = DeviceDelegate::CreateDefault();
  cloud_ = CloudDelegate::CreateDefault(
      bus_, parser_->gcd_bootstrap_mode() != GcdBootstrapMode::kDisabled);
  cloud_observer_.Add(cloud_.get());
  security_.reset(new SecurityManager(parser_->pairing_modes(),
                                      parser_->embedded_code_path(),
                                      disable_security_));
  shill_client_.reset(new ShillClient(
      bus_, device_whitelist_.empty() ? parser_->automatic_wifi_interfaces()
                                      : device_whitelist_));
  shill_client_->RegisterConnectivityListener(
      base::Bind(&Manager::OnConnectivityChanged, base::Unretained(this)));
  ap_manager_client_.reset(new ApManagerClient(bus_));

  if (parser_->wifi_bootstrap_mode() != WiFiBootstrapMode::kDisabled) {
    VLOG(1) << "Enabling WiFi bootstrapping.";
    wifi_bootstrap_manager_.reset(new WifiBootstrapManager(
        state_store_.get(), shill_client_.get(), ap_manager_client_.get(),
        cloud_.get(), parser_->connect_timeout_seconds(),
        parser_->bootstrap_timeout_seconds(),
        parser_->monitor_timeout_seconds()));
    wifi_bootstrap_manager_->Init();
  }

  peerd_client_.reset(new PeerdClient(bus_, device_.get(), cloud_.get(),
                                      wifi_bootstrap_manager_.get()));

  privet_handler_.reset(
      new PrivetHandler(cloud_.get(), device_.get(), security_.get(),
                        wifi_bootstrap_manager_.get(), peerd_client_.get()));
  web_server_.reset(new libwebserv::Server);
  web_server_->OnProtocolHandlerConnected(base::Bind(
      &Manager::OnProtocolHandlerConnected, weak_ptr_factory_.GetWeakPtr()));
  web_server_->OnProtocolHandlerDisconnected(base::Bind(
      &Manager::OnProtocolHandlerDisconnected, weak_ptr_factory_.GetWeakPtr()));

  web_server_->Connect(bus_, kServiceName,
                       sequencer->GetHandler("Server::Connect failed.", true),
                       base::Bind(&base::DoNothing),
                       base::Bind(&base::DoNothing));

  web_server_->GetDefaultHttpHandler()->AddHandlerCallback(
      "/privet/", "",
      base::Bind(&Manager::PrivetRequestHandler, base::Unretained(this)));
  web_server_->GetDefaultHttpsHandler()->AddHandlerCallback(
      "/privet/", "",
      base::Bind(&Manager::PrivetRequestHandler, base::Unretained(this)));
  if (enable_ping_) {
    web_server_->GetDefaultHttpHandler()->AddHandlerCallback(
        "/privet/ping", chromeos::http::request_type::kGet,
        base::Bind(&Manager::HelloWorldHandler, base::Unretained(this)));
    web_server_->GetDefaultHttpsHandler()->AddHandlerCallback(
        "/privet/ping", chromeos::http::request_type::kGet,
        base::Bind(&Manager::HelloWorldHandler, base::Unretained(this)));
  }
}

void Manager::OnShutdown(int* return_code) {
  web_server_->Disconnect();
  DBusDaemon::OnShutdown(return_code);
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
      value.reset(base::JSONReader::Read(data));
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

int old_main(int argc, char* argv[]) {
  DEFINE_bool(disable_security, false, "disable Privet security for tests");
  DEFINE_bool(enable_ping, false, "enable test HTTP handler at /privet/ping");
  DEFINE_bool(log_to_stderr, false, "log trace messages to stderr as well");
  DEFINE_string(config_path, privetd::kDefaultConfigFilePath,
                "Path to file containing config information.");
  DEFINE_string(state_path, privetd::kDefaultStateFilePath,
                "Path to file containing state information.");
  DEFINE_string(device_whitelist, "",
                "Comma separated list of network interfaces to monitor for "
                "connectivity (an empty list enables all interfaces).");

  chromeos::FlagHelper::Init(argc, argv, "Privet protocol handler daemon");

  int flags = chromeos::kLogToSyslog;
  if (FLAGS_log_to_stderr)
    flags |= chromeos::kLogToStderr;
  chromeos::InitLog(flags | chromeos::kLogHeader);

  if (FLAGS_config_path.empty())
    FLAGS_config_path = privetd::kDefaultConfigFilePath;

  if (FLAGS_state_path.empty())
    FLAGS_state_path = privetd::kDefaultStateFilePath;

  auto device_whitelist =
      chromeos::string_utils::Split(FLAGS_device_whitelist, ",", true, true);

  privetd::Manager daemon(
      FLAGS_disable_security, FLAGS_enable_ping,
      std::set<std::string>(device_whitelist.begin(), device_whitelist.end()),
      base::FilePath(FLAGS_config_path), base::FilePath(FLAGS_state_path));
  return daemon.Run();
}
