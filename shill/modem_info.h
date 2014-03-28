// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_INFO_H_
#define SHILL_MODEM_INFO_H_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

struct mobile_provider_db;

namespace shill {

class CellularOperatorInfo;
class ControlInterface;
class EventDispatcher;
class GLib;
class Manager;
class Metrics;
class MobileOperatorInfo;
class ModemManager;
class PendingActivationStore;

// Manages modem managers.
class ModemInfo {
 public:
  ModemInfo(ControlInterface *control,
            EventDispatcher *dispatcher,
            Metrics *metrics,
            Manager *manager,
            GLib *glib);
  virtual ~ModemInfo();

  virtual void Start();
  virtual void Stop();

  virtual void OnDeviceInfoAvailable(const std::string &link_name);

  ControlInterface *control_interface() const { return control_interface_; }
  EventDispatcher *dispatcher() const { return dispatcher_; }
  Metrics *metrics() const { return metrics_; }
  Manager *manager() const { return manager_; }
  GLib *glib() const { return glib_; }
  PendingActivationStore *pending_activation_store() const {
    return pending_activation_store_.get();
  }
  CellularOperatorInfo *cellular_operator_info() const {
    return cellular_operator_info_.get();
  }
  mobile_provider_db *provider_db() const { return provider_db_; }
  MobileOperatorInfo *home_provider_info() const {
    return home_provider_info_.get();
  }
  MobileOperatorInfo *serving_operator_info() const {
    return serving_operator_info_.get();
  }

 protected:
  // Write accessors for unit-tests.
  void set_control_interface(ControlInterface *control) {
    control_interface_ = control;
  }
  void set_event_dispatcher(EventDispatcher *dispatcher) {
    dispatcher_ = dispatcher;
  }
  void set_metrics(Metrics *metrics) {
    metrics_ = metrics;
  }
  void set_manager(Manager *manager) {
    manager_ = manager;
  }
  void set_glib(GLib *glib) {
    glib_ = glib;
  }
  void set_pending_activation_store(
      PendingActivationStore *pending_activation_store);
  void set_cellular_operator_info(
      CellularOperatorInfo *cellular_operator_info);
  void set_mobile_provider_db(mobile_provider_db *provider_db) {
    provider_db_ = provider_db;
  }
  // Takes ownership.
  void set_home_provider_info(MobileOperatorInfo *home_provider_info);
  // Takes ownership.
  void set_serving_operator_info(MobileOperatorInfo *serving_operator_info);

 private:
  friend class ModemInfoTest;
  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemInfoTest, StartStop);

  typedef ScopedVector<ModemManager> ModemManagers;

  // Registers and starts |manager|. Takes ownership of |manager|.
  void RegisterModemManager(ModemManager *manager);
  ModemManagers modem_managers_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  GLib *glib_;

  // Post-payment activation state of the modem.
  scoped_ptr<PendingActivationStore> pending_activation_store_;
  scoped_ptr<CellularOperatorInfo> cellular_operator_info_;
  std::string provider_db_path_;  // For testing.
  mobile_provider_db *provider_db_;  // Database instance owned by |this|.
  // Operator info objects. These objects receive updates as we receive
  // information about the network operators from the SIM or OTA. In turn, they
  // send out updates through their observer interfaces whenever the identity of
  // the network operator changes, or any other property of the operator
  // changes.
  scoped_ptr<MobileOperatorInfo> home_provider_info_;
  scoped_ptr<MobileOperatorInfo> serving_operator_info_;

  DISALLOW_COPY_AND_ASSIGN(ModemInfo);
};

}  // namespace shill

#endif  // SHILL_MODEM_INFO_H_
