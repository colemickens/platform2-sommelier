// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_RULE_ENGINE_H_
#define PERMISSION_BROKER_RULE_ENGINE_H_

#include <libudev.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>

namespace permission_broker {

class Rule;

class RuleEngine {
 public:
  RuleEngine(const std::string& access_group,
             const std::string& udev_run_path,
             int poll_interval_msecs);
  virtual ~RuleEngine();

  // Adds |rule| to the end of the existing rule chain. Takes ownership of
  // |rule|.
  void AddRule(Rule* rule);

  // Invokes each of the rules in order on |path| until either a rule explicitly
  // denies access to the path or until there are no more rules left. If, after
  // executing all of the stored rules, no rule has explicitly allowed access to
  // the path then access is denied. If _any_ rule denies access to |path| then
  // processing the rules is aborted early and access is denied.
  bool ProcessPath(const std::string& path, int interface_id);

 protected:
  // This constructor is for use by test code only.
  explicit RuleEngine(const gid_t access_group);

 private:
  friend class RuleEngineTest;

  // Waits for all queued udev events to complete before returning. Is
  // equivalent to invoking 'udevadm settle', but without the external
  // dependency and overhead.
  virtual void WaitForEmptyUdevQueue();

  // Grants access to |path|, which is accomplished by changing the owning group
  // on the path to the one specified numerically by the 'access_group' flag.
  virtual bool GrantAccess(const std::string& path);

  struct udev* udev_;
  gid_t access_group_;
  std::vector<Rule*> rules_;

  int poll_interval_msecs_;
  std::string udev_run_path_;

  DISALLOW_COPY_AND_ASSIGN(RuleEngine);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_RULE_ENGINE_H_
