// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/mock_firewall.h"

#include <algorithm>

namespace permission_broker {

int MockFirewall::SetRunInMinijailFailCriterion(
    const std::vector<std::string>& keywords, bool repeat,
    bool omit_failure) {
  match_criteria_.push_back(Criterion{keywords, repeat, omit_failure, 0});
  return match_criteria_.size() - 1;
}

int MockFirewall::GetRunInMinijailCriterionMatchCount(int id) {
  if (id < 0 || id >= static_cast<int>(match_criteria_.size()))
    return -1;
  return match_criteria_[id].match_count;
}

bool MockFirewall::MatchAndUpdate(const std::vector<std::string>& argv) {
  for (auto& criterion : match_criteria_) {
    bool match = true;
    // Empty criterion is a catch all -- fail on any RunInMinijail.
    for (const std::string& keyword : criterion.keywords) {
      match = match &&
        (std::find(argv.begin(), argv.end(), keyword) != argv.end());
    }
    if (match) {
      criterion.match_count++;
      if (!criterion.repeat) {
        match_criteria_.erase(std::remove(match_criteria_.begin(),
                                          match_criteria_.end(),
                                          criterion),
                              match_criteria_.end());
      }
      // If not negating, treat as fail criterion,
      // otherwise ignore (return false).
      return !criterion.omit_failure;
    }
  }
  return false;
}

int MockFirewall::RunInMinijail(const std::vector<std::string>& argv) {
  if (MatchAndUpdate(argv))
    return 1;
  commands_.push_back(argv);
  return 0;
}

std::vector<std::string> MockFirewall::GetInverseCommand(
    const std::vector<std::string>& argv) {
  std::vector<std::string> inverse;
  if (argv.size() == 0) {
    return inverse;
  }

  bool isIpTablesCommand = argv[0] == kIpTablesPath;
  for (const auto& arg : argv) {
    if (arg == "-A" && isIpTablesCommand)
      inverse.push_back("-D");
    else if (arg == "-D" && isIpTablesCommand)
      inverse.push_back("-A");
    else
      inverse.push_back(arg);
  }
  return inverse;
}

// This check does not enforce ordering. It only checks
// that for each command that adds a rule/mark with
// ip/iptables, there is a later match command that
// deletes that same rule/mark.
bool MockFirewall::CheckCommandsUndone() {
  for (std::vector<std::vector<std::string>>::iterator it = commands_.begin();
       it != commands_.end(); it++) {
    // For each command that adds a rule, check that its inverse
    // exists later in the log of commands.
    if ((std::find(it->begin(), it->end(), "-A") != it->end()) ||
        (std::find(it->begin(), it->end(), "add") != it->end())) {
      bool found = false;
      std::vector<std::string> inverse = GetInverseCommand(*it);
      // If GetInverseCommand returns the same command, then it was
      // not an ip/iptables command that added/removed a rule/mark.
      if (std::equal(it->begin(), it->end(), inverse.begin()))
        continue;
      for (auto it_next = it + 1; it_next != commands_.end(); it_next++) {
        if (*it_next == inverse) {
          found = true;
          break;
        }
      }
      if (!found)
        return false;
    }
  }
  return true;
}

}  // namespace permission_broker
