// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image-burner/image_burn_service.h"

#include <cstring>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>

#include <base/basictypes.h>
#include <base/logging.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <fstream>
#include <string>

#include "image-burner/image_burner.h"
#include "image-burner/marshal.h"

namespace imageburn {
#include "image-burner/bindings/server.h"

const int kBurningBlockSize = 4 * 1024;  // 4 KiB
// after every kSentSignalRatio IO operations, update signal will be emited
const int kSentSignalRatio = 256;
const int kFsyncRatio = 1024;

const char* kFilePathPrefix = "/dev/sd";
const char* kRootDevicePath = "/dev/sda";

ImageBurnService::ImageBurnService() : image_burner_(),
                                       main_loop_(NULL),
                                       shutting_down_(false),
                                       burning_(false) {
  LOG(INFO) << "Image Burn Service initialized";
}

ImageBurnService::~ImageBurnService() {
  Cleanup();
}

const char *ImageBurnService::service_name() const {
  return kImageBurnServiceName;
}

const char *ImageBurnService::service_path() const {
  return kImageBurnServicePath;
}

const char *ImageBurnService::service_interface() const {
  return kImageBurnServiceInterface;
}

GObject *ImageBurnService::service_object() const {
  return G_OBJECT(image_burner_);
}

bool ImageBurnService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(image_burner_get_type(),
                                  &dbus_glib_image_burner_object_info);

  signals_[kSignalBurnUpdate] =
        g_signal_new(kSignalBurnUpdateName,
                     image_burner_get_type(),
                     G_SIGNAL_RUN_FIRST,
                     0, NULL, NULL,
                     image_burner_VOID__STRING_INT64_INT64, G_TYPE_NONE,
                     3, G_TYPE_STRING, G_TYPE_INT64, G_TYPE_INT64);
  signals_[kSignalBurnFinished] =
        g_signal_new(kSignalBurnFinishedName,
                     image_burner_get_type(),
                     G_SIGNAL_RUN_FIRST,
                     0, NULL, NULL,
                     image_burner_VOID__STRING_BOOLEAN_STRING, G_TYPE_NONE,
                     3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
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
  shutting_down_ = true;
  chromeos::dbus::AbstractDbusService::Shutdown();
}

gboolean ImageBurnService::BurnImageAsync(gchar* from_path, gchar* to_path,
    DBusGMethodInvocation * context) {
  std::string error_str;
  bool success = true;
  if (!from_path) {
    error_str = "Source path set to NULL.";
    success = false;
  } else if (!to_path) {
    error_str = "Destination path set to NULL.";
    success = false;
  } else {
    success = BurnImageImpl(from_path, to_path, &error_str);
  }

  if (success) {
    dbus_g_method_return(context);
    return true;
  } else {
    GError* error = NULL;
    SetError(error_str, &error);
    dbus_g_method_return_error(context, error);
    g_error_free(error);
    return false;
  }
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

void ImageBurnService::SetError(const std::string& message, GError** error) {
  g_set_error_literal(error, g_quark_from_static_string("image-burn-quark"), 0,
      message.c_str());
}

bool ImageBurnService::ValidateTargetPath(const char* target_path,
     std::string* error) {
  if (strncmp(target_path, kRootDevicePath, strlen(kRootDevicePath)) == 0) {
    *error = "Target path is on root device.";
    LOG(ERROR) << *error;
    return false;
  }
  int file_prefix_len = strlen(kFilePathPrefix);
  if (strncmp(target_path, kFilePathPrefix, file_prefix_len) != 0 ||
      strlen(target_path) == file_prefix_len) {
    *error = "Target path is not valid file path.";
    LOG(ERROR) << *error;
    return false;
  }
  int i = file_prefix_len;
  while (target_path[i]) {
    if (!islower(target_path[i])) {
      *error = "Target path is not valid file path.";
      LOG(ERROR) << *error;
      return false;
    }
    i++;
  }
  return true;
}

bool ImageBurnService::BurnImageImpl(const char* from_path, const char* to_path,
    std::string* error) {
  error->clear();
  if (burning_) {
    (*error) = "Another burn in progress.";
    return false;
  }
  if (!ValidateTargetPath(to_path, error)){
    return false;
  }
  burning_ = true;

  from_path_ = from_path;
  to_path_ = to_path;

  scoped_ptr<BurnArguments> args(new BurnArguments);
  args->from_path = from_path_.c_str();
  args->to_path = to_path_.c_str();
  args->service = this;
  g_timeout_add(1, &ImageBurnService::BurnImageTimeoutCallback,
      args.release());
  return true;
}

// static.
gboolean ImageBurnService::BurnImageTimeoutCallback(gpointer data) {
  scoped_ptr<BurnArguments> args(static_cast<BurnArguments*>(data));
  args->service->InitiateBurning(args->from_path.c_str(),
                                 args->to_path.c_str());
  return false;
}

void ImageBurnService::InitiateBurning(const char* from_path,
                                       const char* to_path) {

  std::string err;
  bool success = DoBurn(from_path, to_path, &err);

  SendFinishedSignal(to_path, success, err.c_str());
  burning_ = false;
}

bool ImageBurnService::DoBurn(const char* from_path,
                              const char* to_path,
                              std::string* err) {
  LOG(INFO) << "Burning " << from_path << " to " << to_path;

  bool success = true;

  FILE* from_file = fopen(from_path, "rb");
  if (!from_file) {
    ProcessFileError("Couldn't open ", from_path, err);
    success = false;
  } else {
    LOG(INFO) << from_path << " opened";
  }

  FILE* to_file = NULL;
  if (success) {
    to_file = fopen(to_path, "wb");
    if (!to_file) {
      ProcessFileError("Couldn't open ", to_path, err);
      success = false;
    } else {
      LOG(INFO) << to_path << " opened";
    }
  }

  if (success) {
    scoped_array<char> buffer(new char[kBurningBlockSize]);
    int len = 0;
    int64 total_burnt = 0;
    int64 image_size = GetTotalImageSize(from_file);
    int i = kSentSignalRatio;

    while ((len = fread(buffer.get(), sizeof(char), kBurningBlockSize,
        from_file)) > 0) {
      if (shutting_down_)
        return false;
      if (fwrite(buffer.get(), sizeof(char), len, to_file) ==
          static_cast<size_t>(len)) {
        total_burnt += static_cast<int64>(len);

        if (!(i %  kSentSignalRatio)) {
          SendProgressSignal(total_burnt, image_size, to_path);
        }

        if (i == kFsyncRatio) {
          if (fsync(fileno(to_file)) != 0) {
            success = false;
            ProcessFileError("Unable to write to ", to_path, err);
            break;
          }
          i = 0;
        } else {
          ++i;
        }
      } else {
        success = false;
        ProcessFileError("Unable to write to ", to_path, err);
        break;
      }
    }
  }

  if (to_file) {
   if (fclose(to_file) != 0) {
      success = false;
      ProcessFileError("Couldn't close", to_path, err);
    } else {
      LOG(INFO) << to_path << " closed";
    }
  }

  if (from_file) {
    if ((fclose(from_file)) != 0) {
      success = false;
      ProcessFileError("Couldn't close", from_path, err);
    } else {
      LOG(INFO) << from_path << " closed";
    }
  }

  return success;
}

void ImageBurnService::ProcessFileError(const std::string& error,
    const std::string& path, std::string* error_message) {
  error_message->append(error);
  error_message->append(path);
  error_message->append(":");
  error_message->append(strerror(errno));
  LOG(WARNING) << error << path << ": " << strerror(errno);
}

void ImageBurnService::SendFinishedSignal(const char* target_path, bool success,
                                          const char* error) {
  g_signal_emit(image_burner_,
                signals_[kSignalBurnFinished],
                0, target_path, success, error);
}

void ImageBurnService::SendProgressSignal(int64 amount_burnt, int64 total_size,
                                          const char* target_path) {
  g_signal_emit(image_burner_,
                signals_[kSignalBurnUpdate],
                0, target_path, amount_burnt, total_size);
}

int64 ImageBurnService::GetTotalImageSize(FILE* file) {
  int64 result = 0;
  fseek(file, 0, SEEK_END);
  result = static_cast<int64>(ftell(file));
  fseek(file, 0, SEEK_SET);
  return result;
}

}  // namespace imageburn
