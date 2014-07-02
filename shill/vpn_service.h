// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_SERVICE_
#define SHILL_VPN_SERVICE_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/connection.h"
#include "shill/service.h"

namespace shill {

class KeyValueStore;
class VPNDriver;

class VPNService : public Service {
 public:
  VPNService(ControlInterface *control,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             VPNDriver *driver);  // Takes ownership of |driver|.
  virtual ~VPNService();

  // Inherited from Service.
  virtual void Connect(Error *error, const char *reason);
  virtual void Disconnect(Error *error);
  virtual std::string GetStorageIdentifier() const;
  virtual bool Load(StoreInterface *storage);
  virtual bool Save(StoreInterface *storage);
  virtual bool Unload();
  virtual void EnableAndRetainAutoConnect();
  virtual void SetConnection(const ConnectionRefPtr &connection);
  virtual bool SetNameProperty(const std::string &name, Error *error);

  virtual void InitDriverPropertyStore();

  VPNDriver *driver() const { return driver_.get(); }

  static std::string CreateStorageIdentifier(const KeyValueStore &args,
                                             Error *error);
  void set_storage_id(const std::string &id) { storage_id_ = id; }

 protected:
  // Inherited from Service.
  virtual bool IsAutoConnectable(const char **reason) const;
  virtual std::string GetTethering(Error *error) const override;

 private:
  friend class VPNServiceTest;
  FRIEND_TEST(VPNServiceTest, GetDeviceRpcId);
  FRIEND_TEST(VPNServiceTest, SetConnection);
  FRIEND_TEST(VPNServiceTest, GetPhysicalTechologyPropertyFailsIfNoCarrier);
  FRIEND_TEST(VPNServiceTest, GetPhysicalTechologyPropertyOverWifi);
  FRIEND_TEST(VPNServiceTest, GetTethering);

  static const char kAutoConnNeverConnected[];
  static const char kAutoConnVPNAlreadyActive[];

  virtual std::string GetDeviceRpcId(Error *error) const;

  // Returns the Type name of the lowest connection (presumably the "physical"
  // connection) that this service depends on.
  std::string GetPhysicalTechologyProperty(Error *error);

  std::string storage_id_;
  scoped_ptr<VPNDriver> driver_;
  scoped_ptr<Connection::Binder> connection_binder_;

  // Provided only for compatibility.  crbug.com/211858
  std::string vpn_domain_;

  DISALLOW_COPY_AND_ASSIGN(VPNService);
};

}  // namespace shill

#endif  // SHILL_VPN_SERVICE_
