// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_queue.h"

#include "buffet/commands/command_dispatch_interface.h"

namespace buffet {

void CommandQueue::Add(std::unique_ptr<CommandInstance> instance) {
  std::string id = instance->GetID();
  LOG_IF(FATAL, id.empty()) << "Command has no ID";
  instance->SetCommandQueue(this);
  auto pair = map_.insert(std::make_pair(id, std::move(instance)));
  LOG_IF(FATAL, !pair.second) << "Command with ID '" << id
                              << "' is already in the queue";
  if (dispatch_interface_)
    dispatch_interface_->OnCommandAdded(pair.first->second.get());
}

std::unique_ptr<CommandInstance> CommandQueue::Remove(
    const std::string& id) {
  std::unique_ptr<CommandInstance> instance;
  auto p = map_.find(id);
  if (p != map_.end()) {
    instance = std::move(p->second);
    instance->SetCommandQueue(nullptr);
    map_.erase(p);
    if (dispatch_interface_)
      dispatch_interface_->OnCommandRemoved(instance.get());
  }
  return instance;
}

CommandInstance* CommandQueue::Find(const std::string& id) const {
  auto p = map_.find(id);
  return (p != map_.end()) ? p->second.get() : nullptr;
}

}  // namespace buffet
