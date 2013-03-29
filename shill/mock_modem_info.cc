// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_modem_info.h"

#include <mobile_provider.h>

namespace shill {

MockModemInfo::MockModemInfo() :
    ModemInfo(NULL, NULL, NULL, NULL, NULL),
    mock_pending_activation_store_(NULL),
    mock_cellular_operator_info_(NULL) {}

MockModemInfo::MockModemInfo(ControlInterface *control,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager,
                             GLib *glib) :
    ModemInfo(control, dispatcher, metrics, manager, glib),
    mock_pending_activation_store_(NULL),
    mock_cellular_operator_info_(NULL) {
  SetMockMembers();
}

MockModemInfo::~MockModemInfo() {}

void MockModemInfo::SetMockMembers() {
  // These are always replaced by mocks.
  // Assumes ownership.
  set_pending_activation_store(new MockPendingActivationStore());
  mock_pending_activation_store_ =
      static_cast<MockPendingActivationStore*>(pending_activation_store());
  // Assumes ownership.
  set_cellular_operator_info(new MockCellularOperatorInfo());
  mock_cellular_operator_info_ =
      static_cast<MockCellularOperatorInfo*>(cellular_operator_info());

  // These are replaced by mocks only if current unset in ModemInfo.
  if(control_interface() == NULL) {
    mock_control_.reset(new MockControl());
    set_control_interface(mock_control_.get());
  }
  if(dispatcher() == NULL) {
    mock_dispatcher_.reset(new MockEventDispatcher());
    set_event_dispatcher(mock_dispatcher_.get());
  }
  if(metrics() == NULL) {
    mock_metrics_.reset(new MockMetrics(dispatcher()));
    set_metrics(mock_metrics_.get());
  }
  if(glib() == NULL) {
    mock_glib_.reset(new MockGLib());
    set_glib(mock_glib_.get());
  }
  if(manager() == NULL) {
    mock_manager_.reset(new MockManager(control_interface(), dispatcher(),
                                        metrics(), glib()));
    set_manager(mock_manager_.get());
  }
}

void MockModemInfo::SetProviderDB(const char *provider_db_path) {
  mobile_provider_db *provider_db =
      mobile_provider_open_db(provider_db_path);
  ASSERT_TRUE(provider_db);
  // Assumes ownership.
  set_mobile_provider_db(provider_db);
}

}  // namespace shill
