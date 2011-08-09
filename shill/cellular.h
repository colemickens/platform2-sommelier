// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_
#define SHILL_CELLULAR_

#include <string>

#include <base/basictypes.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_properties.h"
#include "shill/device.h"
#include "shill/modem_cdma_proxy_interface.h"
#include "shill/modem_proxy_interface.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class ModemSimpleProxyInterface;

class Cellular : public Device,
                 public ModemCDMAProxyListener,
                 public ModemProxyListener {
 public:
  enum Type {
    kTypeGSM,
    kTypeCDMA
  };

  enum State {
    kStateDisabled,
    kStateEnabled,
    kStateRegistered,
    kStateConnected,
  };

  class Network {
   public:
    Network();
    ~Network();

    const std::string &GetStatus() const;
    void SetStatus(const std::string &status);

    const std::string &GetId() const;
    void SetId(const std::string &id);

    const std::string &GetShortName() const;
    void SetShortName(const std::string &name);

    const std::string &GetLongName() const;
    void SetLongName(const std::string &name);

    const std::string &GetTechnology() const;
    void SetTechnology(const std::string &technology);

    const Stringmap &ToDict() const;

   private:
    Stringmap dict_;

    DISALLOW_COPY_AND_ASSIGN(Network);
  };

  struct CDMA {
    CDMA();

    uint32 registration_state_evdo;
    uint32 registration_state_1x;
    uint32 activation_state;
  };

  struct SimLockStatus {
   public:
    SimLockStatus() : retries_left(0) {}

    std::string lock_type;
    uint32 retries_left;
  };

  static const char kConnectPropertyPhoneNumber[];

  // |owner| is the ModemManager DBus service owner (e.g., ":1.17"). |path| is
  // the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  Cellular(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const std::string &link_name,
           int interface_index,
           Type type,
           const std::string &owner,
           const std::string &path);
  virtual ~Cellular();

  // Asynchronously connects the modem to the network.
  void Connect();

  // Inherited from Device.
  virtual void Start();
  virtual void Stop();
  virtual bool TechnologyIs(Technology type) const;

 private:
  static const char kPhoneNumberCDMA[];
  static const char kPhoneNumberGSM[];

  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, GetCDMARegistrationState);
  FRIEND_TEST(CellularTest, GetCDMASignalQuality);
  FRIEND_TEST(CellularTest, GetModemInfo);
  FRIEND_TEST(CellularTest, GetModemStatus);
  FRIEND_TEST(CellularTest, GetStateString);
  FRIEND_TEST(CellularTest, GetTypeString);
  FRIEND_TEST(CellularTest, InitProxiesCDMA);
  FRIEND_TEST(CellularTest, InitProxiesGSM);
  FRIEND_TEST(CellularTest, Start);

  void ConnectTask(const DBusPropertiesMap &properties);

  Stringmaps EnumerateNetworks();
  StrIntPair SimLockStatusToProperty();
  void HelpRegisterDerivedStringmaps(const std::string &name,
                                     Stringmaps(Cellular::*get)(void),
                                     bool(Cellular::*set)(const Stringmaps&));
  void HelpRegisterDerivedStrIntPair(const std::string &name,
                                     StrIntPair(Cellular::*get)(void),
                                     bool(Cellular::*set)(const StrIntPair&));

  void InitProxies();

  std::string GetTypeString();
  std::string GetStateString();

  void EnableModem();
  void GetModemStatus();
  void GetGSMProperties();
  void RegisterGSMModem();

  // Obtains the modem identifiers: MEID for CDMA; IMEI, IMSI, SPN and MSISDN
  // for GSM.
  void GetModemIdentifiers();

  // Obtains modem's manufacturer, model ID, and hardware revision.
  void GetModemInfo();

  void GetModemRegistrationState();
  void GetCDMARegistrationState();
  void GetGSMRegistrationState();

  // Processes a change in the modem registration state, possibly creating,
  // destroying or updating the CellularService.
  void HandleNewRegistrationState();

  void CreateService();

  void GetModemSignalQuality();
  uint32 GetCDMASignalQuality();
  uint32 GetGSMSignalQuality();

  void HandleNewSignalQuality(uint32 strength);

  // Returns true if the modem is registered. Note that this method looks at the
  // latest CDMA/GSM registration info obtained from the modem rather than the
  // device |state_|.
  bool IsModemRegistered();

  // Signal callbacks inherited from ModemCDMAProxyListener.
  virtual void OnCDMARegistrationStateChanged(uint32 state_1x,
                                              uint32 state_evdo);
  virtual void OnCDMASignalQualityChanged(uint32 strength);

  // Signal callbacks inherited from ModemProxyListener.
  virtual void OnModemStateChanged(uint32 old_state,
                                   uint32 new_state,
                                   uint32 reason);

  Type type_;
  State state_;

  const std::string dbus_owner_;  // ModemManager.Modem
  const std::string dbus_path_;  // ModemManager.Modem
  scoped_ptr<ModemProxyInterface> proxy_;
  scoped_ptr<ModemSimpleProxyInterface> simple_proxy_;
  scoped_ptr<ModemCDMAProxyInterface> cdma_proxy_;

  CDMA cdma_;

  CellularServiceRefPtr service_;
  bool service_registered_;

  ScopedRunnableMethodFactory<Cellular> task_factory_;

  // Properties
  bool allow_roaming_;
  std::string carrier_;
  std::string meid_;
  std::string imei_;
  std::string imsi_;
  std::string esn_;
  std::string mdn_;
  std::string min_;
  std::string model_id_;
  std::string manufacturer_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  uint16 prl_version_;
  bool scanning_;
  uint16 scan_interval_;
  std::vector<Network> found_networks_;
  SimLockStatus sim_lock_status_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_
