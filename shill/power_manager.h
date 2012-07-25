// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_H_
#define SHILL_POWER_MANAGER_H_

// This class instantiates a PowerManagerProxy and distributes power events to
// registered users.  It also provides a means for calling methods on the
// PowerManagerProxy.
//
// Usage:
//
// Registering for power state changes is done as follows:
//
//   class Foo {
//    public:
//     void HandleStateChange(PowerManager::SuspendState new_state);
//   };
//   Foo foo;
//   PowerManager power_manager(ProxyFactory::GetInstance());
//   PowerManager::PowerStateCallback cb = Bind(&Foo::HandleStateChange, &foo);
//   power_manager.AddStateChangeCallback("foo_key", cb);
//
//   Note that depending on the definition of Foo, "&foo" may need to appear
//   inside an appropriate wrapper, such as base::Unretained.
//
// Whenever the power state changes, foo.HandleStateChange() is called with the
// new state passed in. To unregister:
//
//   power_manager.RemoveStateChangeCallback("foo_key");

#include <map>
#include <string>

#include <base/callback.h>
#include <base/memory/scoped_ptr.h>

#include "shill/power_manager_proxy_interface.h"

namespace shill {

class ProxyFactory;

class PowerManager : public PowerManagerProxyDelegate {
 public:
  typedef PowerManagerProxyDelegate::SuspendState SuspendState;

  // Callbacks registered with the power manager are of this type.  They take
  // one argument, the new power state.  The callback function or method should
  // look like this:
  //
  //   void HandlePowerStateChange(PowerStateCallbacks::SuspendState);
  typedef base::Callback<void(SuspendState)> PowerStateCallback;

  // |proxy_factory| creates the PowerManagerProxy.  Usually this is
  // ProxyFactory::GetInstance().  Use a fake for testing.
  explicit PowerManager(ProxyFactory *proxy_factory);
  virtual ~PowerManager();

  SuspendState power_state() const { return power_state_; }

  // Methods inherited from PowerManagerProxyDelegate.
  virtual void OnSuspendDelay(uint32 sequence_number);
  virtual void OnPowerStateChanged(SuspendState new_power_state);

  // TODO(gmorain): Add descriptive comment.
  void RegisterSuspendDelay(const uint32_t &delay_ms);

  // Registers a power state change callback with the power manager.  When a
  // power state change occurs, this callback will be called with the new power
  // state as its argument.  |key| must be unique.
  virtual void AddStateChangeCallback(const std::string &key,
                                      const PowerStateCallback &callback);

  // Unregisters a callback identified by |key|.  The callback must have been
  // previously registered and not yet removed.  The callback is deleted.
  virtual void RemoveStateChangeCallback(const std::string &key);

  // TODO(gmorain): Add registration for the OnSuspendDelay event.

 private:
  friend class ManagerTest;

  typedef std::map<const std::string, PowerStateCallback>
    StateChangeCallbackMap;

  // The power manager proxy created by this class.  It dispatches the inherited
  // delegate methods of this object when changes in the power state occur.
  const scoped_ptr<PowerManagerProxyInterface> power_manager_proxy_;
  StateChangeCallbackMap state_change_callbacks_;

  SuspendState power_state_;

  DISALLOW_COPY_AND_ASSIGN(PowerManager);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_H_
