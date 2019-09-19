// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image-burner/image_burn_service.h"

#include <ctype.h>
#include <glib.h>

#include <string>

#include <base/logging.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "image-burner/image_burner.h"

namespace imageburn {
#include "bindings/image_burner.dbusserver.h"  // NOLINT(build/include_alpha)

namespace {
// Update signal is emitted only when there is at least
// |kProgressSignalInterval| bytes progress.
const int kProgressSignalInterval = 100 * 1024;  // 100 KB
}  // namespace

ImageBurnService::ImageBurnService(BurnerImpl* burner_impl)
    : image_burner_(),
      main_loop_(NULL),
      amount_burnt_for_next_signal_(0),
      burning_(false),
      burner_impl_(burner_impl) {
  LOG(INFO) << "Image Burn Service created";
}

ImageBurnService::~ImageBurnService() {
  Cleanup();
}

const char* ImageBurnService::service_name() const {
  return kImageBurnServiceName;
}

const char* ImageBurnService::service_path() const {
  return kImageBurnServicePath;
}

const char* ImageBurnService::service_interface() const {
  return kImageBurnServiceInterface;
}

GObject* ImageBurnService::service_object() const {
  return G_OBJECT(image_burner_);
}

bool ImageBurnService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(image_burner_get_type(),
                                  &dbus_glib_image_burner_object_info);

  signals_[kSignalBurnUpdate] =
      g_signal_new(kSignalBurnUpdateName, image_burner_get_type(),
                   G_SIGNAL_RUN_FIRST, 0, NULL, NULL, nullptr, G_TYPE_NONE, 3,
                   G_TYPE_STRING, G_TYPE_INT64, G_TYPE_INT64);
  signals_[kSignalBurnFinished] =
      g_signal_new(kSignalBurnFinishedName, image_burner_get_type(),
                   G_SIGNAL_RUN_FIRST, 0, NULL, NULL, nullptr, G_TYPE_NONE, 3,
                   G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);

  return Reset();
}

bool ImageBurnService::Reset() {
  LOG(INFO) << "Reseting Image Burn Service";
  Cleanup();

  image_burner_ = reinterpret_cast<ImageBurner*>(
      g_object_new(image_burner_get_type(), NULL));
  // Allow references to this instance.
  image_burner_->service = this;

  main_loop_ = g_main_loop_new(NULL, false);
  if (!main_loop_) {
    LOG(ERROR) << "Failed to create main loop for Image Burn Service";
    return false;
  }
  return true;
}

bool ImageBurnService::Shutdown() {
  return brillo::dbus::AbstractDbusService::Shutdown();
}

gboolean ImageBurnService::BurnImageAsync(gchar* from_path,
                                          gchar* to_path,
                                          DBusGMethodInvocation* context) {
  std::string error_str;
  bool success = true;
  if (!burner_impl_) {
    error_str = "Burner not set";
    success = false;
  }
  if (!burning_) {
    burning_ = true;
  } else {
    error_str = "Another burn in progress.";
    success = false;
  }

  if (success) {
    dbus_g_method_return(context);
    amount_burnt_for_next_signal_ = 0;
    burner_impl_->BurnImage(from_path, to_path);
    burning_ = false;
  } else {
    GError* error = NULL;
    SetError(error_str, &error);
    dbus_g_method_return_error(context, error);
    g_error_free(error);
    return false;
  }
  return true;
}

void ImageBurnService::Cleanup() {
  if (main_loop_) {
    g_main_loop_unref(main_loop_);
    main_loop_ = NULL;
  }
  if (image_burner_) {
    g_object_unref(image_burner_);
    image_burner_ = NULL;
  }
}

void ImageBurnService::SendFinishedSignal(const char* target_path,
                                          bool success,
                                          const char* error_message) {
  if (!image_burner_) {
    LOG(WARNING) << "Finished signal not sent due to sender not being "
                 << "initialized";
    return;
  }
  g_signal_emit(image_burner_, signals_[kSignalBurnFinished], 0, target_path,
                success, error_message);
}

void ImageBurnService::SendProgressSignal(int64_t amount_burnt,
                                          int64_t total_size,
                                          const char* target_path) {
  if (!image_burner_) {
    LOG(WARNING) << "Progress signal not sent due to sender not being "
                 << "initialized";
    return;
  }
  // Send signal only when there is at least |kProgressSignalInterval| bytes
  // progress.
  if (amount_burnt >= amount_burnt_for_next_signal_) {
    g_signal_emit(image_burner_, signals_[kSignalBurnUpdate], 0, target_path,
                  amount_burnt, total_size);
    amount_burnt_for_next_signal_ = amount_burnt + kProgressSignalInterval;
  }
}

void ImageBurnService::SetError(const std::string& message, GError** error) {
  g_set_error_literal(error, g_quark_from_static_string("image-burn-quark"), 0,
                      message.c_str());
}

}  // namespace imageburn
