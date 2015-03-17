// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BRDEBUG_PEERD_CLIENT_H_
#define BRDEBUG_PEERD_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>

#include "peerd/dbus-proxies.h"

namespace brdebug {

// Publishes brdebug service on mDns using peerd.
class PeerdClient {
 public:
  PeerdClient(const scoped_refptr<dbus::Bus>& bus,
              const std::map<std::string, std::string>& service_info);
  virtual ~PeerdClient();

  // Updates brdebug service info with peerd.
  void UpdateServiceInfo(const std::map<std::string, std::string>& info);

 private:
  void OnPeerdOnline(org::chromium::peerd::ManagerProxy* manager_proxy);
  void OnPeerdOffline(const dbus::ObjectPath& object_path);
  void ExposeService();
  void RemoveService();

  org::chromium::peerd::ObjectManagerProxy peerd_object_manager_proxy_;
  // |peerd_manager_proxy_| is owned by |peerd_object_manager_proxy_|.
  org::chromium::peerd::ManagerProxy* peerd_manager_proxy_{nullptr};

  // The brdebug service info for peerd.
  std::map<std::string, std::string> service_info_;

  base::WeakPtrFactory<PeerdClient> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PeerdClient);
};

}  // namespace brdebug

#endif  // BRDEBUG_PEERD_CLIENT_H_
