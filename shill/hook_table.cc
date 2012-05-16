// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/hook_table.h"

#include <string>

#include <base/bind.h>
#include <base/callback.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"

using base::Bind;
using base::Closure;
using base::ConstRef;
using base::Unretained;
using std::string;

namespace shill {

const int HookTable::kPollIterations = 10;

HookTable::HookTable(EventDispatcher *event_dispatcher)
    : event_dispatcher_(event_dispatcher),
      iteration_counter_(0) {}

void HookTable::Add(const string &name, const base::Closure &start,
                    const base::Callback<bool()> &poll) {
  hook_table_.insert(
      HookTableMap::value_type(name, HookCallbacks(start, poll)));
}

void HookTable::Run(int timeout_seconds,
                    const base::Callback<void(const Error &)> &done) {
  // Run all the start actions in the hook table.
  for (HookTableMap::const_iterator it = hook_table_.begin();
       it != hook_table_.end(); ++it) {
    it->second.start.Run();
  }

  // Start polling for completion.
  iteration_counter_ = 0;
  PollActions(timeout_seconds, done);
}

void HookTable::PollActions(int timeout_seconds,
                            const base::Callback<void(const Error &)> &done) {
  // Call all the poll functions in the hook table.
  bool all_done = true;
  for (HookTableMap::const_iterator it = hook_table_.begin();
       it != hook_table_.end(); ++it) {
    if (!it->second.poll.Run()) {
      all_done = false;
    }
  }

  if (all_done) {
    done.Run(Error(Error::kSuccess));
    return;
  }

  if (iteration_counter_ >= kPollIterations) {
    done.Run(Error(Error::kOperationTimeout));
    return;
  }

  // Some actions have not yet completed.  Queue this function to poll again
  // later.
  Closure poll_actions_cb = Bind(&HookTable::PollActions, Unretained(this),
                                 timeout_seconds, ConstRef(done));
  const uint64 delay_ms = (timeout_seconds * 1000) / kPollIterations;
  event_dispatcher_->PostDelayedTask(poll_actions_cb, delay_ms);
  iteration_counter_++;
}

}  // namespace shill
