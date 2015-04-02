// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <sysexits.h>

#include <base/command_line.h>
#include <base/json/json_reader.h>
#include <base/values.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/syslog_logging.h>
#include <libwebserv/protocol_handler.h>

#include "leaderd/manager.h"
#include "leaderd/peerd_client.h"
#include "leaderd/webserver_client.h"

using chromeos::DBusServiceDaemon;
using chromeos::dbus_utils::AsyncEventSequencer;
using leaderd::Manager;
using leaderd::PeerdClient;
using leaderd::PeerdClientImpl;
using leaderd::WebServerClient;

namespace {

const char kServiceNameFlag[] = "service_name";
const char kPeerdServiceNameFlag[] = "peerd_service_name";
const char kProtocolHandlerNameFlag[] = "protocol_handler_name";

const char kServiceName[] = "org.chromium.leaderd";
const char kRootServicePath[] = "/org/chromium/leaderd";
const char kPeerdServiceName[] = "org.chromium.peerd";

const char kHelpFlag[] = "help";
const char kHelpMessage[] =
    "\n"
    "This daemon allows groups of devices to elect a leader device.\n"
    "Usage: leaderd [--v=<logging level>]\n"
    "               [--vmodule=<see base/logging.h>]\n"
    "               [--service_name=<DBus service name to claim>]\n"
    "               [--peerd_service_name=<DBus service name of peerd>]\n"
    "               [--protocol_handler_name=<name of webserver handler>]\n";

class Daemon : public DBusServiceDaemon {
 public:
  Daemon(const std::string& leaderd_service_name,
         const std::string& peerd_service_name,
         const std::string& web_handler_name)
      : DBusServiceDaemon{leaderd_service_name, kRootServicePath},
        peerd_service_name_{peerd_service_name},
        web_handler_name_{web_handler_name} {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    std::unique_ptr<PeerdClient> peerd_client(
        new PeerdClientImpl(bus_, peerd_service_name_));
    manager_.reset(
        new Manager{bus_, object_manager_.get(), std::move(peerd_client)});
    manager_->RegisterAsync(sequencer);
    webserver_.reset(
        new WebServerClient(manager_.get(), web_handler_name_));
    webserver_->RegisterAsync(bus_, service_name_, sequencer);
    LOG(INFO) << "leaderd starting";
  }

  void OnShutdown(int* return_code) override {
    DBusServiceDaemon::OnShutdown(return_code);
    webserver_.reset();
    manager_.reset();
  }

 private:
  const std::string peerd_service_name_;
  const std::string web_handler_name_;

  std::unique_ptr<Manager> manager_;
  std::unique_ptr<WebServerClient> webserver_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kHelpFlag)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }
  // In test, we'll claim a slightly different leaderd service name in order
  // to support starting multiple instances of leaderd on the same machine.
  std::string leaderd_service_name = cl->GetSwitchValueASCII(kServiceNameFlag);
  if (leaderd_service_name.empty()) { leaderd_service_name = kServiceName; }
  // Similarly, each instance of leaderd started in test gets its own
  // peerd instance.
  std::string peerd_service_name =
      cl->GetSwitchValueASCII(kPeerdServiceNameFlag);
  if (peerd_service_name.empty()) { peerd_service_name = kPeerdServiceName; }
  // And each instance needs to register handlers on a separate port to avoid
  // conflicting with the other instances.
  std::string web_handler_name =
      cl->GetSwitchValueASCII(kProtocolHandlerNameFlag);
  if (web_handler_name.empty()) {
    web_handler_name = libwebserv::ProtocolHandler::kHttp;
  }
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  Daemon daemon(leaderd_service_name, peerd_service_name, web_handler_name);
  return daemon.Run();
}
