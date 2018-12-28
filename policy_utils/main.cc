// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/command_line.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>

#include "policy_utils/policy_tool.h"

namespace {

// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "\n"
    "Device Policy tool\n"
    "Set or clear device policies on the local device. Setting a local\n"
    "policy overrides the policy set in Chrome. The command format is:\n"
    "\n"
    "    policy [set|clear] PolicyName [value]\n"
    "\n"
    "Examples:\n"
    "    policy set DeviceAllowBluetooth true\n"
    "    policy clear DeviceAllowBluetooth";

const char kUsageMessage[] =
    "\n"
    "Usage:\n"
    "    policy [set|clear] PolicyName [value]\n"
    "or\n"
    "    policy --help for more detailed help\n";

const char kPolicyListHeader[] =
    "\n"
    "List of available policies:\n";

// Return a PolicyTool singleton.
const policy_utils::PolicyTool& GetPolicyTool() {
  static policy_utils::PolicyTool policy_tool;
  return policy_tool;
}

// Show a list of all policies this tool can edit.
void ListPolicies() {
  const policy_utils::PolicyTool::PolicyList& policies =
      GetPolicyTool().get_policies();
  std::string name_list;
  for (auto& policy : policies) {
    name_list += "  " + policy + "\n";
  }

  LOG(INFO) << kPolicyListHeader << name_list;
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_bool(list, false,
              "Show the list of policies this tool can set or clear");

  brillo::FlagHelper::Init(argc, argv, kHelpMessage);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (FLAGS_list) {
    ListPolicies();
    return 0;
  }

  const base::CommandLine::StringVector& args = cl->GetArgs();
  if (args.size() < 2) {
    LOG(INFO) << kUsageMessage;
    return 1;
  }

  if (!GetPolicyTool().DoCommand(args)) {
    LOG(INFO) << "Failed";
    return 1;
  }

  LOG(INFO) << "Done";
  return 0;
}
