// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>

#include <base/command_line.h>
#include <brillo/syslog_logging.h>

#include "usb_bouncer/entry_manager.h"
#include "usb_bouncer/util.h"

using usb_bouncer::EntryManager;

namespace {

void PrintUsage() {
  printf(R"(Usage:
  help - prints this help message.
  cleanup - removes stale whitelist entries.
  genrules - writes the generated rules configuration and to stdout.
  udev (add|remove) <devpath> - handles a udev device event.
  userlogin - add current entries to user whitelist.
)");
}

EntryManager* GetEntryManagerOrDie() {
  EntryManager* entry_manager = EntryManager::GetInstance();
  if (!entry_manager) {
    LOG(FATAL) << "EntryManager::GetInstance() failed!";
  }
  return entry_manager;
}

int HandleCleanup(const std::vector<std::string>& argv) {
  if (!argv.empty()) {
    LOG(ERROR) << "Invalid options!";
    PrintUsage();
    return EXIT_FAILURE;
  }

  EntryManager* entry_manager = GetEntryManagerOrDie();
  if (!entry_manager->GarbageCollect()) {
    LOG(ERROR) << "cleanup failed!";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int HandleGenRules(const std::vector<std::string>& argv) {
  if (!argv.empty()) {
    LOG(ERROR) << "Invalid options!";
    PrintUsage();
    return EXIT_FAILURE;
  }

  EntryManager* entry_manager = GetEntryManagerOrDie();
  std::string rules = entry_manager->GenerateRules();
  if (rules.empty()) {
    LOG(ERROR) << "genrules failed!";
    return EXIT_FAILURE;
  }

  printf("%s", rules.c_str());
  return EXIT_SUCCESS;
}

int HandleUdev(const std::vector<std::string>& argv) {
  if (argv.size() != 2) {
    LOG(ERROR) << "Invalid options!";
    PrintUsage();
    return EXIT_FAILURE;
  }

  EntryManager::UdevAction action;
  if (argv[0] == "add") {
    action = EntryManager::UdevAction::kAdd;
  } else if (argv[0] == "remove") {
    action = EntryManager::UdevAction::kRemove;
  } else {
    LOG(ERROR) << "Invalid options!";
    PrintUsage();
    return EXIT_FAILURE;
  }

  EntryManager* entry_manager = GetEntryManagerOrDie();
  if (!entry_manager->HandleUdev(action, argv[1])) {
    LOG(ERROR) << "udev failed!";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int HandleUserLogin(const std::vector<std::string>& argv) {
  if (!argv.empty()) {
    LOG(ERROR) << "Invalid options!";
    PrintUsage();
    return EXIT_FAILURE;
  }

  EntryManager* entry_manager = GetEntryManagerOrDie();
  if (!entry_manager->HandleUserLogin()) {
    LOG(ERROR) << "userlogin failed!";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToStderr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const auto& args = cl->argv();

  if (args.size() < 2) {
    LOG(ERROR) << "Invalid options!";
    PrintUsage();
    return EXIT_FAILURE;
  }

  const auto& command = args[1];
  auto command_args_start = args.begin() + 2;

  const struct {
    const std::string command;
    int (*handler)(const std::vector<std::string>& argv);
  } command_handlers[] = {
      {"cleanup", HandleCleanup},
      {"genrules", HandleGenRules},
      {"udev", HandleUdev},
      {"userlogin", HandleUserLogin},
  };

  for (const auto& command_handler : command_handlers) {
    if (command_handler.command == command) {
      return command_handler.handler(
          std::vector<std::string>(command_args_start, args.end()));
    }
  }

  if (command != "help") {
    LOG(ERROR) << "Invalid options!";
  }
  PrintUsage();
  return EXIT_FAILURE;
}
