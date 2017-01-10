// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader.h"

#include <sysexits.h>
#include <libminijail.h>

#include <string>

#include <chromeos/dbus/service_constants.h>

namespace imageloader {

namespace {
constexpr char kSeccompFilterPath[] =
    "/opt/google/imageloader/imageloader-seccomp.policy";
}  // namespace

// static
const char ImageLoader::kImageLoaderGroupName[] = "imageloaderd";
const char ImageLoader::kImageLoaderUserName[] = "imageloaderd";
const int ImageLoader::kShutdownTimeoutMilliseconds = 20000;

ImageLoader::ImageLoader(ImageLoaderConfig config)
    : DBusServiceDaemon(kImageLoaderServiceName,
                        "/org/chromium/ImageLoader/Manager"),
      impl_(std::move(config)) {}

ImageLoader::~ImageLoader() {}

int ImageLoader::OnInit() {
  // Run with minimal privileges.
  struct minijail* jail = minijail_new();
  minijail_no_new_privs(jail);
  minijail_use_seccomp_filter(jail);
  minijail_parse_seccomp_filters(jail, kSeccompFilterPath);
  minijail_reset_signal_mask(jail);
  minijail_namespace_ipc(jail);
  minijail_namespace_net(jail);
  minijail_namespace_user(jail);
  minijail_remount_proc_readonly(jail);
  CHECK_EQ(0, minijail_change_user(jail, kImageLoaderUserName));
  CHECK_EQ(0, minijail_change_group(jail, kImageLoaderGroupName));
  minijail_enter(jail);

  int return_code = brillo::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  PostponeShutdown();

  return EX_OK;
}

void ImageLoader::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  dbus_object_.reset(new brillo::dbus_utils::DBusObject(
      object_manager_.get(), object_manager_->GetBus(),
      org::chromium::ImageLoaderInterfaceAdaptor::GetObjectPath()));
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("ImageLoader.RegisterAsync() failed.", true));
}

void ImageLoader::OnShutdown(int* return_code) {
  brillo::DBusServiceDaemon::OnShutdown(return_code);
}

void ImageLoader::PostponeShutdown() {
  shutdown_callback_.Reset(base::Bind(&brillo::Daemon::Quit,
                                      base::Unretained(this)));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, shutdown_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMilliseconds));
}

bool ImageLoader::RegisterComponent(
    brillo::ErrorPtr* err, const std::string& name, const std::string& version,
    const std::string& component_folder_abs_path, bool* out_success) {
  *out_success =
      impl_.RegisterComponent(name, version, component_folder_abs_path);
  PostponeShutdown();
  return true;
}

bool ImageLoader::GetComponentVersion(brillo::ErrorPtr* err,
                                      const std::string& name,
                                      std::string* out_version) {
  *out_version = impl_.GetComponentVersion(name);
  PostponeShutdown();
  return true;
}

bool ImageLoader::LoadComponent(brillo::ErrorPtr* err, const std::string& name,
                                std::string* out_mount_point) {
  *out_mount_point = impl_.LoadComponent(name);
  PostponeShutdown();
  return true;
}

}  // namespace imageloader
