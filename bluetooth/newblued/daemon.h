// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_DAEMON_H_
#define BLUETOOTH_NEWBLUED_DAEMON_H_

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/message.h>

namespace bluetooth {

class Daemon {
 public:
  // Bluez's D-Bus object path representing the Bluetooth adapter.
  static const char kBluetoothAdapterObjectPath[];

  explicit Daemon(scoped_refptr<dbus::Bus> bus);
  ~Daemon();

  // Initializes the daemon D-Bus operations.
  void Init();

 private:
  // Called when the power manager is initially available or restarted.
  void HandlePowerManagerAvailableOrRestarted(bool available);
  // Called when ownership of the power manager's D-Bus service name changes.
  // Invokes HandlePowerManagerAvailableOrRestarted if the service is running.
  void PowerManagerNameOwnerChangedReceived(const std::string& old_owner,
                                            const std::string& new_owner);
  // Called when SuspendImminentSignal is received from power manager.
  void HandleSuspendImminentSignal(dbus::Signal* signal);
  // Called when SuspendDoneSignal is received from power manager.
  void HandleSuspendDoneSignal(dbus::Signal* signal);
  // Called when power manager's RegisterSuspendDelay method returns.
  void OnSuspendDelayRegistered(dbus::Response* response);

  // Called when bluez's PauseDiscovery/UnpauseDiscovery method returns.
  void OnDiscoveryPaused(dbus::Response* response);
  void OnDiscoveryUnpaused(dbus::Response* response);

  // Initiates call to bluez PauseDiscovery/UnpauseDiscovery.
  // These methods may or may not make the call to bluez depending on whether
  // there is a bluez PauseDiscovery/UnpauseDiscovery call in progress.
  void InitiatePauseDiscovery(int new_suspend_id);
  void InitiateUnpauseDiscovery();

  // Keeps the D-Bus connection. Mock/fake D-Bus can be injected through
  // constructor for unit testing without actual D-Bus IPC.
  scoped_refptr<dbus::Bus> bus_;

  // Proxy to power manager D-Bus service.
  dbus::ObjectProxy* power_manager_dbus_proxy_ = nullptr;
  // Proxy to bluez D-Bus service.
  dbus::ObjectProxy* bluez_dbus_proxy_ = nullptr;

  // If non-zero, that means we have registered a delay with power manager and
  // this variable keeps the delay id returned by power manager for later call
  // to HandleSuspendReadiness.
  // TODO(sonnysasaka): Migrate this to base::Optional when it's available.
  int suspend_delay_id_ = 0;

  // If non-zero, that means we are currently in a suspend imminent state and
  // this variable keeps its suspend id to be passed back to later
  // HandleSuspendReadiness.
  // TODO(sonnysasaka): Migrate this to base::Optional when it's available.
  int suspend_id_ = 0;

  // True if there is an in-progress bluez PauseDiscovery/UnpauseDiscovery call.
  // There can't be more than one PauseDiscovery/UnpauseDiscovery bluez call
  // at a time. This flag is needed to decide whether we can make the bluez call
  // immediately or "queued" after the in-progress D-Bus call completes.
  bool is_pause_or_unpause_in_progress_ = false;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Daemon> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_DAEMON_H_
