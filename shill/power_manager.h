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

  // This callback is called prior to a suspend event.  When it is OK for the
  // system to suspend, this callback should call SuspendActionComplete(),
  // passing it the sequence number passed to this callback.
  typedef base::Callback<void(uint32)> SuspendDelayCallback;

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

  // Registers a suspend delay callback with the power manager.  This callback
  // will be called prior to a suspend event by the amount specified in the most
  // recent call to RegisterSuspendDelay().  |key| must be unique.
  virtual void AddSuspendDelayCallback(const std::string &key,
                                       const SuspendDelayCallback &callback);

  // Unregisters a power state change callback identified by |key|.  The
  // callback must have been previously registered and not yet removed.
  virtual void RemoveStateChangeCallback(const std::string &key);

  // Unregisters a suspend delay callback identified by |key|.  The callback
  // must have been previously registered and not yet removed.
  virtual void RemoveSuspendDelayCallback(const std::string &key);

 private:
  friend class ManagerTest;

  typedef std::map<const std::string, PowerStateCallback>
    StateChangeCallbackMap;

  typedef std::map<const std::string, SuspendDelayCallback>
    SuspendDelayCallbackMap;

  template<class Callback>
  void AddCallback(const std::string &key, const Callback &callback,
                   std::map<const std::string, Callback> *callback_map);

  template<class Callback>
  void RemoveCallback(const std::string &key,
                      std::map<const std::string, Callback> *callback_map);

  template<class Param, class Callback>
  void OnEvent(const Param &param,
               std::map<const std::string, Callback> *callback_map) const;


  // The power manager proxy created by this class.  It dispatches the inherited
  // delegate methods of this object when changes in the power state occur.
  const scoped_ptr<PowerManagerProxyInterface> power_manager_proxy_;
  StateChangeCallbackMap state_change_callbacks_;
  SuspendDelayCallbackMap suspend_delay_callbacks_;

  SuspendState power_state_;

  DISALLOW_COPY_AND_ASSIGN(PowerManager);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_H_
