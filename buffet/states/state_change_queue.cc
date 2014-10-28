// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "buffet/states/state_change_queue.h"

namespace buffet {

StateChangeQueue::StateChangeQueue(size_t max_queue_size)
    : max_queue_size_(max_queue_size) {
  CHECK_GT(max_queue_size_, 0) << "Max queue size must not be zero";
}

bool StateChangeQueue::NotifyPropertiesUpdated(const StateChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_changes_.push_back(change);
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
    element_new->property_set.insert(element_old->property_set.begin(),
                                     element_old->property_set.end());
    state_changes_.erase(element_old);
  }
  return true;
}

std::vector<StateChange> StateChangeQueue::GetAndClearRecordedStateChanges() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Return the accumulated state changes and clear the current queue.
  return std::move(state_changes_);
}

}  // namespace buffet
