// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloadclient.h"

#include <signal.h>

#include <iostream>
#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <pthread.h>

#include "imageloader_common.h"

namespace imageloader {

ImageLoadClient::ImageLoadClient(DBus::Connection* conn, const char* path,
                                 const char* name)
    : DBus::ObjectProxy(*conn, path, name) {}

void ImageLoadClient::RegisterComponentCallback(const bool& success,
                                                const ::DBus::Error& err,
                                                void*) {
  if (success) {
    LOG(INFO) << "Success.";
  } else {
    LOG(INFO) << "Failure.";
  }
}

void ImageLoadClient::GetComponentVersionCallback(const std::string& version,
                                                  const ::DBus::Error& err,
                                                  void*) {
  if (version == kBadResult) {
    LOG(INFO) << "Failure.";
  } else {
    LOG(INFO) << "Version = " << version;
  }
}

namespace {

void *TestCalls(void *arg) {
  ImageLoadClient *client = reinterpret_cast<ImageLoadClient *>(arg);
  while (1) {
    std::string inp, name, abs_path, rel_path, version;
    std::cin >> inp;
    if (inp == "rc") {  // RegisterComponent
      std::cin >> name;
      std::cin >> version;
      std::cin >> rel_path;
      char* c_abs_path = realpath(rel_path.c_str(), NULL);
      if (c_abs_path != NULL) {
        abs_path = std::string(c_abs_path);
        client->RegisterComponentAsync(name, version, abs_path, NULL);
        free(c_abs_path);
      } else {
        PLOG(ERROR) << "realpath : " << rel_path;
      }
    } else if (inp == "gcv") {  // GetComponentVersion
      std::cin >> name;
      client->GetComponentVersionAsync(name, NULL);
    }
  }
  return NULL;
}

}  // namespace {}

}  // namespace imageloader

int main(int argc, char** argv) {
  signal(SIGTERM, imageloader::OnQuit);
  signal(SIGINT, imageloader::OnQuit);

  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  DBus::_init_threading();
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();

  imageloader::ImageLoadClient client(&conn, imageloader::kImageLoaderPath,
                                      imageloader::kImageLoaderName);

  pthread_t thread;
  pthread_create(&thread, NULL, imageloader::TestCalls, &client);

  dispatcher.enter();
  LOG(INFO) << "Exiting ...";

  return 0;
}
