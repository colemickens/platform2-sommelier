// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <gtest/gtest.h>

#include <glib.h>
#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>

#include "shill/dbus_control.h"
#include "shill/device_info.h"
#include "shill/wifi.h"

namespace switches {
// wi-fi device name
static const char kDeviceName[] = "device-name";
// Flag that causes shill to show the help message and exit.
static const char kHelp[] = "help";
// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] = "\n"
    "Switches for " __FILE__ "\n"
    "  --device-name\n"
    "    name of wi-fi device (e.g. wlan0).\n";
}  // namespace switches

namespace shill {
using ::testing::Test;

const int kInterfaceIndexUnknown = -1;
const unsigned int kScanTimeoutSecs = 60;
const char kDefaultDeviceName[] = "wlan0";
DBus::Glib::BusDispatcher dbus_glib_dispatcher;
std::string device_name;

class WiFiTest : public Test {
 public:
  WiFiTest() : timed_out_(false) {
    wifi_ = new WiFi(&dbus_control_, NULL, NULL, device_name,
                     kInterfaceIndexUnknown);
  }

  bool ScanPending() {
    return wifi_->scan_pending_;
  }

  void TimeOut() {
    timed_out_ = true;
  }

  ~WiFiTest() {
    wifi_->Release();
  }

  static gboolean TimeoutHandler(void *test_instance) {
    static_cast<WiFiTest *>(test_instance)->TimeOut();
    return false;
  }

 protected:
  DBusControl dbus_control_;
  WiFi *wifi_;
  bool timed_out_;
};


TEST_F(WiFiTest, SSIDScanning) {
  wifi_->Start();
  g_timeout_add_seconds(10, WiFiTest::TimeoutHandler, this);

  // Crank the glib main loop
  while (ScanPending() && !timed_out_) {
    LOG(INFO) << "cranking event loop";
    g_main_context_iteration(NULL, TRUE);
  }

  ASSERT_FALSE(timed_out_);
}

}  // namespace shill

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  ::testing::InitGoogleTest(&argc, argv);
  CommandLine::Init(argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    // NB(quiche): google test prints test framework help message
    // at InitGoogleTest, above.
    std::cout << switches::kHelpMessage;
    return 0;
  }

  if (cl->HasSwitch(switches::kDeviceName)) {
    shill::device_name =
        cl->GetSwitchValueASCII(switches::kDeviceName);
  } else {
    shill::device_name = shill::kDefaultDeviceName;
  }

  shill::dbus_glib_dispatcher.attach(NULL);  // NULL => default context
  DBus::default_dispatcher = &shill::dbus_glib_dispatcher;
  return RUN_ALL_TESTS();
}
