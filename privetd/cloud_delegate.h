// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_CLOUD_DELEGATE_H_
#define PRIVETD_CLOUD_DELEGATE_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/observer_list.h>

#include "privetd/privet_types.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace dbus {
class Bus;
}  // namespace dbus

namespace privetd {

class DeviceDelegate;

// Interface to provide GCD functionality for PrivetHandler.
// TODO(vitalybuka): Rename to BuffetDelegate.
class CloudDelegate {
 public:
  CloudDelegate();
  virtual ~CloudDelegate();

  using SuccessCallback = base::Callback<void(const base::DictionaryValue&)>;
  using ErrorCallback = base::Callback<void(chromeos::Error*)>;

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnRegistrationChanged() {}
    virtual void OnCommandDefsChanged() {}
    virtual void OnStateChanged() {}
  };

  // Returns status of the GCD connection.
  virtual const ConnectionState& GetConnectionState() const = 0;

  // Returns status of the last setup.
  virtual const SetupState& GetSetupState() const = 0;

  // Starts GCD setup.
  virtual bool Setup(const std::string& ticket_id,
                     const std::string& user,
                     chromeos::ErrorPtr* error) = 0;

  // Returns cloud id if the registered device or empty string if unregistered.
  virtual std::string GetCloudId() const = 0;

  // Returns dictionary with commands definitions.
  virtual const base::DictionaryValue& GetCommandDef() const = 0;

  // Adds command created from the given JSON representation.
  virtual void AddCommand(const base::DictionaryValue& command,
                          const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback) = 0;

  // Returns command with the given ID.
  virtual void GetCommand(const std::string& id,
                          const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback) = 0;

  void AddObserver(Observer* observer) { observer_list_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

  void NotifyOnRegistrationChanged();
  void NotifyOnCommandDefsChanged();
  void NotifyOnStateChanged();

  // Create default instance.
  static std::unique_ptr<CloudDelegate> CreateDefault(
      const scoped_refptr<dbus::Bus>& bus,
      DeviceDelegate* device);

 private:
  ObserverList<Observer> observer_list_;
};

}  // namespace privetd

#endif  // PRIVETD_CLOUD_DELEGATE_H_
