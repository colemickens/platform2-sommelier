// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/proxy_generator.h"

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "chromeos-dbus-bindings/interface.h"

using std::string;
using std::vector;
using testing::Test;

namespace chromeos_dbus_bindings {

namespace {

const char kInterfaceName[] = "org.chromium.TestInterface";
const char kInterfaceName2[] = "org.chromium.TestInterface2";
const char kMethod1Name[] = "Elements";
const char kMethod1Return[] = "s";
const char kMethod1Argument1[] = "s";
const char kMethod1ArgumentName1[] = "space_walk";
const char kMethod1Argument2[] = "ao";
const char kMethod1ArgumentName2[] = "ramblin_man";
const char kMethod2Name[] = "ReturnToPatagonia";
const char kMethod2Return[] = "x";
const char kMethod3Name[] = "NiceWeatherForDucks";
const char kMethod3Argument1[] = "b";
const char kMethod4Name[] = "ExperimentNumberSix";
const char kMethod5Name[] = "GetPersonInfo";
const char kMethod5Argument1[] = "s";
const char kMethod5ArgumentName1[] = "name";
const char kMethod5Argument2[] = "i";
const char kMethod5ArgumentName2[] = "age";
const char kSignal1Name[] = "Closer";
const char kSignal2Name[] = "TheCurseOfKaZar";
const char kSignal2Argument1[] = "as";
const char kSignal2Argument2[] = "y";
const char kExpectedContent[] = R"literal_string(
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace org {
namespace chromium {

class TestInterfaceProxy final {
 public:
  class SignalReceiver {
   public:
    virtual void OnCloserSignal() {}
    virtual void OnTheCurseOfKaZarSignal(
        const std::vector<std::string>&,
        uint8_t) {}
  };

  TestInterfaceProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name,
      const std::string& object_path,
      SignalReceiver* signal_receiver)
      : bus_(bus),
        service_name_(service_name),
        object_path_(object_path),
        dbus_object_proxy_(
            bus_->GetObjectProxy(service_name_, object_path_)) {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "Closer",
        base::Bind(
            &SignalReceiver::OnCloserSignal,
            base::Unretained(signal_receiver)),
        base::Bind(
            &TestInterfaceProxy::OnDBusSignalConnected,
            base::Unretained(this)));
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "TheCurseOfKaZar",
        base::Bind(
            &SignalReceiver::OnTheCurseOfKaZarSignal,
            base::Unretained(signal_receiver)),
        base::Bind(
            &TestInterfaceProxy::OnDBusSignalConnected,
            base::Unretained(this)));
  }

  ~TestInterfaceProxy() {
    dbus_object_proxy_->Detach();
    bus_->RemoveObjectProxy(service_name_, object_path_, base::Closure());
  }

  void OnDBusSignalConnected(
      const std::string& interface,
      const std::string& signal,
      bool success) {
    if (!success) {
      LOG(ERROR)
          << "Failed to connect to " << interface << "." << signal
          << " for " << service_name_ << " at "
          << object_path_.value();
    }
  }

  bool Elements(
      const std::string& in_space_walk,
      const std::vector<dbus::ObjectPath>& in_ramblin_man,
      std::string* out_3,
      chromeos::ErrorPtr* error) {
    auto response = chromeos::dbus_utils::CallMethodAndBlock(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "Elements",
        error,
        in_space_walk,
        in_ramblin_man);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_3);
  }

  bool ReturnToPatagonia(
      int64_t* out_1,
      chromeos::ErrorPtr* error) {
    auto response = chromeos::dbus_utils::CallMethodAndBlock(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "ReturnToPatagonia",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  bool NiceWeatherForDucks(
      bool in_1,
      chromeos::ErrorPtr* error) {
    auto response = chromeos::dbus_utils::CallMethodAndBlock(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "NiceWeatherForDucks",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  // Comment line1
  // line2
  bool ExperimentNumberSix(
      chromeos::ErrorPtr* error) {
    auto response = chromeos::dbus_utils::CallMethodAndBlock(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "ExperimentNumberSix",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  bool GetPersonInfo(
      std::string* out_name,
      int32_t* out_age,
      chromeos::ErrorPtr* error) {
    auto response = chromeos::dbus_utils::CallMethodAndBlock(
        dbus_object_proxy_,
        "org.chromium.TestInterface2",
        "GetPersonInfo",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_name, out_age);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaceProxy);
};

}  // namespace chromium
}  // namespace org
)literal_string";

}  // namespace

class ProxyGeneratorTest : public Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  base::FilePath CreateInputFile(const string& contents) {
    base::FilePath path;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &path));
    EXPECT_EQ(contents.size(),
              base::WriteFile(path, contents.c_str(), contents.size()));
    return path;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(ProxyGeneratorTest, GenerateAdaptors) {
  Interface interface;
  interface.name = kInterfaceName;
  interface.methods.emplace_back(
      kMethod1Name,
      vector<Interface::Argument>{
          {kMethod1ArgumentName1, kMethod1Argument1},
          {kMethod1ArgumentName2, kMethod1Argument2}},
      vector<Interface::Argument>{{"", kMethod1Return}});
  interface.methods.emplace_back(
      kMethod2Name,
      vector<Interface::Argument>{},
      vector<Interface::Argument>{{"", kMethod2Return}});
  interface.methods.emplace_back(
      kMethod3Name,
      vector<Interface::Argument>{{"", kMethod3Argument1}},
      vector<Interface::Argument>{});
  interface.methods.emplace_back(kMethod4Name);
  interface.signals.emplace_back(kSignal1Name);
  interface.signals.emplace_back(
      kSignal2Name,
      vector<Interface::Argument>{
          {"", kSignal2Argument1},
          {"", kSignal2Argument2}});
  interface.methods.back().doc_string_ = "Comment line1\nline2";
  Interface interface2;
  interface2.name = kInterfaceName2;
  interface2.methods.emplace_back(
      kMethod5Name,
      vector<Interface::Argument>{},
      vector<Interface::Argument>{
          {kMethod5ArgumentName1, kMethod5Argument1},
          {kMethod5ArgumentName2, kMethod5Argument2}});
  vector<Interface> interfaces{interface, interface2};
  base::FilePath output_path = temp_dir_.path().Append("output.h");
  EXPECT_TRUE(ProxyGenerator::GenerateProxy(interfaces, output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos, contents.find(kExpectedContent))
      << "Expected to find the following content...\n"
      << kExpectedContent << "...within content...\n" << contents;
}

}  // namespace chromeos_dbus_bindings
