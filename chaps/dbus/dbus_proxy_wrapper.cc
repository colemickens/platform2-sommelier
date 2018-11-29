// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include <brillo/errors/error.h>

#include "chaps/dbus/dbus_proxy_wrapper.h"
#include "chaps/isolate.h"

namespace {

void OnServiceAvailable(chaps::OnObjectProxyConstructedCallback callback,
                        chaps::ScopedBus bus,
                        dbus::ObjectProxy* proxy,
                        bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "Failed to wait for chaps service to become available";
    callback.Run(false, chaps::ScopedBus(), nullptr);
    return;
  }

  // Call GetSlotList to perform stage 2 initialization of chapsd if it
  // hasn't already done so.
  brillo::ErrorPtr error;
  brillo::SecureBlob default_isolate_credential_blob =
      chaps::IsolateCredentialManager::GetDefaultIsolateCredential();

  std::vector<uint8_t> default_isolate_credential(
      default_isolate_credential_blob.begin(),
      default_isolate_credential_blob.end());

  if (!brillo::dbus_utils::CallMethodAndBlockWithTimeout(
          chaps::DBusProxyWrapper::kDBusTimeoutMs, proxy,
          chaps::kChapsInterface, chaps::kGetSlotListMethod, &error,
          default_isolate_credential, false)) {
    LOG(ERROR) << "Chaps service is up but unresponsive: "
               << error->GetMessage();
    callback.Run(false, chaps::ScopedBus(), nullptr);
    return;
  }

  callback.Run(true, std::move(bus), proxy);
}

void CreateObjectProxyOnTaskRunner(
    const chaps::OnObjectProxyConstructedCallback& callback) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  chaps::ScopedBus bus(options);

  auto proxy = bus->GetObjectProxy(chaps::kChapsServiceName,
                                   dbus::ObjectPath(chaps::kChapsServicePath));
  if (!proxy) {
    callback.Run(false, chaps::ScopedBus(), nullptr);
    return;
  }

  proxy->WaitForServiceToBeAvailable(
      base::Bind(&OnServiceAvailable,
                 callback,
                 base::Passed(std::move(bus)),
                 proxy));
}

}  // namespace

namespace chaps {

ProxyWrapperConstructionTask::ProxyWrapperConstructionTask()
    : construction_callback_(base::Bind(&CreateObjectProxyOnTaskRunner)),
      completion_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {}

scoped_refptr<DBusProxyWrapper>
ProxyWrapperConstructionTask::ConstructProxyWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // This task will call the SetObjectProxyCallback with the results
  // of the construction attempt.
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(
          construction_callback_,
          base::Bind(&ProxyWrapperConstructionTask::SetObjectProxyCallback,
                     this)));

  // If we wait too long for the chapsd service to become available,
  // cancel construction.
  if (!completion_event_.TimedWait(base::TimeDelta::FromSeconds(5))) {
    LOG(ERROR) << "Chaps service is not available";
    return nullptr;
  }

  // |completion_event_| was signaled, try constructing the proxy.
  if (!success_)
    return nullptr;

  return scoped_refptr<DBusProxyWrapper>(
      new DBusProxyWrapper(task_runner, std::move(bus_), object_proxy_));
}

void ProxyWrapperConstructionTask::SetObjectProxyCallback(
    bool success,
    ScopedBus bus,
    dbus::ObjectProxy* object_proxy) {
  success_ = success;
  bus_ = std::move(bus);
  object_proxy_ = object_proxy;
  completion_event_.Signal();
}

}  // namespace chaps
