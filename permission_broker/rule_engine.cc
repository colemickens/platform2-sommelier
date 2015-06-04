// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/rule_engine.h"

#include <libudev.h>
#include <poll.h>
#include <sys/inotify.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "permission_broker/rule.h"

namespace permission_broker {

RuleEngine::RuleEngine()
    : udev_(udev_new()) {}

RuleEngine::RuleEngine(
    const std::string& udev_run_path,
    int poll_interval_msecs)
    : udev_(udev_new()) {
  CHECK(udev_) << "Could not create udev context, is sysfs mounted?";

  poll_interval_msecs_ = poll_interval_msecs;
  udev_run_path_ = udev_run_path;
}

RuleEngine::~RuleEngine() {
  STLDeleteContainerPointers(rules_.begin(), rules_.end());
  udev_unref(udev_);
}

void RuleEngine::AddRule(Rule* rule) {
  CHECK(rule) << "Cannot add NULL as a rule.";
  rules_.push_back(rule);
}

Rule::Result RuleEngine::ProcessPath(const std::string& path) {
  WaitForEmptyUdevQueue();

  LOG(INFO) << "ProcessPath(" << path << ")";
  Rule::Result result = Rule::IGNORE;
  for (unsigned int i = 0; i < rules_.size(); ++i) {
    const Rule::Result rule_result = rules_[i]->Process(path);
    LOG(INFO) << "  " << rules_[i]->name() << ": "
              << Rule::ResultToString(rule_result);
    if (rule_result == Rule::DENY) {
      result = Rule::DENY;
      break;
    } else if (rule_result == Rule::ALLOW_WITH_LOCKDOWN) {
      result = Rule::ALLOW_WITH_LOCKDOWN;
    } else if (rule_result == Rule::ALLOW &&
               result != Rule::ALLOW_WITH_LOCKDOWN) {
      result = Rule::ALLOW;
    }
  }
  LOG(INFO) << "Verdict for " << path << ": " << Rule::ResultToString(result);
  return result;
}

void RuleEngine::WaitForEmptyUdevQueue() {
  struct udev_queue* queue = udev_queue_new(udev_);
  if (udev_queue_get_queue_is_empty(queue)) {
    udev_queue_unref(queue);
    return;
  }

  struct pollfd udev_poll;
  memset(&udev_poll, 0, sizeof(udev_poll));
  udev_poll.fd = inotify_init();
  udev_poll.events = POLLIN;

  int watch =
      inotify_add_watch(udev_poll.fd, udev_run_path_.c_str(), IN_MOVED_TO);
  CHECK_NE(watch, -1) << "Could not add watch for udev run directory.";

  while (!udev_queue_get_queue_is_empty(queue)) {
    if (poll(&udev_poll, 1, poll_interval_msecs_) > 0) {
      char buffer[sizeof(struct inotify_event)];
      const ssize_t result = read(udev_poll.fd, buffer, sizeof(buffer));
      if (result < 0)
        LOG(WARNING) << "Did not read complete udev event.";
    }
  }
  udev_queue_unref(queue);
  close(udev_poll.fd);
}

}  // namespace permission_broker
