// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MODEM_INFO_
#define SHILL_MOCK_MODEM_INFO_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/mock_control.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_pending_activation_store.h"
#include "shill/modem_info.h"

namespace shill {

class MockModemInfo : public ModemInfo {
 public:
  MockModemInfo();

  // All NULL parameters are replaced by mock objects.
  MockModemInfo(ControlInterface *control,
                EventDispatcher *dispatcher,
                Metrics *metrics,
                Manager *manager,
                GLib *glib);

  virtual ~MockModemInfo();

  // Replaces data members in ModemInfo by mock objects.
  // The following are relaced by mocks if they are NULL: control_interface,
  // dispatcher, metrics, manager, glib.
  // The following are always replaced by mocks: pending_activation_store.
  void SetMockMembers();

  // Accessors for mock objects
  MockPendingActivationStore* mock_pending_activation_store() const {
    return mock_pending_activation_store_;
  }
  MockControl* mock_control_interface() const {
    return mock_control_.get();
  }
  MockEventDispatcher* mock_dispatcher() const {
    return mock_dispatcher_.get();
  }
  MockMetrics* mock_metrics() const {
    return mock_metrics_.get();
  }
  MockManager* mock_manager() const {
    return mock_manager_.get();
  }
  MockGLib* mock_glib() const {
    return mock_glib_.get();
  }

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(OnDeviceInfoAvailable, void(const std::string &link_name));

 private:
  scoped_ptr<MockControl> mock_control_;
  scoped_ptr<MockEventDispatcher> mock_dispatcher_;
  scoped_ptr<MockMetrics> mock_metrics_;
  scoped_ptr<MockManager> mock_manager_;
  scoped_ptr<MockGLib> mock_glib_;

  // owned by ModemInfo
  MockPendingActivationStore *mock_pending_activation_store_;

  DISALLOW_COPY_AND_ASSIGN(MockModemInfo);
};

}  // namespace shill

#endif  // SHILL_MOCK_MODEM_INFO_
