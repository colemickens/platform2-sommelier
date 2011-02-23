// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <string>
#include "shill/shill_daemon.h"
#include "shill/dbus_control.h"

using std::string;

  /*
DEFINE_string(config_dir, "",
              "Directory to read confguration settings.");
DEFINE_string(default_config_dir, "",
              "Directory to read default configuration settings (Read Only).");
  */


int main(int /* argc */, char*[] /* argv*/) {
  /*
  FilePath config_dir(FLAGS_config_dir);
  FilePath default_config_dir(FLAGS_default_config_dir.empty() ?
                              shill::Config::kShillDefaultPrefsDir :
                              FLAGS_default_config_dir);
  */
  shill::Config config; /* (config_dir, default_config_dir) */

  // TODO(pstew): This should be chosen based on config
  shill::ControlInterface *control_interface = new shill::DBusControl();

  shill::Daemon daemon(&config, control_interface);
  daemon.Run();

  return 0;
}
