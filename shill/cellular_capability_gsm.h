// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_GSM_
#define SHILL_CELLULAR_CAPABILITY_GSM_

#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/cellular_capability.h"
#include "shill/modem_gsm_card_proxy_interface.h"
#include "shill/modem_gsm_network_proxy_interface.h"

namespace shill {

class CellularCapabilityGSM : public CellularCapability,
                              public ModemGSMCardProxyDelegate {
 public:
  CellularCapabilityGSM(Cellular *cellular);

  // Inherited from CellularCapability.
  virtual void InitProxies();
  virtual void GetSignalQuality();
  virtual void RequirePIN(const std::string &pin, bool require, Error *error);
  virtual void EnterPIN(const std::string &pin, Error *error);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error);
  virtual void Scan(Error *error);
  virtual std::string GetNetworkTechnologyString() const;
  virtual std::string GetRoamingStateString() const;

  // Obtains the IMEI, IMSI, SPN and MSISDN.
  virtual void GetIdentifiers();

 private:
  friend class CellularCapabilityGSMTest;
  FRIEND_TEST(CellularCapabilityGSMTest, ParseScanResult);
  FRIEND_TEST(CellularCapabilityGSMTest, ParseScanResultProviderLookup);
  FRIEND_TEST(CellularCapabilityGSMTest, Scan);

  static const char kNetworkPropertyAccessTechnology[];
  static const char kNetworkPropertyID[];
  static const char kNetworkPropertyLongName[];
  static const char kNetworkPropertyShortName[];
  static const char kNetworkPropertyStatus[];

  void RequirePINTask(const std::string &pin, bool require);
  void EnterPINTask(const std::string &pin);
  void UnblockPINTask(const std::string &unblock_code, const std::string &pin);
  void ChangePINTask(const std::string &old_pin, const std::string &new_pin);
  void ScanTask();

  Stringmap ParseScanResult(
      const ModemGSMNetworkProxyInterface::ScanResult &result);

  scoped_ptr<ModemGSMCardProxyInterface> card_proxy_;

  ScopedRunnableMethodFactory<CellularCapabilityGSM> task_factory_;

  // Properties.
  Stringmaps found_networks_;
  bool scanning_;
  uint16 scan_interval_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityGSM);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_GSM_
