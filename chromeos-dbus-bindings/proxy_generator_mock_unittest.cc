// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/proxy_generator.h"

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
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

#include <base/callback_forward.h>
#include <base/macros.h>
#include <chromeos/any.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <gmock/gmock.h>

#include "proxies.h"

namespace org {
namespace chromium {

// Mock object for TestInterfaceProxyInterface.
class TestInterfaceProxyMock final : public TestInterfaceProxyInterface {
 public:
  TestInterfaceProxyMock() = default;

  MOCK_METHOD5(Elements,
               bool(const std::string& /*in_space_walk*/,
                    const std::vector<dbus::ObjectPath>& /*in_ramblin_man*/,
                    std::string*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(ElementsAsync,
               void(const std::string& /*in_space_walk*/,
                    const std::vector<dbus::ObjectPath>& /*in_ramblin_man*/,
                    const base::Callback<void(const std::string&)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ReturnToPatagonia,
               bool(int64_t*,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ReturnToPatagoniaAsync,
               void(const base::Callback<void(int64_t)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(NiceWeatherForDucks,
               bool(bool,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(NiceWeatherForDucksAsync,
               void(bool,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD2(ExperimentNumberSix,
               bool(chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(ExperimentNumberSixAsync,
               void(const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInterfaceProxyMock);
};
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Mock object for TestInterface2ProxyInterface.
class TestInterface2ProxyMock final : public TestInterface2ProxyInterface {
 public:
  TestInterface2ProxyMock() = default;

  MOCK_METHOD4(GetPersonInfo,
               bool(std::string* /*out_name*/,
                    int32_t* /*out_age*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(GetPersonInfoAsync,
               void(const base::Callback<void(const std::string& /*name*/, int32_t /*age*/)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInterface2ProxyMock);
};
}  // namespace chromium
}  // namespace org
)literal_string";

}  // namespace

class ProxyGeneratorMockTest : public Test {
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

TEST_F(ProxyGeneratorMockTest, GenerateMocks) {
  Interface interface;
  interface.name = kInterfaceName;
  interface.path = "/org/chromium/Test";
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
  interface.methods.back().doc_string = "Comment line1\nline2";
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
  base::FilePath proxy_path = temp_dir_.path().Append("proxies.h");
  ServiceConfig config;
  EXPECT_TRUE(ProxyGenerator::GenerateMocks(config, interfaces, output_path,
                                            proxy_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos, contents.find(kExpectedContent))
      << "Expected to find the following content...\n"
      << kExpectedContent << "...within content...\n" << contents;
}

}  // namespace chromeos_dbus_bindings
