// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_MANAGER_H_
#define WEBSERVER_WEBSERVD_MANAGER_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/dbus/dbus_object.h>

#include "webservd/org.chromium.WebServer.Manager.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace webservd {

// Manages global state of webservd.
class Manager : public org::chromium::WebServer::ManagerInterface {
 public:
  explicit Manager(chromeos::dbus_utils::ExportedObjectManager* object_manager);
  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction& cb);

  // Overrides from org::chromium::WebServer::ManagerInterface.
  std::string Ping() override;

 private:
  org::chromium::WebServer::ManagerAdaptor dbus_adaptor_{this};
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_MANAGER_H_
