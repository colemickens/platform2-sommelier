// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLICY_UTILS_POLICY_TOOL_H_
#define POLICY_UTILS_POLICY_TOOL_H_

#include <string>
#include <vector>

#include <base/command_line.h>

#include "policy_utils/policy_writer.h"

namespace policy_utils {

// Utility class to parse a command, policy name and optional parameters from a
// list of cmd-line arguments and perform the desired action.
class PolicyTool {
 public:
  // PolicyList is a list of policy names.
  typedef std::vector<const std::string> PolicyList;

  // Create a PolicyTool instance that writes policy JSON files to the default
  // directory. Use this to override policies in Chrome.
  PolicyTool();
  // Create a PolicyTool instance that writes policy JSON files to the specified
  // directory. Use this for testing.
  explicit PolicyTool(const std::string& policy_dir_path);

  ~PolicyTool() = default;

  // Parse and perform the command specified by args. Return whether successful.
  bool DoCommand(const base::CommandLine::StringVector& args) const;

  // Return a list of policies this tool knows how to handle.
  static const PolicyList& get_policies();

 private:
  PolicyWriter writer_;

  DISALLOW_COPY_AND_ASSIGN(PolicyTool);
};

}  // namespace policy_utils

#endif  // POLICY_UTILS_POLICY_TOOL_H_
