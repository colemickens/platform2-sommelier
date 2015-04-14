// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/rule_engine.h"

#include <libudev.h>
#include <poll.h>
#include <sys/inotify.h>

#include "base/logging.h"
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
}

void RuleEngine::AddRule(Rule* rule) {
  CHECK(rule) << "Cannot add NULL as a rule.";
  rules_.push_back(std::unique_ptr<Rule>(rule));
}

Rule::Result RuleEngine::ProcessPath(const std::string& path) {
  WaitForEmptyUdevQueue();

  LOG(INFO) << "ProcessPath(" << path << ")";
  Rule::Result result = Rule::IGNORE;

  ScopedUdevDevicePtr device(FindUdevDevice(path));
  if (device.get()) {
    for (const std::unique_ptr<Rule>& rule : rules_) {
      Rule::Result rule_result = rule->ProcessDevice(device.get());
      LOG(INFO) << "  " << rule->name() << ": "
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
  } else {
    LOG(INFO) << "No udev entry found for " << path << ", denying access.";
    result = Rule::DENY;
  }

  LOG(INFO) << "Verdict for " << path << ": " << Rule::ResultToString(result);
  return result;
}

void RuleEngine::WaitForEmptyUdevQueue() {
  ScopedUdevQueuePtr queue(udev_queue_new(udev_.get()));
  if (udev_queue_get_queue_is_empty(queue.get()))
    return;

  struct pollfd udev_poll;
  memset(&udev_poll, 0, sizeof(udev_poll));
  udev_poll.fd = udev_queue_get_fd(queue.get());
  udev_poll.events = POLLIN;

  if (udev_poll.fd < 0) {
    LOG(ERROR) << "Failed to get udev queue fd: "
               << logging::SystemErrorCodeToString(-udev_poll.fd);
    return;
  }

  while (!udev_queue_get_queue_is_empty(queue.get())) {
    if (poll(&udev_poll, 1, poll_interval_msecs_) > 0)
      udev_queue_flush(queue.get());
  }
}

ScopedUdevDevicePtr RuleEngine::FindUdevDevice(const std::string& path) {
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev_.get()));
  udev_enumerate_scan_devices(enumerate.get());

  struct udev_list_entry* entry = NULL;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char* syspath = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr device(
        udev_device_new_from_syspath(udev_.get(), syspath));

    const char* devnode = udev_device_get_devnode(device.get());
    if (devnode && !strcmp(devnode, path.c_str()))
      return device.Pass();
  }

  return nullptr;
}

}  // namespace permission_broker
