// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_queue.h"

#include <base/bind.h>
#include <base/time/time.h>

namespace buffet {

namespace {
const int kRemoveCommandDelayMin = 5;
}

void CommandQueue::AddOnCommandAddedCallback(const Callback& callback) {
  on_command_added_.push_back(callback);
  // Send all pre-existed commands.
  for (const auto& command : map_)
    callback.Run(command.second.get());
}

void CommandQueue::AddOnCommandRemovedCallback(const Callback& callback) {
  on_command_removed_.push_back(callback);
}

void CommandQueue::Add(std::unique_ptr<CommandInstance> instance) {
  std::string id = instance->GetID();
  LOG_IF(FATAL, id.empty()) << "Command has no ID";
  instance->SetCommandQueue(this);
  auto pair = map_.insert(std::make_pair(id, std::move(instance)));
  LOG_IF(FATAL, !pair.second) << "Command with ID '" << id
                              << "' is already in the queue";
  for (const auto& cb : on_command_added_)
    cb.Run(pair.first->second.get());
  Cleanup();
}

void CommandQueue::DelayedRemove(const std::string& id) {
  auto p = map_.find(id);
  if (p == map_.end())
    return;
  remove_queue_.push(std::make_pair(
      base::Time::Now() + base::TimeDelta::FromMinutes(kRemoveCommandDelayMin),
      id));
  Cleanup();
}

bool CommandQueue::Remove(const std::string& id) {
  auto p = map_.find(id);
  if (p == map_.end())
    return false;
  std::unique_ptr<CommandInstance> instance{std::move(p->second)};
  instance->SetCommandQueue(nullptr);
  map_.erase(p);
  for (const auto& cb : on_command_removed_)
    cb.Run(instance.get());
  return true;
}

void CommandQueue::Cleanup() {
  while (!remove_queue_.empty() && remove_queue_.front().first < Now()) {
    Remove(remove_queue_.front().second);
    remove_queue_.pop();
  }
}

void CommandQueue::SetNowForTest(base::Time now) {
  test_now_ = now;
}

base::Time CommandQueue::Now() const {
  return test_now_.is_null() ? base::Time::Now() : test_now_;
}

CommandInstance* CommandQueue::Find(const std::string& id) const {
  auto p = map_.find(id);
  return (p != map_.end()) ? p->second.get() : nullptr;
}

}  // namespace buffet
