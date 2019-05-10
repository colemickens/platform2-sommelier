// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_modem_info.h"

namespace shill {

MockModemInfo::MockModemInfo() :
    ModemInfo(nullptr, nullptr, nullptr, nullptr),
    mock_pending_activation_store_(nullptr) {}

MockModemInfo::MockModemInfo(ControlInterface* control,
                             EventDispatcher* dispatcher,
                             Metrics* metrics,
                             Manager* manager)
    : ModemInfo(control, dispatcher, metrics, manager),
      mock_pending_activation_store_(nullptr) {
  SetMockMembers();
}

MockModemInfo::~MockModemInfo() = default;

void MockModemInfo::SetMockMembers() {
  // These are always replaced by mocks.
  // Assumes ownership.
  set_pending_activation_store(new MockPendingActivationStore());
  mock_pending_activation_store_ =
      static_cast<MockPendingActivationStore*>(pending_activation_store());
  // These are replaced by mocks only if current unset in ModemInfo.
  if (control_interface() == nullptr) {
    mock_control_.reset(new MockControl());
    set_control_interface(mock_control_.get());
  }
  if (dispatcher() == nullptr) {
    mock_dispatcher_.reset(new MockEventDispatcher());
    set_event_dispatcher(mock_dispatcher_.get());
  }
  if (metrics() == nullptr) {
    mock_metrics_.reset(new MockMetrics());
    set_metrics(mock_metrics_.get());
  }
  if (manager() == nullptr) {
    mock_manager_.reset(new MockManager(control_interface(), dispatcher(),
                                        metrics()));
    set_manager(mock_manager_.get());
  }
}

}  // namespace shill
