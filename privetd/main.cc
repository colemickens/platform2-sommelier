// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/json/json_reader.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/syslog_logging.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

#include "privetd/ap_manager_client.h"
#include "privetd/cloud_delegate.h"
#include "privetd/constants.h"
#include "privetd/daemon_state.h"
#include "privetd/device_delegate.h"
#include "privetd/peerd_client.h"
#include "privetd/privet_handler.h"
#include "privetd/security_manager.h"
#include "privetd/shill_client.h"
#include "privetd/wifi_bootstrap_manager.h"

namespace privetd {

namespace {

using libwebserv::Request;
using libwebserv::Response;

std::string GetFirstHeader(const Request& request, const std::string& name) {
  std::vector<std::string> headers = request.GetHeader(name);
  return headers.empty() ? std::string() : headers.front();
}

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon(uint16_t http_port_number,
         uint16_t https_port_number,
         bool disable_security,
         bool enable_ping,
         const std::vector<std::string>& device_whitelist,
         const base::FilePath& state_path)
      : http_port_number_(http_port_number),
        https_port_number_(https_port_number),
        disable_security_(disable_security),
        enable_ping_(enable_ping),
        device_whitelist_(device_whitelist),
        state_store_(new DaemonState(state_path)) {}

  int OnInit() override {
    int ret = DBusDaemon::OnInit();
    if (ret != EX_OK)
      return EX_OK;

    state_store_->Init();
    device_ = DeviceDelegate::CreateDefault(
        http_port_number_, https_port_number_, state_store_.get(),
        base::Bind(&Daemon::OnChanged, base::Unretained(this)));
    cloud_ = CloudDelegate::CreateDefault(
        bus_, device_.get(),
        base::Bind(&Daemon::OnChanged, base::Unretained(this)));
    // TODO(vitalybuka): Provide real embeded password.
    security_.reset(new SecurityManager("1234", disable_security_));
    shill_client_.reset(new ShillClient(bus_, device_whitelist_));
    ap_manager_client_.reset(new ApManagerClient(bus_));
    wifi_bootstrap_manager_.reset(new WifiBootstrapManager(
        state_store_.get(), shill_client_.get(), ap_manager_client_.get(),
        device_.get(), cloud_.get()));
    wifi_bootstrap_manager_->Init();

    privet_handler_.reset(new PrivetHandler(cloud_.get(), device_.get(),
                                            security_.get(),
                                            wifi_bootstrap_manager_.get()));

    if (!http_server_.Start(http_port_number_))
      return EX_UNAVAILABLE;

    security_->InitTlsData();
    if (!https_server_.StartWithTLS(https_port_number_,
                                    security_->GetTlsPrivateKey(),
                                    security_->GetTlsCertificate())) {
      return EX_UNAVAILABLE;
    }

    // TODO(vitalybuka): Device daemons should populate supported types on boot.
    device_->AddType("camera");

    http_server_.AddHandlerCallback(
        "/privet/", "",
        base::Bind(&Daemon::PrivetRequestHandler, base::Unretained(this)));
    https_server_.AddHandlerCallback(
        "/privet/", "",
        base::Bind(&Daemon::PrivetRequestHandler, base::Unretained(this)));

    if (enable_ping_) {
      http_server_.AddHandlerCallback(
          "/privet/ping", chromeos::http::request_type::kGet,
          base::Bind(&Daemon::HelloWorldHandler, base::Unretained(this)));
      https_server_.AddHandlerCallback(
          "/privet/ping", chromeos::http::request_type::kGet,
          base::Bind(&Daemon::HelloWorldHandler, base::Unretained(this)));
    }

    peerd_client_.reset(new PeerdClient(bus_, *device_, cloud_.get()));
    peerd_client_->Start();

    return EX_OK;
  }

  void OnShutdown(int* return_code) override {
    http_server_.Stop();
    https_server_.Stop();
    DBusDaemon::OnShutdown(return_code);
  }

 private:
  void PrivetRequestHandler(scoped_ptr<Request> request,
                            scoped_ptr<Response> response) {
    std::string auth_header = GetFirstHeader(
        *request, chromeos::http::request_header::kAuthorization);
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
      std::string content_type =
          chromeos::mime::RemoveParameters(GetFirstHeader(
              *request, chromeos::http::request_header::kContentType));
      if (content_type == chromeos::mime::application::kJson) {
        value.reset(base::JSONReader::Read(data));
        if (value)
          value->GetAsDictionary(&dictionary);
      }
    }

    privet_handler_->HandleRequest(
        request->GetPath(), auth_header, dictionary,
        base::Bind(&Daemon::PrivetResponseHandler, base::Unretained(this),
                   base::Passed(&response)));
  }

  void PrivetResponseHandler(scoped_ptr<Response> response,
                             int status,
                             const base::DictionaryValue& output) {
    VLOG(3) << "status: " << status << ", Output: " << output;
    if (status == chromeos::http::status_code::NotFound)
      response->ReplyWithErrorNotFound();
    else
      response->ReplyWithJson(status, &output);
  }

  void HelloWorldHandler(scoped_ptr<Request> request,
                         scoped_ptr<Response> response) {
    response->ReplyWithText(chromeos::http::status_code::Ok, "Hello, world!",
                            chromeos::mime::text::kPlain);
  }

  void OnChanged() {
    if (peerd_client_) {
      peerd_client_->Stop();
      peerd_client_->Start();
    }
  }

  uint16_t http_port_number_;
  uint16_t https_port_number_;
  bool disable_security_;
  bool enable_ping_;
  std::vector<std::string> device_whitelist_;
  std::unique_ptr<DaemonState> state_store_;
  std::unique_ptr<CloudDelegate> cloud_;
  std::unique_ptr<DeviceDelegate> device_;
  std::unique_ptr<SecurityManager> security_;
  std::unique_ptr<ShillClient> shill_client_;
  std::unique_ptr<ApManagerClient> ap_manager_client_;
  std::unique_ptr<WifiBootstrapManager> wifi_bootstrap_manager_;
  std::unique_ptr<PrivetHandler> privet_handler_;
  std::unique_ptr<PeerdClient> peerd_client_;
  libwebserv::Server http_server_;
  libwebserv::Server https_server_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // anonymous namespace

}  // namespace privetd

int main(int argc, char* argv[]) {
  DEFINE_bool(disable_security, false, "disable Privet security for tests");
  DEFINE_bool(enable_ping, false, "enable test HTTP handler at /privet/ping");
  DEFINE_int32(http_port, 8080, "HTTP port to listen for requests on");
  DEFINE_int32(https_port, 8081, "HTTPS port to listen for requests on");
  DEFINE_bool(log_to_stderr, false, "log trace messages to stderr as well");
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

  if (FLAGS_state_path.empty())
    FLAGS_state_path = privetd::kDefaultStateFilePath;

  if (FLAGS_http_port < 1 || FLAGS_http_port > 0xFFFF) {
    LOG(ERROR) << "Invalid HTTP port specified: '" << FLAGS_http_port << "'.";
    return EX_USAGE;
  }

  if (FLAGS_https_port < 1 || FLAGS_https_port > 0xFFFF) {
    LOG(ERROR) << "Invalid HTTPS port specified: '" << FLAGS_https_port << "'.";
    return EX_USAGE;
  }

  auto device_whitelist =
      chromeos::string_utils::Split(FLAGS_device_whitelist, ',', true, true);

  privetd::Daemon daemon(FLAGS_http_port, FLAGS_https_port,
                         FLAGS_disable_security, FLAGS_enable_ping,
                         device_whitelist, base::FilePath(FLAGS_state_path));
  return daemon.Run();
}
