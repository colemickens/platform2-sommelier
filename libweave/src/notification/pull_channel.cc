// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/notification/pull_channel.h"

#include <base/bind.h>

#include "libweave/src/notification/notification_delegate.h"

namespace weave {

PullChannel::PullChannel(base::TimeDelta pull_interval, TaskRunner* task_runner)
    : pull_interval_{pull_interval}, task_runner_{task_runner} {}

std::string PullChannel::GetName() const {
  return "pull";
}

bool PullChannel::IsConnected() const {
  return true;
}

void PullChannel::AddChannelParameters(base::DictionaryValue* channel_json) {
  // No extra parameters needed for "Pull" channel.
}

void PullChannel::Start(NotificationDelegate* delegate) {
  CHECK(delegate);
  delegate_ = delegate;
  RePost();
}

void PullChannel::RePost() {
  CHECK(delegate_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PullChannel::OnTimer, weak_ptr_factory_.GetWeakPtr()),
      pull_interval_);
}

void PullChannel::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  delegate_ = nullptr;
}

void PullChannel::UpdatePullInterval(base::TimeDelta pull_interval) {
  pull_interval_ = pull_interval;
  if (delegate_)
    RePost();
}

void PullChannel::OnTimer() {
  // Repost before delegate notification to give it a chance to stop channel.
  RePost();
  base::DictionaryValue empty_dict;
  delegate_->OnCommandCreated(empty_dict);
}

}  // namespace weave
