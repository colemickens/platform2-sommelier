// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_L2TP_IPSEC_DRIVER_
#define SHILL_L2TP_IPSEC_DRIVER_

#include <vector>

#include <base/file_path.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/key_value_store.h"
#include "shill/vpn_driver.h"

namespace shill {

class Manager;
class NSS;

class L2TPIPSecDriver : public VPNDriver {
 public:
  explicit L2TPIPSecDriver(Manager *manager);
  virtual ~L2TPIPSecDriver();

  // Inherited from VPNDriver.
  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index);
  virtual void Connect(const VPNServiceRefPtr &service, Error *error);
  virtual void Disconnect();
  virtual bool Load(StoreInterface *storage, const std::string &storage_id);
  virtual bool Save(StoreInterface *storage, const std::string &storage_id);
  virtual void InitPropertyStore(PropertyStore *store);
  virtual std::string GetProviderType() const;

 private:
  friend class L2TPIPSecDriverTest;
  FRIEND_TEST(L2TPIPSecDriverTest, AppendFlag);
  FRIEND_TEST(L2TPIPSecDriverTest, AppendValueOption);
  FRIEND_TEST(L2TPIPSecDriverTest, Cleanup);
  FRIEND_TEST(L2TPIPSecDriverTest, InitNSSOptions);
  FRIEND_TEST(L2TPIPSecDriverTest, InitOptions);
  FRIEND_TEST(L2TPIPSecDriverTest, InitOptionsNoHost);
  FRIEND_TEST(L2TPIPSecDriverTest, InitPSKOptions);

  static const char kPPPDPlugin[];

  void InitOptions(std::vector<std::string> *options, Error *error);
  bool InitPSKOptions(std::vector<std::string> *options, Error *error);
  void InitNSSOptions(std::vector<std::string> *options);

  void Cleanup();

  // Returns true if an opton was appended.
  bool AppendValueOption(const std::string &property,
                         const std::string &option,
                         std::vector<std::string> *options);

  // Returns true if a flag was appended.
  bool AppendFlag(const std::string &property,
                  const std::string &true_option,
                  const std::string &false_option,
                  std::vector<std::string> *options);

  Manager *manager_;
  NSS *nss_;
  KeyValueStore args_;

  FilePath psk_file_;

  DISALLOW_COPY_AND_ASSIGN(L2TPIPSecDriver);
};

}  // namespace shill

#endif  // SHILL_L2TP_IPSEC_DRIVER_
