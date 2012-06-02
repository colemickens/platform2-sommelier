// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_NETWORK_H_
#define WIMAX_MANAGER_NETWORK_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>

#include "wimax_manager/dbus_adaptable.h"

namespace wimax_manager {

enum NetworkType {
  kNetworkHome,
  kNetworkPartner,
  kNetworkRoamingParnter,
  kNetworkUnknown,
};

class NetworkDBusAdaptor;

class Network : public base::RefCounted<Network>,
                public DBusAdaptable<Network, NetworkDBusAdaptor> {
 public:
  typedef uint32 Identifier;

  static const int kMaxCINR;
  static const int kMinCINR;
  static const int kMaxRSSI;
  static const int kMinRSSI;

  Network(Identifier identifier, const std::string &name, NetworkType type,
          int cinr, int rssi);

  static int DecodeCINR(int encoded_cinr);
  static int DecodeRSSI(int encoded_rssi);

  void UpdateFrom(const Network &network);
  int GetSignalStrength() const;

  Identifier identifier() const { return identifier_; }
  const std::string &name() const { return name_; }
  NetworkType type() const { return type_; }
  int cinr() const { return cinr_; }
  int rssi() const { return rssi_; }

 private:
  friend class base::RefCounted<Network>;

  ~Network();

  Identifier identifier_;
  std::string name_;
  NetworkType type_;
  int cinr_;
  int rssi_;

  DISALLOW_COPY_AND_ASSIGN(Network);
};

typedef scoped_refptr<Network> NetworkRefPtr;
typedef std::map<Network::Identifier, NetworkRefPtr> NetworkMap;

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_NETWORK_H_
