// Copyright 2010 Google Inc. All Rights Reserved.
// Author: tbarzic@google.com (Toni Barzic)

#include "image_burner/service.h"

#include <dbus/dbus-glib-lowlevel.h>

#include <chromeos/dbus/service_constants.h>

#include <errno.h>
#include <fstream>
#include <glib.h>
#include <string>
#include <zlib.h>

#include <iostream>

#include "image_burner/interface.h"
#include "image_burner/marshal.h"


namespace imageburn {  // NOLINT
namespace gobject {  // NOLINT
#include "image_burner/bindings/server.h"
}  // namespace gobject
}  // namespace imageburn

namespace imageburn {

ImageBurnService::ImageBurnService() : image_burner_(),
                                       main_loop_(NULL),
                                       shutting_down_(false) { }

ImageBurnService::~ImageBurnService() {
  if (main_loop_)
    g_main_loop_unref(main_loop_);
  if (image_burner_)
    g_object_unref(image_burner_);
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

GObject *ImageBurnService::service_object() const{
  return G_OBJECT(image_burner_);
}

bool ImageBurnService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(gobject::image_burner_get_type(),
                                  &gobject::dbus_glib_image_burner_object_info);

  signals_[kSignalBurnUpdate] =
        g_signal_new(kSignalBurnUpdateName,
                     gobject::image_burner_get_type(),
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     image_burner_VOID__STRING_INT64_INT64,
                     G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_INT64, G_TYPE_INT64);

  signals_[kSignalBurnFinished] =
        g_signal_new(kSignalBurnFinishedName,
                     gobject::image_burner_get_type(),
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     image_burner_VOID__STRING_BOOLEAN_STRING,
                     G_TYPE_NONE, 3, G_TYPE_STRING,G_TYPE_BOOLEAN,G_TYPE_STRING);

  return Reset();
}

bool ImageBurnService::Reset() {
  if (image_burner_)
    g_object_unref(image_burner_);
  image_burner_ = reinterpret_cast<gobject::ImageBurner*>(
      g_object_new(gobject::image_burner_get_type(), NULL));
  // Allow references to this instance.
  image_burner_->service = this;

  if (main_loop_) {
    ::g_main_loop_unref(main_loop_);
  }
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
  int pid = fork();
if (pid == 0){
  return true;
} else {
  std::cout<<"Burning "<<from_path<<" : "<<to_path<<std::endl;
  bool s=DoBurn(from_path, to_path);
  std::cout<<"Burn complete"<<std::endl;
  return s;
}
}

bool ImageBurnService::DoBurn(gchar* from_path, gchar* to_path){
  std::string error = "";
  bool success = true;

  gzFile from_file = gzopen(from_path, "rb");
  if (!from_file){
    int gz_err = Z_OK;
    error += "Couldn't open"+static_cast<std::string>(from_path)+"\n"+
        static_cast<std::string>(gzerror(from_file, &gz_err))+"/n";
    success = false;
  }
  FILE* to_file = NULL;
  if(success){
    to_file = fopen(to_path, "wb");
    if (!to_file){
      error += "Couldn't open"+static_cast<std::string>(to_path)+"\n"+
          static_cast<std::string>(strerror(errno))+"\n";
      success = false;
    }
  }

  if(success){
    int  block_size=1<<22;
    char buffer[block_size];

    int len = 0;
    bool success=true;
    int64 total_burnt = 0;
    int64 image_size = GetTotalImageSize(from_path);//1549813760;
    while ((len=gzread(from_file, buffer, sizeof(buffer)))> 0){
       if(shutting_down_)
         return false;
       if(fwrite(buffer,1,len*sizeof(char),to_file) == static_cast<size_t>(len)){
        total_burnt += static_cast<int64>(len);

       SendProgressSignal(total_burnt, image_size, to_path);
      } else {
        success = false;
        error += "Unable to write to "+static_cast<std::string>(to_path)+"\n"+
            static_cast<std::string>(strerror(errno))+"\n";
        break;
      }
    }
    if(len != 0){
      int gz_err = Z_OK;
      error += static_cast<std::string>(gzerror(from_file, &gz_err))+"\n";
    }
  }

  if(from_file){
    if((gzclose(from_file)) != 0){
      success = false;
      int gz_err = Z_OK;
      error += "Couldn't close "+static_cast<std::string>(from_path)+"\n"+
          static_cast<std::string>(gzerror(from_file, &gz_err))+"\n";
    }
  }


  if(to_file){
    if(fclose(to_file) != 0){
      success = false;
      error += "Couldn't close "+static_cast<std::string>(to_path)+"\n"+
          static_cast<std::string>(strerror(errno))+"\n";
    }
  }

  SendFinishedSignal(to_path, success, error.c_str());

  return success;
}

void ImageBurnService::SendFinishedSignal(const char* target_path, bool success,
                                          const char* error){
 if(error){
   std::cout<<"End "<<success<<" : "<<error<<std::endl;
 }else {
   std::cout<<"End "<<success<<" : "<<"no error"<<std::endl;

 }
 std::flush(std::cout);
  g_signal_emit(image_burner_,
                signals_[kSignalBurnFinished],
                     0, target_path, success, error);
}

void ImageBurnService::SendProgressSignal(int64 amount_burnt, int64 total_size,
                                          const char* target_path){

  g_signal_emit(image_burner_,
                signals_[kSignalBurnUpdate],
                0, target_path, amount_burnt, total_size);
}

int64 ImageBurnService::GetTotalImageSize(gchar* from_path){
  FILE* from_file=fopen(from_path,"rb");
  int s=0;
  fseek(from_file,-sizeof(s),SEEK_END);
  if (fread(&s,1,sizeof(s),from_file) != sizeof(s)){
    s = 0;
  }
  fclose(from_file);
  return static_cast<int64>(s);
}
}//namespace imageburn
