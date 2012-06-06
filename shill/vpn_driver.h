// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_DRIVER_
#define SHILL_VPN_DRIVER_

#include <string>

#include <base/basictypes.h>
#include <base/cancelable_callback.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/key_value_store.h"
#include "shill/refptr_types.h"

namespace shill {

class Error;
class EventDispatcher;
class Manager;
class PropertyStore;
class StoreInterface;

class VPNDriver {
 public:
  virtual ~VPNDriver();

  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index) = 0;
  virtual void Connect(const VPNServiceRefPtr &service, Error *error) = 0;
  virtual void Disconnect() = 0;
  virtual void OnConnectionDisconnected() = 0;
  virtual std::string GetProviderType() const = 0;

  virtual void InitPropertyStore(PropertyStore *store);

  virtual bool Load(StoreInterface *storage, const std::string &storage_id);
  virtual bool Save(StoreInterface *storage,
                    const std::string &storage_id,
                    bool save_credentials);
  virtual void UnloadCredentials();

  KeyValueStore *args() { return &args_; }

 protected:
  struct Property {
    enum Flags {
      kEphemeral = 1 << 0,   // Never load or save.
      kCredential = 1 << 1,  // Save if saving credentials (crypted).
      kWriteOnly = 1 << 2,   // Never read over RPC.
    };

    const char *property;
    int flags;
  };

  VPNDriver(EventDispatcher *dispatcher,
            Manager *manager,
            const Property *properties,
            size_t property_count);

  EventDispatcher *dispatcher() const { return dispatcher_; }
  Manager *manager() const { return manager_; }

  virtual KeyValueStore GetProvider(Error *error);

  // Initializes a callback that will invoke OnConnectTimeout. The timeout will
  // not be restarted if it's already scheduled.
  void StartConnectTimeout();
  // Cancels the connect timeout callback, if any, previously scheduled through
  // StartConnectTimeout.
  void StopConnectTimeout();
  // Returns true if a connect timeout is scheduled, false otherwise.
  bool IsConnectTimeoutStarted() const;

 private:
  FRIEND_TEST(VPNDriverTest, ConnectTimeout);

  static const int kDefaultConnectTimeoutSeconds;

  void ClearMappedProperty(const size_t &index, Error *error);
  std::string GetMappedProperty(const size_t &index, Error *error);
  void SetMappedProperty(
      const size_t &index, const std::string &value, Error *error);

  // Called if a connect timeout scheduled through StartConnectTimeout
  // fires. Marks the callback as stopped and invokes OnConnectionDisconnected.
  void OnConnectTimeout();

  base::WeakPtrFactory<VPNDriver> weak_ptr_factory_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  const Property * const properties_;
  const size_t property_count_;
  KeyValueStore args_;

  base::CancelableClosure connect_timeout_callback_;
  int connect_timeout_seconds_;

  DISALLOW_COPY_AND_ASSIGN(VPNDriver);
};

}  // namespace shill

#endif  // SHILL_VPN_DRIVER_
