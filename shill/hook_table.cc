// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/hook_table.h"

#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/cancelable_callback.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"

using base::Bind;
using base::Callback;
using base::Closure;
using base::ConstRef;
using base::Unretained;
using std::string;

namespace shill {

HookTable::HookTable(EventDispatcher *event_dispatcher)
    : event_dispatcher_(event_dispatcher) {}

void HookTable::Add(const string &name, const base::Closure &start) {
  Remove(name);
  hook_table_.insert(
      HookTableMap::value_type(name, HookAction(start)));
}

HookTable::~HookTable() {
  timeout_cb_.Cancel();
}

void HookTable::Remove(const std::string &name) {
  HookTableMap::iterator it = hook_table_.find(name);
  if (it != hook_table_.end()) {
    hook_table_.erase(it);
  }
}

void HookTable::ActionComplete(const std::string &name) {
  HookTableMap::iterator it = hook_table_.find(name);
  if (it != hook_table_.end()) {
    HookAction *action = &it->second;
    if (action->started && !action->completed) {
      action->completed = true;
    }
  }
  if (AllActionsComplete() && !done_cb_.is_null()) {
    timeout_cb_.Cancel();
    done_cb_.Run(Error(Error::kSuccess));
    done_cb_.Reset();
  }
}

void HookTable::Run(int timeout_ms,
                    const Callback<void(const Error &)> &done) {
  if (hook_table_.empty()) {
    done.Run(Error(Error::kSuccess));
    return;
  }
  done_cb_ = done;
  timeout_cb_.Reset(Bind(&HookTable::ActionsTimedOut, Unretained(this)));
  event_dispatcher_->PostDelayedTask(timeout_cb_.callback(), timeout_ms);

  // Mark all actions as having started before we execute any actions.
  // Otherwise, if the first action completes inline, its call to
  // ActionComplete() will cause the |done| callback to be invoked before the
  // rest of the actions get started.
  for (HookTableMap::iterator it = hook_table_.begin();
       it != hook_table_.end(); ++it) {
    HookAction *action = &it->second;
    action->started = true;
    action->completed = false;
  }
  // Now start the actions.
  for (HookTableMap::iterator it = hook_table_.begin();
       it != hook_table_.end(); ++it) {
    it->second.start.Run();
  }
}

bool HookTable::AllActionsComplete() {
  for (HookTableMap::const_iterator it = hook_table_.begin();
       it != hook_table_.end(); ++it) {
    const HookAction &action = it->second;
    if (action.started && !action.completed) {
        return false;
    }
  }
  return true;
}

void HookTable::ActionsTimedOut() {
  done_cb_.Run(Error(Error::kOperationTimeout));
}

}  // namespace shill
