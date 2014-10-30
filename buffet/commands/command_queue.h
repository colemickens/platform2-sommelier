// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_QUEUE_H_
#define BUFFET_COMMANDS_COMMAND_QUEUE_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>

#include "buffet/commands/command_instance.h"

namespace buffet {

class CommandDispachInterface;

class CommandQueue final {
 public:
  CommandQueue() = default;

  // Sets a command dispatch notifications for changes in command queue.
  // |dispatch_interface| must outlive the CommandQueue object instance
  // or be nullptr.
  void SetCommandDispachInterface(CommandDispachInterface* dispatch_interface) {
    dispatch_interface_ = dispatch_interface;
  }

  // Checks if the command queue is empty.
  bool IsEmpty() const { return map_.empty(); }

  // Returns the number of commands in the queue.
  size_t GetCount() const { return map_.size(); }

  // Adds a new command to the queue. Each command in the queue has a unique
  // ID that identifies that command instance in this queue.
  // One shouldn't attempt to add a command with the same ID.
  void Add(std::unique_ptr<CommandInstance> instance);

  // Removes a command identified by |id| from the queue. Returns a unique
  // pointer to the command instance if removed successfully, or an empty
  // unique_ptr if the command with the given ID doesn't exist in the queue.
  std::unique_ptr<CommandInstance> Remove(const std::string& id);

  // Finds a command instance in the queue by the instance |id|. Returns
  // nullptr if the command with the given |id| is not found. The returned
  // pointer should not be persisted for a long period of time.
  CommandInstance* Find(const std::string& id) const;

 private:
  // ID-to-CommandInstance map.
  std::map<std::string, std::unique_ptr<CommandInstance>> map_;
  // Callback interface for command dispatch, if provided.
  CommandDispachInterface* dispatch_interface_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CommandQueue);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_QUEUE_H_
