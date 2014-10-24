// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/syslog_logging.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

namespace {

const char kHelpFlag[] = "help";
const char kLogToStdErrFlag[] = "log_to_stderr";
const char kHelpMessage[] = "\n"
    "This is the Privet protocol handler daemon.\n"
    "Usage: privetd [--v=<logging level>]\n"
    "               [--vmodule=<see base/logging.h>]\n"
    "               [--log_to_stderr]";

using libwebserv::Request;
using libwebserv::Response;

void HelloWorldRequestHandler(const std::shared_ptr<const Request>& request,
                              const std::shared_ptr<Response>& response) {
  response->ReplyWithText(
      chromeos::http::status_code::Ok,
      "<html><body><h1>Hello world from privetd!</h1></body></html>",
      chromeos::mime::text::kHtml);
}

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon() {}

  int OnInit() override {
    int ret = DBusDaemon::OnInit();
    if (ret != EX_OK)
      return EX_OK;

    if (!web_server_.Start(8080))
      return EX_UNAVAILABLE;
    web_server_.AddHandlerCallback("/privet/",
                                   chromeos::http::request_type::kGet,
                                   base::Bind(HelloWorldRequestHandler));
    return EX_OK;
  }

  void OnShutdown(int* return_code) {
    web_server_.Stop();
    DBusDaemon::OnShutdown(return_code);
  }

 private:
  libwebserv::Server web_server_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // anonymous namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kHelpFlag)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }
  chromeos::InitFlags flags = chromeos::kLogToSyslog;
  if (cl->HasSwitch(kLogToStdErrFlag)) {
    flags = chromeos::kLogToStderr;
  }
  chromeos::InitLog(flags | chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}
