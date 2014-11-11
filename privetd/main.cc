// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/syslog_logging.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/privet_handler.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

namespace {

const char kHelpFlag[] = "help";
const char kPort[] = "port";
const char kAllowEmptyAuth[] = "allow_empty_auth";
const char kLogToStdErrFlag[] = "log_to_stderr";
const char kHelpMessage[] =
    "\n"
    "This is the Privet protocol handler daemon.\n"
    "Usage: privetd [--v=<logging level>]\n"
    "               [--vmodule=<see base/logging.h>]\n"
    "               [--port=<tcp_port>]\n"
    "               [--allow_empty_auth]\n"
    "               [--log_to_stderr]";

using libwebserv::Request;
using libwebserv::Response;

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon(uint16_t port_number, bool allow_empty_auth)
      : port_number_(port_number), allow_empty_auth_(allow_empty_auth) {}

  int OnInit() override {
    int ret = DBusDaemon::OnInit();
    if (ret != EX_OK)
      return EX_OK;

    if (!web_server_.Start(port_number_))
      return EX_UNAVAILABLE;

    cloud_ = privetd::CloudDelegate::CreateDefault();
    device_ = privetd::DeviceDelegate::CreateDefault(port_number_, 0);
    security_ = privetd::SecurityDelegate::CreateDefault();
    wifi_ = privetd::WifiDelegate::CreateDefault();
    privet_handler_.reset(new privetd::PrivetHandler(
        cloud_.get(), device_.get(), security_.get(), wifi_.get()));

    // TODO(vitalybuka): Device daemons should populate supported types on boot.
    device_->AddType("camera");

    web_server_.AddHandlerCallback(
        "/privet/", chromeos::http::request_type::kGet,
        base::Bind(&Daemon::PrivetRequestHandler, base::Unretained(this)));
    web_server_.AddHandlerCallback(
        "/privet/", chromeos::http::request_type::kPost,
        base::Bind(&Daemon::PrivetRequestHandler, base::Unretained(this)));

    return EX_OK;
  }

  void OnShutdown(int* return_code) override {
    web_server_.Stop();
    DBusDaemon::OnShutdown(return_code);
  }

 private:
  void PrivetRequestHandler(const std::shared_ptr<const Request>& request,
                            const std::shared_ptr<Response>& response) {
    std::vector<std::string> auth_headers =
        request->GetHeader(chromeos::http::request_header::kAuthorization);
    std::string auth_header;
    if (!auth_headers.empty())
      auth_header = auth_headers.front();
    else if (allow_empty_auth_)
      auth_header = "Privet anonymous";
    base::DictionaryValue input;
    if (!privet_handler_->HandleRequest(
            request->GetPath(), auth_header, input,
            base::Bind(&Daemon::PrivetResponseHandler, base::Unretained(this),
                       response))) {
      response->ReplyWithErrorNotFound();
    }
  }

  void PrivetResponseHandler(const std::shared_ptr<Response>& response,
                             const base::DictionaryValue& output,
                             bool success) {
    response->ReplyWithJson(chromeos::http::status_code::Ok, &output);
  }

  uint16_t port_number_;
  bool allow_empty_auth_;
  std::unique_ptr<privetd::CloudDelegate> cloud_;
  std::unique_ptr<privetd::DeviceDelegate> device_;
  std::unique_ptr<privetd::SecurityDelegate> security_;
  std::unique_ptr<privetd::WifiDelegate> wifi_;
  std::unique_ptr<privetd::PrivetHandler> privet_handler_;
  libwebserv::Server web_server_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // anonymous namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  int flags = chromeos::kLogToSyslog;
  if (cl->HasSwitch(kLogToStdErrFlag))
    flags |= chromeos::kLogToStderr;
  chromeos::InitLog(flags | chromeos::kLogHeader);

  if (cl->HasSwitch(kHelpFlag)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }

  uint16_t port_number = 8080;  // Default port to use.
  if (cl->HasSwitch(kPort)) {
    std::string port_str = cl->GetSwitchValueASCII(kPort);
    int value = 0;
    if (!base::StringToInt(port_str, &value) || value < 1 || value > 0xFFFF) {
      LOG(ERROR) << "Invalid port number specified: '" << port_str << "'.";
      return EX_USAGE;
    }
    port_number = value;
  }
  Daemon daemon(port_number, cl->HasSwitch(kAllowEmptyAuth));
  return daemon.Run();
}
