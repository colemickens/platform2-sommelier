// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "container_utils/device_jail/permission_broker_client.h"

#include <memory>

#include <base/bind.h>
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/file_descriptor.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace {

void OpenWithBrokerAsync(dbus::ObjectProxy* broker_proxy,
                         const std::string& path,
                         const base::Callback<void(int)>& callback) {
  DLOG(INFO) << "Open(" << path << ")";
  dbus::MethodCall method_call(permission_broker::kPermissionBrokerInterface,
                               permission_broker::kOpenPath);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(path);

  std::unique_ptr<dbus::Response> response =
      broker_proxy->CallMethodAndBlock(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!response) {
    DLOG(INFO) << "Open(" << path << "): permission denied";
    callback.Run(-EACCES);
    return;
  }

  dbus::FileDescriptor fd;
  dbus::MessageReader reader(response.get());
  if (!reader.PopFileDescriptor(&fd)) {
    LOG(ERROR) << "Could not parse permission broker's response";
    callback.Run(-EINVAL);
    return;
  }

  fd.CheckValidity();
  if (!fd.is_valid()) {
    LOG(ERROR) << "Permission broker returned invalid fd";
    callback.Run(-EINVAL);
    return;
  }

  DLOG(INFO) << "Open(" << path << ") -> " << fd.value();

  // Pass the FD back to the caller.
  callback.Run(fd.TakeValue());
}

}  // namespace

namespace device_jail {


void PermissionBrokerClient::Open(const std::string& path,
                                  const base::Callback<void(int)>& callback) {
  // This must be run on the thread which instantiated the D-Bus objects
  // or else the library will get mad, as above.
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&OpenWithBrokerAsync, broker_proxy_, path, callback));
}

}  // namespace device_jail
