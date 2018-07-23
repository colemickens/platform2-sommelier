// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "bluetooth/common/dbus_daemon.h"
#include "bluetooth/dispatcher/dispatcher.h"
#include "bluetooth/dispatcher/dispatcher_daemon.h"

namespace {

constexpr char kNewblueConfigFile[] = "/var/lib/bluetooth/newblue";

// True if the kernel is configured to split LE traffic.
bool IsBleSplitterEnabled() {
  std::string content;
  // LE splitter is enabled iff /var/lib/bluetooth/newblue starts with "1".
  if (base::ReadFileToString(base::FilePath(kNewblueConfigFile), &content)) {
    base::TrimWhitespaceASCII(content, base::TRIM_TRAILING, &content);
    if (content == "1")
      return true;
  }

  // Current LE splitter default = disabled.
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_string(passthrough, "",
                "Pure D-Bus forwarding to/from BlueZ or NewBlue. Valid values "
                "are \"bluez\" and \"newblue\".");

  brillo::FlagHelper::Init(argc, argv,
                           "btdispatch, the Chromium OS Bluetooth service.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  // Default passthrough mode depends on whether LE splitter is enabled.
  bluetooth::PassthroughMode passthrough_mode =
      IsBleSplitterEnabled() ? bluetooth::PassthroughMode::MULTIPLEX
                             : bluetooth::PassthroughMode::BLUEZ_ONLY;

  // Passthrough mode can be overridden by the command line flag.
  if (!FLAGS_passthrough.empty()) {
    if (FLAGS_passthrough == "bluez")
      passthrough_mode = bluetooth::PassthroughMode::BLUEZ_ONLY;
    else if (FLAGS_passthrough == "newblue")
      passthrough_mode = bluetooth::PassthroughMode::NEWBLUE_ONLY;
    else
      CHECK(false) << "--passthrough is invalid";
  }

  bluetooth::DBusDaemon daemon(
      std::make_unique<bluetooth::DispatcherDaemon>(passthrough_mode));
  return daemon.Run();
}
