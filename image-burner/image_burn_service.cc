// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. 

#include "image-burner/image_burn_service.h"

#include <errno.h>
#include <fstream>
#include <string>
#include <zlib.h>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <cros/chromeos_cros_api.h>
#include <cros/chromeos_mount.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include "image-burner/image_burner.h"
#include "image-burner/marshal.h"

namespace imageburn {
#include "image-burner/bindings/server.h"

const int kBurningBlockSize = 4 * 1024; //4 kB
// after every kSentSignalRatio IO operations, update signal will be emited
const int kSentSignalRatio = 256;
const char* kDevPath = "/dev/";

ImageBurnService::ImageBurnService() : image_burner_(),
                                       main_loop_(NULL),
                                       shutting_down_(false),
                                       burning_(false) {
  LoadLibcros();
  LOG(INFO) << "Service initialized";
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
                     3, G_TYPE_STRING,G_TYPE_BOOLEAN,G_TYPE_STRING);
  return Reset();
}

bool ImageBurnService::Reset() {
  Cleanup();

  image_burner_ = reinterpret_cast<ImageBurner*>(
      g_object_new(image_burner_get_type(), NULL));
  // Allow references to this instance.
  image_burner_->service = this;

  main_loop_ = g_main_loop_new(NULL, false);
  if (!main_loop_) {
    LOG(ERROR) << "Failed to create main loop";
    return false;
  }
  return true;
}

bool ImageBurnService::Shutdown(){
  shutting_down_ = true;
  chromeos::dbus::AbstractDbusService::Shutdown();
}

gboolean ImageBurnService::BurnImage(gchar* from_path, gchar* to_path,
                                     GError** error){
  if (burning_) { 
    g_set_error_literal(error, 0, 0, "Another burn in progress");
    return false;
  } else {
    burning_ = true;
  }

  scoped_ptr<BurnArguments> args(new BurnArguments);
  args->from_path = from_path;
  args->to_path = to_path;
  args->service = this;
  g_timeout_add(1,&ImageBurnService::BurnImageTimeoutCallback, args.release());
  return true;  
}

gboolean ImageBurnService::BurnImageTimeoutCallback(gpointer data) { 
  scoped_ptr<BurnArguments> args(static_cast<BurnArguments*>(data)); 
  args->service->InitiateBurning(args->from_path.c_str(),
                                 args->to_path.c_str());
  return false;
}

bool ImageBurnService::LoadLibcros() {
  std::string load_error_string;
  static bool loaded = false;
  if (loaded) {
    LOG(INFO) << "Already loaded libcros...";
    return true;
  }

  loaded = chromeos::LoadLibcros(chromeos::kCrosDefaultPath, load_error_string);

  if (!loaded) {
    LOG(ERROR) << "Problem loading chromeos shared object: "
               << load_error_string;
  }
  return loaded;
}

void ImageBurnService::Cleanup(){
  if (main_loop_) {
    g_main_loop_unref(main_loop_);
    main_loop_ = NULL;
  }
  if (image_burner_) {
    g_object_unref(image_burner_);
    image_burner_ = NULL;
  }
}

void ImageBurnService::InitiateBurning(const char* from_path, 
                                       const char* to_path) {
  bool success = UnmountDevice(to_path);
  std::string err;
  if (success) {
    success = DoBurn(from_path, to_path, &err);
    LOG(INFO) << (success ? "Burn complete" : "Burn failed");
  } else {
    err = "Unable to unmount target path";
    success = false;
  }

  SendFinishedSignal(to_path, success, err.c_str());
  burning_ = false;
}

bool ImageBurnService::UnmountDevice(const char* device_path){
  LOG(INFO) << "Unmounting " << device_path;
  chromeos::MountStatus* status = chromeos::RetrieveMountInformation();
  
  for (int i = 0; i < status->size; ++i) {
    FilePath device_system_path =
        static_cast<FilePath>(status->disks[i].systempath).DirName();
    std::string device_target_path(kDevPath);
    device_target_path.append(device_system_path.BaseName().value());
    if (status->disks[i].mountpath &&
        device_target_path == device_path) {
      if (!chromeos::UnmountDevicePath(status->disks[i].path)) {
        LOG(INFO) << "Unmounting " << status->disks[i].path << " failed";
        return false;
      } else {
        LOG(INFO) << status->disks[i].path << " unmounted";
      }
    }
  }
  LOG(INFO) << "Unmounting successful";
  return true;
}

bool ImageBurnService::DoBurn(const char* from_path, 
                              const char* to_path, 
                              std::string* err) {
  LOG(INFO) << "Burning " << from_path << " to " << to_path;

  bool success = true;

  gzFile from_file = gzopen(from_path, "rb");
  if (!from_file) {
    int gz_err = Z_OK;
    err->append("Couldn't open" + static_cast<std::string>(from_path) + 
        "\n" + static_cast<std::string>(gzerror(from_file, &gz_err)) + "\n");
    LOG(WARNING) << "Couldn't open" << from_path << " : " << 
        gzerror(from_file, &gz_err);
    success = false;
  } else {
    LOG(INFO) << from_path << " opened";
  }
  
  FILE* to_file = NULL;
  if (success) {
    to_file = fopen(to_path, "wb");
    if (!to_file) {
      err->append("Couldn't open" + static_cast<std::string>(to_path) + 
          "\n" + static_cast<std::string>(strerror(errno)) + "\n");
      LOG(WARNING) << "Couldn't open" << to_path << " : " << strerror(errno);
      success = false;
    } else {
      LOG(INFO) << to_path << " opened";
    }
  }
  if (success) {
    scoped_array<char> buffer(new char[kBurningBlockSize]);
    LOG(INFO) << "a" << sizeof(buffer.get());
    int len = 0;
    bool success = true;
    int64 total_burnt = 0;
    int64 image_size = GetTotalImageSize(from_path);
    int i = kSentSignalRatio;
    while ((len = gzread(from_file, buffer.get(), 
                         kBurningBlockSize*sizeof(char))) > 0) {
       if (shutting_down_)
         return false;
       if (fwrite(buffer.get(), 1, len * sizeof(char), to_file) == 
           static_cast<size_t>(len)) {
         total_burnt += static_cast<int64>(len);
         fflush(to_file);
        if (i == kSentSignalRatio) { 
          SendProgressSignal(total_burnt, image_size, to_path);
          i = 0;
        } else {
          ++i;
        }
      } else {
        success = false;
        err->append("Unable to write to " + static_cast<std::string>(to_path) + 
            "\n" + static_cast<std::string>(strerror(errno)) + "\n");
        LOG(WARNING) << "Unable to write to " << to_path << " : " << 
            strerror(errno);
        break;
      }
    }
    if (len != 0) {
      int gz_err = Z_OK;
      err->append(static_cast<std::string>(gzerror(from_file, &gz_err)) + "\n");
      LOG(WARNING) << gzerror(from_file, &gz_err);
    }
  }

  if (from_file) {
    if ((gzclose(from_file)) != 0) {
      success = false;
      int gz_err = Z_OK;
      err->append("Couldn't close " + static_cast<std::string>(from_path) + 
          "\n" + static_cast<std::string>(gzerror(from_file, &gz_err)) + "\n");
      LOG(WARNING) << "Couldn't close" << from_path << " : " << 
          gzerror(from_file, &gz_err);
    } else {
      LOG(INFO) << from_path << " closed";
    }
  }

  if (to_file) {
    if (fclose(to_file) != 0) {
      success = false;
      err->append("Couldn't close " + static_cast<std::string>(to_path) + 
          "\n" + static_cast<std::string>(strerror(errno)) + "\n");
      LOG(WARNING) << "Couldn't close" << to_path << " : " << strerror(errno);
    } else {
      LOG(INFO) << to_path << " closed";
    }
  }

  return success;
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

int64 ImageBurnService::GetTotalImageSize(const char* from_path) {
  FILE* from_file = fopen(from_path, "rb");
  int result = 0;
  fseek(from_file, -sizeof(result), SEEK_END);
  if (fread(&result, 1, sizeof(result), from_file) != sizeof(result)) {
    result = 0;
  }
  fclose(from_file);
  return static_cast<int64>(result);
}

}  // namespace imageburn

