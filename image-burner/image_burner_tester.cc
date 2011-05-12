// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/basictypes.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <vector>
#include <utility>
#include <string>

#include "image-burner/marshal.h"
#include "image-burner/bindings/client.h"


namespace chromeos {

static const char* const kImgSrc =
    "/home/chronos/user/Downloads/chromeos_image.imgburn.test";

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
    expects_.resize(0);
    failed_ = false;


    foo_source = std::fopen(kImgSrc, "w");
    for (int i = 0; i < 1000000; i++)
      std::fwrite(&i, sizeof(i), 1, foo_source);
    std::fclose(foo_source);

    AddTest(kImgSrc, "/usr/local/chromeos_image.bin.zip", false);
    AddTest(kImgSrc, "/dev/sda", false);
    AddTest(kImgSrc, "/dev/sda1", false);
    AddTest(kImgSrc, "/dev/sdb1", false);
    AddTest(kImgSrc, "/dev/sdb", true);

    tests_it_ = tests_.begin();
    expects_it_ = expects_.begin();
    RunNext();
  }

  void RunNext() {
    if (tests_it_ == tests_.end()) {
      std::cout << (failed_ ? "********** TESTS FAILED *********" :
          "********** TESTS SUCCEDED **********") << std::endl;
      std::remove(kImgSrc);
      ::g_main_loop_quit(loop_);
    } else {
      std::cout << "Running next test" << std::endl;
      RunOne();
    }
  }

 private:
  void AddTest(const char* to, const char* from, bool expected_success) {
    tests_.push_back(std::make_pair(to, from));
    expects_.push_back(expected_success);
  }

 static void TestRunCallback(DBusGProxy* gproxy, DBusGProxyCall* call_id,
      void* user_data) {
    TestClient* self = static_cast<TestClient*>(user_data);
    glib::ScopedError error;
    if (!::dbus_g_proxy_end_call(gproxy, call_id, &Resetter(&error).lvalue(),
        G_TYPE_INVALID)) {
      std::cout << "Burning failed: " << error->message << std::endl;
      if (*self->expects_it_) {
        self->failed_ = true;
        std::cout << "FAIL" << std::endl;
      } else {
        std::cout << "OK" << std::endl;
      }
      ++self->expects_it_;
      ++self->tests_it_;
      self->RunNext();
      return;
    }
    std::cout << "Burning started" << std::endl;
    if (!(*self->expects_it_)) {
      self->failed_ = true;
      std::cout << "FAIL" << std::endl;
    }
    ++self->tests_it_;
    ++self->expects_it_;
  }

  void RunOne() {
    glib::ScopedError error;
    std::cout << "start" << std::endl;
    if (!::dbus_g_proxy_begin_call(burn_proxy_.gproxy(), "BurnImage",
        &TestRunCallback,
        this, NULL,
        G_TYPE_STRING, tests_it_->first.c_str(),
        G_TYPE_STRING, tests_it_->second.c_str(),
        G_TYPE_INVALID)) {
      std::cout << "Burn call failed." << std::endl;
      std::cout << "FAIL" << std::endl;
      failed_ = true;
      ++expects_it_;
      ++tests_it_;
      RunNext();
    }
  }

  static void Finished(void* object, const char* target_path,
                       const bool success, const char* error ) {
    TestClient* self = static_cast<TestClient*>(object);
    if (success) {
      std::cout << "OK" << std::endl;
    } else {
      self->failed_ = true;
      std::cout << "FAIL" << std::endl;
    }
    self->RunNext();
  }

  dbus::Proxy burn_proxy_;
  GMainLoop *loop_;
  ConnectionUpdateType updateconnection_;
  ConnectionFinishedType finishedconnection_;
  typedef std::vector<std::pair<std::string, std::string> > IBTestVec;
  IBTestVec tests_;
  IBTestVec::iterator tests_it_;
  typedef std::vector<bool> ExpectedSuccessVec;
  ExpectedSuccessVec expects_;
  ExpectedSuccessVec::iterator expects_it_;
  bool failed_;
  FILE* foo_source;

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

