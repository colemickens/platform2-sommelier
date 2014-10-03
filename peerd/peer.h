// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_PEER_H_
#define PEERD_PEER_H_

#include <map>
#include <string>
#include <vector>

#include <stdint.h>

#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_property_set.h>
#include <chromeos/errors/error.h>
#include <dbus/bus.h>
#include <gtest/gtest_prod.h>

#include "peerd/ip_addr.h"
#include "peerd/service.h"
#include "peerd/typedefs.h"

namespace peerd {

namespace errors {
namespace peer {

extern const char kInvalidUUID[];
extern const char kInvalidName[];
extern const char kInvalidNote[];
extern const char kUnknownService[];

}  // namespace peer
}  // namespace errors

// Exposes a Peer interface over DBus.  We use this class to represent
// ourself over DBus to interested viewers.  We also use it to represent
// remote peers that we've discovered over various mediums.
class Peer {
 public:
  Peer(const scoped_refptr<dbus::Bus>& bus,
       chromeos::dbus_utils::ExportedObjectManager* object_manager,
       const dbus::ObjectPath& path);
  virtual ~Peer() = default;
  bool RegisterAsync(
      chromeos::ErrorPtr* error,
      const std::string& uuid,
      const std::string& friendly_name,
      const std::string& note,
      uint64_t last_seen,
      const CompletionAction& completion_callback);

  virtual std::string GetUUID() const;
  virtual std::string GetFriendlyName() const;
  virtual std::string GetNote() const;
  // Returns false on failure.
  virtual bool SetFriendlyName(chromeos::ErrorPtr* error,
                               const std::string& friendly_name);
  // Returns false on failure.
  virtual bool SetNote(chromeos::ErrorPtr* error, const std::string& note);
  virtual void SetLastSeen(uint64_t last_seen);

  // Add a service to be exported by this peer.  Can fail if this peer
  // is already advertising a service with |service_id| or if |service_id|
  // and/or |service_info| are malformed.  Returns false on error, and can
  // optionally provide detailed error information in |error|.
  virtual bool AddService(
      chromeos::ErrorPtr* error,
      const std::string& service_id,
      const std::vector<ip_addr>& addresses,
      const std::map<std::string, std::string>& service_info);
  // Remove a service advertised by this peer.  Can fail if no service with id
  // |service_id| is in this peer.
  virtual bool RemoveService(chromeos::ErrorPtr* error,
                             const std::string& service_id);

 protected:
  std::map<std::string, std::unique_ptr<Service>> services_;

 private:
  scoped_refptr<dbus::Bus> bus_;
  size_t services_added_{0};
  chromeos::dbus_utils::ExportedProperty<std::string> uuid_;
  chromeos::dbus_utils::ExportedProperty<std::string> name_;
  chromeos::dbus_utils::ExportedProperty<std::string> note_;
  chromeos::dbus_utils::ExportedProperty<uint64_t> last_seen_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  dbus::ObjectPath service_path_prefix_;

  friend class PeerTest;
  friend class MockPeer;
  DISALLOW_COPY_AND_ASSIGN(Peer);
};

}  // namespace peerd

#endif  // PEERD_PEER_H_
