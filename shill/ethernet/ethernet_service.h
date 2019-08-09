// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_ETHERNET_SERVICE_H_
#define SHILL_ETHERNET_ETHERNET_SERVICE_H_

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/service.h"

namespace shill {

class Ethernet;
class Manager;

class EthernetService : public Service {
 public:
  static constexpr char kDefaultEthernetDeviceIdentifier[] = "ethernet_any";

  struct Properties {
   public:
    explicit Properties(const std::string& storage_id)
        : storage_id_(storage_id) {}
    explicit Properties(base::WeakPtr<Ethernet> ethernet)
        : ethernet_(ethernet) {}
    std::string storage_id_;
    base::WeakPtr<Ethernet> ethernet_;
  };

  EthernetService(Manager* manager, const Properties& props);
  ~EthernetService() override;

  // Inherited from Service.
  void Disconnect(Error* error, const char* reason) override;

  // ethernet_<MAC>
  std::string GetStorageIdentifier() const override;
  bool IsAutoConnectByDefault() const override;
  bool SetAutoConnectFull(const bool& connect, Error* error) override;

  void Remove(Error* error) override;
  bool IsVisible() const override;
  bool IsAutoConnectable(const char** reason) const override;

  // Override Load and Save from parent Service class.  We will call
  // the parent method.
  bool Load(StoreInterface* storage) override;
  bool Save(StoreInterface* storage) override;

  // Called by the Ethernet device when link state has caused the service
  // visibility to change.
  virtual void OnVisibilityChanged();

 protected:
  // This constructor performs none of the initialization that the normal
  // constructor does and sets the reported technology to |technology|.  It is
  // intended for use by subclasses which want to override specific aspects of
  // EthernetService behavior, while still retaining their own technology
  // identifier.
  EthernetService(Manager* manager,
                  Technology technology,
                  const Properties& props);

  // Inherited from Service.
  void OnConnect(Error* error) override;

  void SetUp();

  Ethernet* ethernet() const { return props_.ethernet_.get(); }
  std::string GetTethering(Error* error) const override;

 private:
  FRIEND_TEST(EthernetServiceTest, GetTethering);

  RpcIdentifier GetDeviceRpcId(Error* error) const override;

  Properties props_;
  DISALLOW_COPY_AND_ASSIGN(EthernetService);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_ETHERNET_SERVICE_H_
