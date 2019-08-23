// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <map>
#include <memory>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/any.h>
#include <brillo/daemons/dbus_daemon.h>
#include <chromeos/dbus/service_constants.h>
#include <shill/dbus-proxies.h>

namespace {

namespace switches {
static const char kHelp[] = "help";
static const char kPassphrase[] = "passphrase";
static const char kHexSsid[] = "hex-ssid";
static const char kSSID[] = "ssid";
static const char kTimeOut[] = "wait-for-online-seconds";
static const char kHelpMessage[] =
    "\n"
    "Available Switches: \n"
    "  --ssid=<ssid>\n"
    "    Set the SSID to configure (mandatory).\n"
    "  --hex-ssid\n"
    "    SSID is provided in hexadecimal\n"
    "  --passphrase=<passprhase>\n"
    "    Set the passphrase for PSK networks\n"
    "  --wait-for-online-seconds=<seconds>\n"
    "    Number of seconds to wait to connect the SSID\n";
}  // namespace switches

static const char kOnlineState[] = "online";
static const int kTimeoutBetweenStateChecksMs = 100;

}  // namespace

class MyClient : public brillo::DBusDaemon {
 public:
  MyClient(std::string ssid, bool is_hex_ssid, std::string psk, int timeout)
      : ssid_(ssid), is_hex_ssid_(is_hex_ssid), psk_(psk), timeout_(timeout) {}
  ~MyClient() override = default;

 protected:
  int OnInit() override {
    int ret = DBusDaemon::OnInit();
    if (ret != EX_OK) {
      return ret;
    }
    ConfigureAndConnect();
    // Timeout if we can't get online.
    brillo::MessageLoop::current()->PostDelayedTask(
        base::Bind(&MyClient::Quit, base::Unretained(this)),
        base::TimeDelta::FromSeconds(timeout_));
    return EX_OK;
  }

  bool ConfigureAndConnect() {
    auto shill_manager_proxy =
        std::make_unique<org::chromium::flimflam::ManagerProxy>(bus_);

    dbus::ObjectPath created_service;
    brillo::ErrorPtr configure_error;
    if (!shill_manager_proxy->ConfigureService(
            GetServiceConfig(), &created_service, &configure_error)) {
      LOG(ERROR) << "Configure service failed";
      return false;
    }

    brillo::ErrorPtr connect_error;
    shill_service_proxy_ =
        std::make_unique<org::chromium::flimflam::ServiceProxy>(
            bus_, created_service);
    if (!shill_service_proxy_->Connect(&connect_error)) {
      LOG(ERROR) << "Connect service failed";
      return false;
    }

    PostCheckWifiStatusTask();
    return true;
  }

  void PostCheckWifiStatusTask() {
    LOG(INFO) << "Sleeping now. Will check wifi status in 100 ms.";
    brillo::MessageLoop::current()->PostDelayedTask(
        base::Bind(&MyClient::QuitIfOnline, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(kTimeoutBetweenStateChecksMs));
  }

  // Check if the device is online. If it is, quit.
  void QuitIfOnline() {
    if (IsOnline())
      Quit();
    else
      PostCheckWifiStatusTask();
  }

  bool IsOnline() {
    brillo::VariantDictionary properties;
    if (!shill_service_proxy_->GetProperties(&properties, nullptr)) {
      LOG(ERROR) << "Cannot get properties.";
      PostCheckWifiStatusTask();
      return false;
    }
    auto property_it = properties.find(shill::kStateProperty);
    if (property_it == properties.end()) {
      PostCheckWifiStatusTask();
      return false;
    }

    std::string state = property_it->second.TryGet<std::string>();
    return state == kOnlineState;
  }

  std::map<std::string, brillo::Any> GetServiceConfig() {
    std::map<std::string, brillo::Any> configure_dict;
    configure_dict[shill::kTypeProperty] = shill::kTypeWifi;
    if (is_hex_ssid_) {
      configure_dict[shill::kWifiHexSsid] = ssid_;
    } else {
      configure_dict[shill::kSSIDProperty] = ssid_;
    }
    if (!psk_.empty()) {
      configure_dict[shill::kPassphraseProperty] = psk_;
      configure_dict[shill::kSecurityProperty] = shill::kSecurityPsk;
    }
    return configure_dict;
  }

  std::unique_ptr<org::chromium::flimflam::ServiceProxy> shill_service_proxy_;
  std::string ssid_;
  bool is_hex_ssid_;
  std::string psk_;
  int timeout_;
};

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return EXIT_SUCCESS;
  }

  if (!cl->HasSwitch(switches::kSSID)) {
    LOG(ERROR) << "ssid switch is mandatory.";
    LOG(ERROR) << switches::kHelpMessage;
    return EXIT_FAILURE;
  }

  std::string ssid = cl->GetSwitchValueASCII(switches::kSSID);
  std::string psk;
  if (cl->HasSwitch(switches::kPassphrase)) {
    psk = cl->GetSwitchValueASCII(switches::kPassphrase);
  }
  bool hex_ssid = cl->HasSwitch(switches::kHexSsid);

  int timeout = 0;
  if (cl->HasSwitch(switches::kTimeOut)) {
    auto value = cl->GetSwitchValueASCII(switches::kTimeOut);
    if (!base::StringToInt(value, &timeout)) {
      LOG(ERROR) << "Timeout value invalid";
      return EXIT_FAILURE;
    }
  }

  MyClient client(ssid, hex_ssid, psk, timeout);
  client.Run();
  LOG(INFO) << "Process exiting.";

  return EXIT_SUCCESS;
}
