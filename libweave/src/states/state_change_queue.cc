// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/states/state_change_queue.h"

#include <base/logging.h>

namespace weave {

StateChangeQueue::StateChangeQueue(size_t max_queue_size)
    : max_queue_size_(max_queue_size) {
  CHECK_GT(max_queue_size_, 0U) << "Max queue size must not be zero";
}

bool StateChangeQueue::NotifyPropertiesUpdated(base::Time timestamp,
                                               ValueMap changed_properties) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto& stored_changes = state_changes_[timestamp];
  // Merge the old property set.
  changed_properties.insert(stored_changes.begin(), stored_changes.end());
  stored_changes = std::move(changed_properties);

  while (state_changes_.size() > max_queue_size_) {
    // Queue is full.
    // Merge the two oldest records into one. The merge strategy is:
    //  - Move non-existent properties from element [old] to [new].
    //  - If both [old] and [new] specify the same property,
    //    keep the value of [new].
    //  - Keep the timestamp of [new].
    auto element_old = state_changes_.begin();
    auto element_new = std::next(element_old);
    // This will skip elements that exist in both [old] and [new].
    element_new->second.insert(element_old->second.begin(),
                               element_old->second.end());
    state_changes_.erase(element_old);
  }
  ++last_change_id_;
  return true;
}

std::vector<StateChange> StateChangeQueue::GetAndClearRecordedStateChanges() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<StateChange> changes;
  changes.reserve(state_changes_.size());
  for (const auto& pair : state_changes_) {
    changes.emplace_back(pair.first, std::move(pair.second));
  }
  state_changes_.clear();
  return changes;
}

StateChangeQueueInterface::Token StateChangeQueue::AddOnStateUpdatedCallback(
    const base::Callback<void(UpdateID)>& callback) {
  if (state_changes_.empty())
    callback.Run(last_change_id_);
  return Token{callbacks_.Add(callback).release()};
}

void StateChangeQueue::NotifyStateUpdatedOnServer(UpdateID update_id) {
  callbacks_.Notify(update_id);
}

}  // namespace weave
