// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_PEERD_CLIENT_H_
#define LEADERD_PEERD_CLIENT_H_

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "peerd/dbus-proxies.h"

namespace leaderd {

class PeerdClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnPeerdAvailable() = 0;
    virtual void OnPeerdDeath() = 0;
    virtual void OnSelfIdChanged(const std::string& uuid) = 0;
    virtual void OnPeerGroupsChanged(
        const std::string& peer_uuid,
        const std::set<std::string>& group_ids) = 0;
  };

  PeerdClient() = default;
  virtual ~PeerdClient() = default;

  void SetDelegate(Delegate* delegate);
  std::set<std::string> GetPeersMatchingGroup(const std::string& in_group_id);
  void StartMonitoring();
  void StopMonitoring();
  std::vector<std::tuple<std::vector<uint8_t>, uint16_t>> GetIPInfo(
      const std::string& peer_uuid);

 protected:
  virtual org::chromium::peerd::PeerProxyInterface* GetPeerProxy(
      const dbus::ObjectPath& object_path) = 0;
  virtual org::chromium::peerd::ServiceProxyInterface* GetServiceProxy(
      const dbus::ObjectPath& object_path) = 0;
  virtual org::chromium::peerd::ManagerProxyInterface* GetManagerProxy() = 0;

  void UpdatePeerService(
      org::chromium::peerd::ServiceProxyInterface* service_proxy,
      const dbus::ObjectPath& object_path);
  void RemovePeerService(const dbus::ObjectPath& object_path);

  Delegate* delegate_{nullptr};
  bool monitoring_{false};
  std::string monitor_token_;
  std::map<dbus::ObjectPath, std::string> paths_to_uuids_;
  std::map<std::string, dbus::ObjectPath> uuids_to_paths_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PeerdClient);
};

class PeerdClientImpl : public PeerdClient {
 public:
  explicit PeerdClientImpl(const scoped_refptr<dbus::Bus>& bus);
  ~PeerdClientImpl() override = default;

 protected:
  org::chromium::peerd::PeerProxyInterface* GetPeerProxy(
      const dbus::ObjectPath& object_path) override;
  org::chromium::peerd::ServiceProxyInterface* GetServiceProxy(
      const dbus::ObjectPath& object_path) override;
  org::chromium::peerd::ManagerProxyInterface* GetManagerProxy() override;

 private:
  void OnPeerdManagerAdded(org::chromium::peerd::ManagerProxy* manager_proxy);
  void OnPeerdManagerRemoved(const dbus::ObjectPath& object_path);
  void OnPeerdPeerAdded(org::chromium::peerd::PeerProxy* peer_proxy);
  void OnPeerdServiceAdded(org::chromium::peerd::ServiceProxy* service_proxy);
  void OnPeerdServiceRemoved(const dbus::ObjectPath& object_path);
  void OnPeerdServiceChanged(org::chromium::peerd::ServiceProxy* service_proxy,
                             const std::string& property);

 private:
  scoped_refptr<dbus::Bus> bus_;
  org::chromium::peerd::ObjectManagerProxy peerd_object_manager_proxy_;
  base::WeakPtrFactory<PeerdClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PeerdClientImpl);
};

}  // namespace leaderd

#endif  // LEADERD_PEERD_CLIENT_H_
