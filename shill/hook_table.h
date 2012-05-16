// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_HOOK_TABLE_H_
#define SHILL_HOOK_TABLE_H_

// HookTable provides a facility for starting a set of generic actions and
// polling for their completion.  For example, on shutdown, each service gets
// disconnected.  A disconnect action may be instantaneous or it may require
// some time to complete.  Users of this facility use the Add() function to
// provide a closure for starting an action and a callback for polling its
// completion.  When an event occurs, the Run() function is called, which starts
// each action and polls for completion.  Upon completion or timeout, Run()
// calls a user-supplied callback to notify the caller of the state of actions.
//
// Usage example.  Add an action to a hook table like this:
//
//   HookTable hook_table_(&event_dispatcher);
//   Closure start_cb = Bind(&MyService::Disconnect, &my_service);
//   Callback poll_cb = Bind(&MyService::IsConnected, &my_service);
//   hook_table_.Add("MyService", start_cb, poll_cb);
//
// The code that catches an event runs the actions of the hook table like this:
//
//   Callback<void(const Error &)> done_cb =
//     Bind(Manager::OnDisconnect, &manager);
//   hook_table_.Run(kTimeout, done_cb);
//
// When |my_service| has completed its disconnect process,
// Manager::OnDisconnect() gets called with Error::kSuccess.  If |my_service|
// does not finish its disconnect processing before kTimeout, then it gets
// called with kOperationTimeout.

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/callback.h>
#include <gtest/gtest_prod.h>

namespace shill {
class Error;
class EventDispatcher;

class HookTable {
 public:
  explicit HookTable(EventDispatcher *event_dispatcher);

  // Adds a closure to the hook table.  |name| should be unique; otherwise, a
  // previous closure by the same name will NOT be replaced.  |start| will be
  // called when Run() is called.  Run() will poll to see if the action has
  // completed by calling |poll|, which should return true when the action has
  // completed.
  void Add(const std::string &name, const base::Closure &start,
           const base::Callback<bool()> &poll);

  // Runs the actions that have been added to the HookTable via Add().  It polls
  // for completion up to |timeout_seconds|.  If all actions complete within the
  // timeout period, |done| is called with a value of Error::kSuccess.
  // Otherwise, it is called with Error::kOperationTimeout.
  void Run(int timeout_seconds,
           const base::Callback<void(const Error &)> &done);

 private:
  FRIEND_TEST(HookTableTest, ActionTimesOut);
  FRIEND_TEST(HookTableTest, DelayedAction);
  FRIEND_TEST(HookTableTest, MultipleActionsAllSucceed);
  FRIEND_TEST(HookTableTest, MultipleActionsAndOneTimesOut);

  // The |timeout_seconds| passed to Run() is divided into |kPollIterations|,
  // polled once iteration.
  static const int kPollIterations;

  // For each action, there is a |start| and a |poll| callback, which are stored
  // in this structure.
  struct HookCallbacks {
    HookCallbacks(const base::Closure &s, const base::Callback<bool()> &p)
        : start(s), poll(p) {}
    const base::Closure start;
    const base::Callback<bool()> poll;
  };

  // Each action is stored in this table.  The key is |name| passed to Add().
  typedef std::map<std::string, HookCallbacks> HookTableMap;

  // Calls all the |poll| callbacks in |hook_table_|.  If all of them return
  // true, then |done| is called with Error::kSuccess.  Otherwise, it queues
  // itself in the |event_dispatcher_| to be called again later.  It repeats
  // this process up to |kPollIterations|, after which if an action still has
  // not completed, |done| is called with Error::kOperationTimeout.
  void PollActions(int timeout_seconds,
                   const base::Callback<void(const Error &)> &done);

  // Each action is stored in this table.
  HookTableMap hook_table_;

  // Used for polling actions that do not complete immediately.
  EventDispatcher *const event_dispatcher_;

  // Counts the number of polling attempts.
  int iteration_counter_;

  DISALLOW_COPY_AND_ASSIGN(HookTable);
};

}  // namespace shill

#endif  // SHILL_HOOK_TABLE_H_
