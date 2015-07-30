// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MODEM_MANAGER_H_
#define SHILL_CELLULAR_MODEM_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular/dbus_objectmanager_proxy_interface.h"
#include "shill/cellular/modem_info.h"
#include "shill/dbus_properties_proxy_interface.h"
#include "shill/glib.h"

namespace shill {

class ControlInterface;
class DBusNameWatcher;
class DBusObjectManagerProxyInterface;
class DBusPropertiesProxyInterface;
class Modem1;
class Modem;
class ModemClassic;
class ModemManagerProxyInterface;

// Handles a modem manager service and creates and destroys modem instances.
class ModemManager {
 public:
  ModemManager(ControlInterface* control_interface,
               const std::string& service,
               const std::string& path,
               ModemInfo* modem_info);
  virtual ~ModemManager();

  // Starts watching for and handling the DBus modem manager service.
  void Start();

  // Stops watching for the DBus modem manager service and destroys any
  // associated modems.
  void Stop();

  // DBusNameWatcher callbacks.
  void OnAppear(const std::string& name, const std::string& owner);
  void OnVanish(const std::string& name);

  void OnDeviceInfoAvailable(const std::string& link_name);

 protected:
  typedef std::map<std::string, std::shared_ptr<Modem>> Modems;

  const std::string& owner() const { return owner_; }
  const std::string& service() const { return service_; }
  const std::string& path() const { return path_; }
  ControlInterface* control_interface() const { return control_interface_; }
  ModemInfo* modem_info() const { return modem_info_; }

  // Connect/Disconnect to a modem manager service.
  // Inheriting classes must call this superclass method.
  virtual void Connect(const std::string& owner);
  // Inheriting classes must call this superclass method.
  virtual void Disconnect();

  bool ModemExists(const std::string& path) const;
  // Put the modem into our modem map
  void RecordAddedModem(std::shared_ptr<Modem> modem);

  // Removes a modem on |path|.
  void RemoveModem(const std::string& path);

 private:
  friend class ModemManagerCoreTest;
  friend class ModemManagerClassicTest;
  friend class ModemManager1Test;

  FRIEND_TEST(ModemInfoTest, RegisterModemManager);
  FRIEND_TEST(ModemManager1Test, AddRemoveInterfaces);
  FRIEND_TEST(ModemManager1Test, Connect);
  FRIEND_TEST(ModemManagerClassicTest, Connect);
  FRIEND_TEST(ModemManagerCoreTest, AddRemoveModem);
  FRIEND_TEST(ModemManagerCoreTest, ConnectDisconnect);
  FRIEND_TEST(ModemManagerCoreTest, OnAppearVanish);
  FRIEND_TEST(ModemManagerCoreTest, StartStopWithModemManagerServiceAbsent);
  FRIEND_TEST(ModemManagerCoreTest, StartStopWithModemManagerServicePresent);

  ControlInterface* control_interface_;

  const std::string service_;
  const std::string path_;
  std::unique_ptr<DBusNameWatcher> name_watcher_;

  std::string owner_;  // DBus service owner.

  Modems modems_;  // Maps a modem |path| to a modem instance.

  ModemInfo* modem_info_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager);
};

class ModemManagerClassic : public ModemManager {
 public:
  ModemManagerClassic(ControlInterface* control_interface,
                      const std::string& service,
                      const std::string& path,
                      ModemInfo* modem_info);

  ~ModemManagerClassic() override;

  // Called by our dbus proxy
  void OnDeviceAdded(const std::string& path);
  void OnDeviceRemoved(const std::string& path);

 protected:
  void Connect(const std::string& owner) override;
  void Disconnect() override;

  virtual void AddModemClassic(const std::string& path);
  virtual void InitModemClassic(std::shared_ptr<ModemClassic> modem);

 private:
  std::unique_ptr<ModemManagerProxyInterface> proxy_;  // DBus service proxy
  std::unique_ptr<DBusPropertiesProxyInterface> dbus_properties_proxy_;

  FRIEND_TEST(ModemManagerClassicTest, Connect);

  DISALLOW_COPY_AND_ASSIGN(ModemManagerClassic);
};

class ModemManager1 : public ModemManager {
 public:
  ModemManager1(ControlInterface* control_interface,
                const std::string& service,
                const std::string& path,
                ModemInfo* modem_info);

  ~ModemManager1() override;

 protected:
  void AddModem1(const std::string& path,
                 const DBusInterfaceToProperties& properties);
  virtual void InitModem1(std::shared_ptr<Modem1> modem,
                          const DBusInterfaceToProperties& properties);

  // ModemManager methods
  void Connect(const std::string& owner) override;
  void Disconnect() override;

  // DBusObjectManagerProxyDelegate signal methods
  virtual void OnInterfacesAddedSignal(
      const ::DBus::Path& object_path,
      const DBusInterfaceToProperties& properties);
  virtual void OnInterfacesRemovedSignal(
      const ::DBus::Path& object_path,
      const std::vector<std::string>& interfaces);

  // DBusObjectManagerProxyDelegate method callbacks
  virtual void OnGetManagedObjectsReply(
      const DBusObjectsWithProperties& objects_with_properties,
      const Error& error);

 private:
  friend class ModemManager1Test;
  FRIEND_TEST(ModemManager1Test, Connect);
  FRIEND_TEST(ModemManager1Test, AddRemoveInterfaces);

  std::unique_ptr<DBusObjectManagerProxyInterface> proxy_;
  base::WeakPtrFactory<ModemManager1> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModemManager1);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_MANAGER_H_
