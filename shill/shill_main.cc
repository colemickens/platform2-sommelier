// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <glib-unix.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>

#include "shill/dbus_control.h"
#include "shill/error.h"
#include "shill/glib_io_handler_factory.h"
#include "shill/logging.h"
#include "shill/net/io_handler_factory_container.h"
#include "shill/shared_dbus_connection.h"
#include "shill/shill_config.h"
#include "shill/shill_daemon.h"

using base::FilePath;
using std::string;
using std::vector;

namespace switches {

// Don't daemon()ize; run in foreground.
static const char kForeground[] = "foreground";
// Don't attempt to manage these devices.
static const char kDeviceBlackList[] = "device-black-list";
// Ignore Ethernet-like devices that don't have any driver information.
static const char kIgnoreUnknownEthernet[] = "ignore-unknown-ethernet";
// Technologies to enable for portal check at startup.
static const char kPortalList[] = "portal-list";
// When in passive mode, Shill will not manage any devices by default.
// Remote service can instruct Shill to manage/unmanage devices through
// org.chromium.flimflam.Manager's ClaimInterface/ReleaseInterface APIs.
static const char kPassiveMode[] = "passive-mode";
// Default priority order of the technologies.
static const char kDefaultTechnologyOrder[] = "default-technology-order";
// Comma-separated list of DNS servers to prepend to the resolver list.
static const char kPrependDNSServers[] = "prepend-dns-servers";
// The minimum MTU value that will be respected in DHCP responses.
static const char kMinimumMTU[] = "minimum-mtu";
// Accept hostname from the DHCP server for the specified devices.
// eg. eth0 or eth*
static const char kAcceptHostnameFrom[] = "accept-hostname-from";
// Flag that causes shill to show the help message and exit.
static const char kHelp[] = "help";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Available Switches: \n"
    "  --foreground\n"
    "    Don\'t daemon()ize; run in foreground.\n"
    "  --device-black-list=device1,device2\n"
    "    Do not manage devices named device1 or device2\n"
    "  --ignore-unknown-ethernet\n"
    "    Ignore Ethernet-like devices that do not report a driver\n"
    "  --log-level=N\n"
    "    Logging level:\n"
    "      0 = LOG(INFO), 1 = LOG(WARNING), 2 = LOG(ERROR),\n"
    "      -1 = SLOG(..., 1), -2 = SLOG(..., 2), etc.\n"
    "  --log-scopes=\"*scope1+scope2\".\n"
    "    Scopes to enable for SLOG()-based logging.\n"
    "  --portal-list=technology1,technology2\n"
    "    Specify technologies to perform portal detection on at startup.\n"
    "  --passive-mode\n"
    "    Do not manage any devices by default\n"
    "  --default-technology-order=technology1,technology2\n"
    "    Specify the default priority order of the technologies.\n"
    "  --prepend-dns-servers=server1,server2,...\n"
    "    Prepend the provided DNS servers to the resolver list.\n"
    "  --accept-hostname-from=eth0 or --accept-hostname-from=eth*\n"
    "    Accept a hostname from the DHCP server for the matching devices.\n"
    "  --minimum-mtu=mtu\n"
    "    Set the minimum value to respect as the MTU from DHCP responses.\n";
}  // namespace switches

namespace {

const char *kLoggerCommand = "/usr/bin/logger";
const char *kLoggerUser = "syslog";

}  // namespace

// Always logs to the syslog and logs to stderr if
// we are running in the foreground.
void SetupLogging(bool foreground, char *daemon_name) {
  int log_flags = 0;
  log_flags |= chromeos::kLogToSyslog;
  log_flags |= chromeos::kLogHeader;
  if (foreground) {
    log_flags |= chromeos::kLogToStderr;
  }
  chromeos::InitLog(log_flags);

  if (!foreground) {
    vector<char *> logger_command_line;
    int logger_stdin_fd;
    logger_command_line.push_back(const_cast<char *>(kLoggerCommand));
    logger_command_line.push_back(const_cast<char *>("--priority"));
    logger_command_line.push_back(const_cast<char *>("daemon.err"));
    logger_command_line.push_back(const_cast<char *>("--tag"));
    logger_command_line.push_back(daemon_name);
    logger_command_line.push_back(nullptr);

    chromeos::Minijail *minijail = chromeos::Minijail::GetInstance();
    struct minijail *jail = minijail->New();
    minijail->DropRoot(jail, kLoggerUser, kLoggerUser);

    if (!minijail->RunPipeAndDestroy(jail, logger_command_line, nullptr,
                                     &logger_stdin_fd)) {
      LOG(ERROR) << "Unable to spawn logger. "
                 << "Writes to stderr will be discarded.";
      return;
    }

    // Note that we don't set O_CLOEXEC here. This means that stderr
    // from any child processes will, by default, be logged to syslog.
    if (dup2(logger_stdin_fd, fileno(stderr)) != fileno(stderr)) {
      PLOG(ERROR) << "Failed to redirect stderr to syslog";
    }
    close(logger_stdin_fd);
  }
}

gboolean ExitSigHandler(gpointer data) {
  LOG(INFO) << "Shutting down due to received signal.";

  shill::Daemon* daemon = reinterpret_cast<shill::Daemon*>(data);
  daemon->Quit();
  return TRUE;
}


int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  const int nochdir = 0, noclose = 0;
  if (!cl->HasSwitch(switches::kForeground))
    PLOG_IF(FATAL, daemon(nochdir, noclose) == -1) << "Failed to daemonize";

  SetupLogging(cl->HasSwitch(switches::kForeground), argv[0]);
  shill::SetLogLevelFromCommandLine(cl);

  // Overwrite default IOHandlerFactory with the glib version of it. This needs
  // to be placed before any reference to the IOHandlerFactory.
  shill::IOHandlerFactoryContainer::GetInstance()->SetIOHandlerFactory(
        new shill::GlibIOHandlerFactory());

  // TODO(pstew): This should be chosen based on config
  shill::SharedDBusConnection::GetInstance()->Init();
  shill::DBusControl* dbus_control = new shill::DBusControl();
  dbus_control->Init();

  vector<shill::Technology::Identifier> technology_order;
  if (cl->HasSwitch(switches::kDefaultTechnologyOrder)) {
    shill::Error error;
    string order_flag = cl->GetSwitchValueASCII(
        switches::kDefaultTechnologyOrder);
    if (!shill::Technology::GetTechnologyVectorFromString(
        order_flag, &technology_order, &error)) {
      LOG(ERROR) << "Invalid default technology order: [" << order_flag
                 << "] Error: " << error.message();
    }
  }
  if (technology_order.empty()) {
    technology_order.push_back(
        shill::Technology::IdentifierFromName(shill::kTypeVPN));
    technology_order.push_back(
        shill::Technology::IdentifierFromName(shill::kTypeEthernet));
    technology_order.push_back(
        shill::Technology::IdentifierFromName(shill::kTypeWifi));
    technology_order.push_back(
        shill::Technology::IdentifierFromName(shill::kTypeWimax));
    technology_order.push_back(
        shill::Technology::IdentifierFromName(shill::kTypeCellular));
  }

  shill::Config config;

  // Passes ownership of dbus_control.
  shill::Daemon daemon(&config, dbus_control, technology_order);

  if (cl->HasSwitch(switches::kDeviceBlackList)) {
    vector<string> device_list;
    base::SplitString(cl->GetSwitchValueASCII(switches::kDeviceBlackList),
                      ',', &device_list);

    for (const auto &device : device_list) {
      daemon.AddDeviceToBlackList(device);
    }
  }

  if (cl->HasSwitch(switches::kIgnoreUnknownEthernet)) {
    daemon.SetIgnoreUnknownEthernet(true);
  }

  if (cl->HasSwitch(switches::kPortalList)) {
    daemon.SetStartupPortalList(cl->GetSwitchValueASCII(switches::kPortalList));
  }

  if (cl->HasSwitch(switches::kPassiveMode)) {
    daemon.SetPassiveMode();
  }

  if (cl->HasSwitch(switches::kPrependDNSServers)) {
    daemon.SetPrependDNSServers(cl->GetSwitchValueASCII(
        switches::kPrependDNSServers));
  }

  if (cl->HasSwitch(switches::kMinimumMTU)) {
    int mtu;
    std::string value = cl->GetSwitchValueASCII(switches::kMinimumMTU);
    if (!base::StringToInt(value, &mtu)) {
      LOG(FATAL) << "Could not convert '" << value << "' to integer.";
    }
    daemon.SetMinimumMTU(mtu);
  }

  if (cl->HasSwitch(switches::kAcceptHostnameFrom)) {
    daemon.SetAcceptHostnameFrom(
        cl->GetSwitchValueASCII(switches::kAcceptHostnameFrom));
  }

  g_unix_signal_add(SIGINT, ExitSigHandler, &daemon);
  g_unix_signal_add(SIGTERM, ExitSigHandler, &daemon);

  // Catch but ignore SIGPIPE signals we receive if we write to the logger
  // process after it exits.  GLib cannot handle this signal number, so use
  // signal() directly.
  signal(SIGPIPE, SIG_IGN);

  daemon.Run();

  LOG(INFO) << "Process exiting.";

  return 0;
}
