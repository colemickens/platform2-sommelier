// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/notificationd/notification_daemon.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <brillo/bind_lambda.h>

#include "vm_tools/notificationd/notification_shell_client.h"

namespace {

constexpr char kNotificationsServerName[] = "notificationd";
constexpr char kNotificationsVendor[] = "Chromium OS";
constexpr char kNotificationsVersion[] = "1.0";
constexpr char kNotificationsSpecVersion[] = "1.2";

}  // namespace

namespace vm_tools {
namespace notificationd {

// static
std::unique_ptr<NotificationDaemon> NotificationDaemon::Create(
    const std::string& display_name,
    const std::string& virtwl_device,
    base::Closure quit_closure) {
  auto daemon = base::WrapUnique(new NotificationDaemon());

  if (!daemon->Init(display_name, virtwl_device, std::move(quit_closure))) {
    LOG(ERROR) << "Failed to initialize notification daemon";
    return nullptr;
  }

  return daemon;
}

bool NotificationDaemon::Init(const std::string& display_name,
                              const std::string& virtwl_device,
                              base::Closure quit_closure) {
  notification_shell_client_ = NotificationShellClient::Create(
      display_name, virtwl_device, std::move(quit_closure));
  if (!notification_shell_client_) {
    LOG(ERROR) << "Failed to create notification shell client";
    return false;
  }

  dbus_service_ = DBusService::Create(this);
  if (!dbus_service_) {
    LOG(ERROR) << "Failed to create D-BUS service";
    return false;
  }

  return true;
}

bool NotificationDaemon::GetCapabilities(
    std::vector<std::string>* out_capabilities) {
  out_capabilities->emplace_back("body");
  return true;
}

bool NotificationDaemon::Notify(const NotifyArgument& input, uint32_t* out_id) {
  // Forward notification request to host via Wayland.
  if (!notification_shell_client_->SendNotification(
          input.summary, input.body, input.app_name,
          std::to_string(id_count_))) {
    LOG(ERROR) << "Failed to request create_notification to host";
    return false;
  }
  *out_id = id_count_;
  id_count_++;
  return true;
}

bool NotificationDaemon::GetServerInformation(ServerInformation* output) {
  output->name = kNotificationsServerName;
  output->vendor = kNotificationsVendor;
  output->version = kNotificationsVersion;
  output->spec_version = kNotificationsSpecVersion;

  return true;
}

}  // namespace notificationd
}  // namespace vm_tools
