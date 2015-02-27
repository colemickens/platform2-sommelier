// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_PEERD_CLIENT_H_
#define LEADERD_PEERD_CLIENT_H_

#include <string>

#include "peerd/dbus-proxies.h"

namespace leaderd {

class PeerdClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnPeerdManagerAdded(
        org::chromium::peerd::ManagerProxyInterface* manager_proxy) = 0;
    virtual void OnPeerdManagerRemoved(const dbus::ObjectPath& object_path) = 0;
    virtual void OnPeerdPeerAdded(
        org::chromium::peerd::PeerProxyInterface* peer_proxy,
        const dbus::ObjectPath& object_path) = 0;
    virtual void OnPeerdPeerRemoved(const dbus::ObjectPath& object_path) = 0;
    virtual void OnPeerdServiceAdded(
        org::chromium::peerd::ServiceProxyInterface* service_proxy,
        const dbus::ObjectPath& object_path) = 0;
    virtual void OnPeerdServiceRemoved(const dbus::ObjectPath& object_path) = 0;
    virtual void OnPeerdServiceChanged(
        org::chromium::peerd::ServiceProxyInterface* service_proxy,
        const dbus::ObjectPath& object_path, const std::string& property) = 0;
  };

  PeerdClient() = default;
  virtual ~PeerdClient() = default;

  void SetDelegate(Delegate* delegate);
  virtual org::chromium::peerd::ManagerProxyInterface* GetManagerProxy() = 0;
  virtual org::chromium::peerd::PeerProxyInterface* GetPeerProxy(
      const dbus::ObjectPath& object_path) = 0;
  virtual org::chromium::peerd::ServiceProxyInterface* GetServiceProxy(
      const dbus::ObjectPath& object_path) = 0;

 protected:
  Delegate* delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PeerdClient);
};

class PeerdClientImpl : public PeerdClient {
 public:
  explicit PeerdClientImpl(const scoped_refptr<dbus::Bus>& bus);
  ~PeerdClientImpl() override = default;

  org::chromium::peerd::ManagerProxyInterface* GetManagerProxy() override;
  org::chromium::peerd::PeerProxyInterface* GetPeerProxy(
      const dbus::ObjectPath& object_path) override;
  org::chromium::peerd::ServiceProxyInterface* GetServiceProxy(
      const dbus::ObjectPath& object_path) override;

 private:
  void OnPeerdManagerAdded(org::chromium::peerd::ManagerProxy* manager_proxy);
  void OnPeerdManagerRemoved(const dbus::ObjectPath& object_path);
  void OnPeerdPeerAdded(org::chromium::peerd::PeerProxy* peer_proxy);
  void OnPeerdPeerRemoved(const dbus::ObjectPath& object_path);
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
