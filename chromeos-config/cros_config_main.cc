// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line utility to access to the Chrome OS master configuration.

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "brillo/flag_helper.h"
#include "chromeos-config/libcros_config/cros_config.h"

int main(int argc, char* argv[]) {
  DEFINE_string(test_database,
                "",
                "Override path to system config database for testing.");
  DEFINE_string(test_model, "", "Override system model name for testing.");

  std::string usage = "Chrome OS Model Configuration\n\nUsage: " +
                      std::string(argv[0]) + " [flags] <path> <key>";
  brillo::FlagHelper::Init(argc, argv, usage);

  CHECK_EQ(FLAGS_test_database.empty(), FLAGS_test_model.empty())
      << "You must pass both --test_database and --test_model or neither.";

  brillo::CrosConfig cros_config;
  if (FLAGS_test_database.empty()) {
    if (!cros_config.InitModel()) {
      return 1;
    }
  } else {
    if (!cros_config.InitForTest(base::FilePath(FLAGS_test_database),
                                 FLAGS_test_model)) {
      return 1;
    }
  }

  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  if (args.size() != 2) {
    std::cerr << usage << "\nPass --help for more information." << std::endl;
    return 1;
  }
  std::string path = args[0];
  std::string property = args[1];

  std::string value;
  if (!cros_config.GetString(path, property, &value)) {
    return 1;
  }

  std::cout << value;
  return 0;
}
