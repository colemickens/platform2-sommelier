// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/basictypes.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <glib.h>
#include <iostream>
#include <vector>
#include <utility>
#include <string>

#include "image-burner/marshal.h"
#include "image-burner/bindings/client.h"


namespace chromeos {

class TestClient {
 public:
  typedef dbus::MonitorConnection<void(const char*, const int64, const int64)>*
      ConnectionUpdateType;
  typedef dbus::MonitorConnection<void(const char*, const bool, const char*)>*
      ConnectionFinishedType;
  static void Update(void* object, const char* target_path,
                     const int64 amount_burnt, const int64 total_size) {
    std::cout << target_path << " " << amount_burnt << " "
              <<total_size << std::endl;
    flush(std::cout);
  }

  TestClient(dbus::Proxy proxy, GMainLoop* loop) : burn_proxy_(proxy),
                                                   loop_(loop),
                                                   tests_it_(tests_.end()) {
    ::dbus_g_proxy_add_signal(burn_proxy_.gproxy(),
                              "burn_progress_update",
                              G_TYPE_STRING,
                              G_TYPE_INT64,
                              G_TYPE_INT64,
                              G_TYPE_INVALID);

    typedef dbus::MonitorConnection<void(const char*, const int64, const int64)>
       ConnectionUpdateTypeRef;
    ConnectionUpdateTypeRef *update =
        new ConnectionUpdateTypeRef(burn_proxy_, "burn_progress_update",
                                    &TestClient::Update, this);
    ::dbus_g_proxy_connect_signal(burn_proxy_.gproxy(),
                                  "burn_progress_update",
                                  G_CALLBACK(&ConnectionUpdateTypeRef::Run),
                                  update, NULL);
    updateconnection_ = update;

    // adding finished signal and connection
    ::dbus_g_proxy_add_signal(burn_proxy_.gproxy(),
                              "burn_finished",
                              G_TYPE_STRING,
                              G_TYPE_BOOLEAN,
                              G_TYPE_STRING,
                              G_TYPE_INVALID);

    typedef dbus::MonitorConnection<void(const char*, const bool, const char*)>
        ConnectionFinishTypeRef;
    ConnectionFinishTypeRef *finished =
        new ConnectionFinishTypeRef(burn_proxy_, "burn_finished",
                                    &TestClient::Finished, this);
    ::dbus_g_proxy_connect_signal(burn_proxy_.gproxy(),
                                  "burn_finished",
                                  G_CALLBACK(&ConnectionFinishTypeRef::Run),
                                  finished, NULL);
    finishedconnection_ = finished;
  }


  ~TestClient() {
    dbus::Disconnect(updateconnection_);
    dbus::Disconnect(finishedconnection_);
  }

  void StartTests() {
    tests_.resize(0);

    AddTest(
      "/home/chronos/user/Downloads/chromeos_image/chroemos_image.bin.gz",
      "/usr/local/chroemos_image.bin.gz");

    AddTest(
      "/home/chronos/user/Downloads/chromeos_image/chroemos_image.bin.gz",
      "/dev/sda");

    AddTest(
      "/home/chronos/user/Downloads/chromeos_image/chroemos_image.bin.gz",
      "/dev/sda1");

    AddTest(
      "/home/chronos/user/Downloads/chromeos_image/chroemos_image.bin.gz",
      "/dev/sdb");

    tests_it_ = tests_.begin();
    RunNext();
  }

  void RunNext() {
    if (tests_it_ == tests_.end()) {
      ::g_main_loop_quit(loop_);
    } else {
      RunOne();
    }
  }

 private:
  void AddTest(const char* to, const char* from) {
    tests_.push_back(std::make_pair(to, from));
  }

  void RunOne() {
    glib::ScopedError error;
    std::cout << "start" << std::endl;
    if (!::dbus_g_proxy_call(burn_proxy_.gproxy(),
                            "BurnImage",
                             &Resetter(&error).lvalue(),
                             G_TYPE_STRING, tests_it_->first.c_str(),
                             G_TYPE_STRING, tests_it_->second.c_str(),
                             G_TYPE_INVALID,
                             G_TYPE_INVALID)) {
      std::cout << "Burning failed" << error->message << std::endl;
    } else {
      std::cout << "Burning started" << std::endl;
    }
    std::cout << "end+++++++++++++++++++++++++++" << std::endl;
    ++tests_it_;
  }

  static void Finished(void* object, const char* target_path,
                       const bool success, const char* error ) {
    std::cout << target_path << " " << success << " " << error << std::endl;

    TestClient* self = static_cast<TestClient*>(object);
    self->RunNext();
  }

  dbus::Proxy burn_proxy_;
  GMainLoop *loop_;
  ConnectionUpdateType updateconnection_;
  ConnectionFinishedType finishedconnection_;
  typedef std::vector<std::pair<std::string, std::string> > IBTestVec;
  IBTestVec tests_;
  IBTestVec::iterator tests_it_;
};

}  // namespace chromeos

int main(int argc, char* argv[]) {
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, true);
  dbus_g_object_register_marshaller(image_burner_VOID__STRING_BOOLEAN_STRING,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING,
                                    G_TYPE_INVALID);
  dbus_g_object_register_marshaller(image_burner_VOID__STRING_INT64_INT64,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_INT64,
                                    G_TYPE_INT64,
                                    G_TYPE_INVALID);

  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy burn_proxy(bus,
                                  "org.chromium.ImageBurner",
                                  "/org/chromium/ImageBurner",
                                  "org.chromium.ImageBurnerInterface");
  chromeos::TestClient test(burn_proxy, loop);

  test.StartTests();
  ::g_main_loop_run(loop);

  return 0;
}

