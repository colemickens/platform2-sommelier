// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_
#define SHILL_POWER_MANAGER_

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
//   PowerManager::PowerStateCallback *cb =
//     NewCallback(&foo, &Foo::HandleStateChange);
//   power_manager.AddStateChangeCallback("foo_key", cb);
//
// Whenever the power state changes, foo.HandleStateChange() is called with the
// new state passed in.  To unregister:
//
//   power_manager.RemoveStateChangeCallback("foo_key");
//
// The callback is owned by PowerManager and is deleted when removed.

#include <map>
#include <string>

#include <base/callback_old.h>
#include <base/scoped_ptr.h>

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
  typedef Callback1<SuspendState>::Type PowerStateCallback;

  // |proxy_factory| creates the PowerManagerProxy.  Usually this is
  // ProxyFactory::GetInstance().  Use a fake for testing.
  explicit PowerManager(ProxyFactory *proxy_factory);
  virtual ~PowerManager();

  // Methods inherited from PowerManagerProxyDelegate.
  virtual void OnSuspendDelay(uint32 sequence_number);
  virtual void OnPowerStateChanged(SuspendState new_power_state);

  // TODO(gmorain): Add descriptive comment.
  void RegisterSuspendDelay(const uint32_t &delay_ms);

  // Registers a power state change callback with the power manager.  When a
  // power state change occurs, this callback will be called with the new power
  // state as its argument.  |key| must be unique.  Ownership of |callback| is
  // passed to this class.
  virtual void AddStateChangeCallback(const std::string &key,
                                      PowerStateCallback *callback);

  // Unregisters a callback identified by |key|.  The callback must have been
  // previously registered and not yet removed.  The callback is deleted.
  virtual void RemoveStateChangeCallback(const std::string &key);

  // TODO(gmorain): Add registration for the OnSuspendDelay event.

 private:
  typedef std::map<const std::string, PowerStateCallback *>
    StateChangeCallbackMap;

  // The power manager proxy created by this class.  It dispatches the inherited
  // delegate methods of this object when changes in the power state occur.
  const scoped_ptr<PowerManagerProxyInterface> power_manager_proxy_;
  StateChangeCallbackMap state_change_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PowerManager);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_
