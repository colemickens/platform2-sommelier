// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/adaptor_generator.h"

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

const char kMethod0Name[] = "Kaneda";
const char kMethod0Return[] = "s";
const char kMethod0Argument0[] = "s";
const char kMethod0ArgumentName0[] = "iwata";
const char kMethod0Argument1[] = "ao";
const char kMethod0ArgumentName1[] = "clarke";
const char kMethod1Name[] = "Tetsuo";
const char kMethod1Argument1[] = "i";
const char kMethod1Return[] = "x";
const char kMethod2Name[] = "Kei";
const char kMethod3Name[] = "Kiyoko";
const char kMethod3ReturnName0[] = "akira";
const char kMethod3Return0[] = "x";
const char kMethod3Return1[] = "s";
const char kSignal0Name[] = "Update";
const char kSignal1Name[] = "Mapping";
const char kSignal1Argument0[] = "s";
const char kSignal1ArgumentName0[] = "key";
const char kSignal1Argument1[] = "ao";
const char kProperty0Name[] = "CharacterName";
const char kProperty0Type[] = "s";
const char kProperty0Access[] = "read";
const char kProperty1Name[] = "WriteProperty";
const char kProperty1Type[] = "s";
const char kProperty1Access[] = "readwrite";

const char kInterfaceName[] = "org.chromium.Test";
const char kInterfaceName2[] = "org.chromium.Test2";
const char kMethod0Name2[] = "Kaneda2";
const char kMethod1Name2[] = "Tetsuo2";
const char kExpectedContent[] = R"literal_string(
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <base/macros.h>
#include <dbus/object_path.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/variant_dictionary.h>

namespace org {
namespace chromium {

// Interface definition for org::chromium::Test.
class TestInterface {
 public:
  virtual ~TestInterface() = default;

  virtual bool Kaneda(
      chromeos::ErrorPtr* error,
      const std::string& in_iwata,
      const std::vector<dbus::ObjectPath>& in_clarke,
      std::string* out_3) = 0;
  virtual bool Tetsuo(
      chromeos::ErrorPtr* error,
      int32_t in_1,
      int64_t* out_2) = 0;
  virtual bool Kei(
      chromeos::ErrorPtr* error) = 0;
  virtual bool Kiyoko(
      chromeos::ErrorPtr* error,
      int64_t* out_akira,
      std::string* out_2) = 0;
};

// Interface adaptor for org::chromium::Test.
class TestAdaptor {
 public:
  TestAdaptor(TestInterface* interface) : interface_(interface) {}

  void RegisterWithDBusObject(chromeos::dbus_utils::DBusObject* object) {
    chromeos::dbus_utils::DBusInterface* itf =
        object->AddOrGetInterface("org.chromium.Test");

    itf->AddSimpleMethodHandlerWithError(
        "Kaneda",
        base::Unretained(interface_),
        &TestInterface::Kaneda);
    itf->AddSimpleMethodHandlerWithError(
        "Tetsuo",
        base::Unretained(interface_),
        &TestInterface::Tetsuo);
    itf->AddSimpleMethodHandlerWithError(
        "Kei",
        base::Unretained(interface_),
        &TestInterface::Kei);
    itf->AddSimpleMethodHandlerWithError(
        "Kiyoko",
        base::Unretained(interface_),
        &TestInterface::Kiyoko);

    signal_Update_ = itf->RegisterSignalOfType<SignalUpdateType>("Update");
    signal_Mapping_ = itf->RegisterSignalOfType<SignalMappingType>("Mapping");

    itf->AddProperty("CharacterName", &character_name_);
    write_property_.SetAccessMode(
        chromeos::dbus_utils::ExportedPropertyBase::Access::kReadWrite);
    write_property_.SetValidator(
        base::Bind(&TestAdaptor::ValidateWriteProperty,
                   base::Unretained(this)));
    itf->AddProperty("WriteProperty", &write_property_);
  }

  void SendUpdateSignal() {
    auto signal = signal_Update_.lock();
    if (signal)
      signal->Send();
  }
  void SendMappingSignal(
      const std::string& in_key,
      const std::vector<dbus::ObjectPath>& in_2) {
    auto signal = signal_Mapping_.lock();
    if (signal)
      signal->Send(in_key, in_2);
  }

  std::string GetCharacterName() const {
    return character_name_.GetValue().Get<std::string>();
  }
  void SetCharacterName(const std::string& character_name) {
    character_name_.SetValue(character_name);
  }

  std::string GetWriteProperty() const {
    return write_property_.GetValue().Get<std::string>();
  }
  void SetWriteProperty(const std::string& write_property) {
    write_property_.SetValue(write_property);
  }
  virtual bool ValidateWriteProperty(
      chromeos::ErrorPtr* /*error*/, const std::string& /*value*/) {
    return true;
  }

  static dbus::ObjectPath GetObjectPath() {
    return dbus::ObjectPath{"/org/chromium/Test"};
  }

 private:
  using SignalUpdateType = chromeos::dbus_utils::DBusSignal<>;
  std::weak_ptr<SignalUpdateType> signal_Update_;

  using SignalMappingType = chromeos::dbus_utils::DBusSignal<
      std::string /*key*/,
      std::vector<dbus::ObjectPath>>;
  std::weak_ptr<SignalMappingType> signal_Mapping_;

  chromeos::dbus_utils::ExportedProperty<std::string> character_name_;
  chromeos::dbus_utils::ExportedProperty<std::string> write_property_;

  TestInterface* interface_;  // Owned by container of this adapter.

  DISALLOW_COPY_AND_ASSIGN(TestAdaptor);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface definition for org::chromium::Test2.
class Test2Interface {
 public:
  virtual ~Test2Interface() = default;

  virtual std::string Kaneda2(
      const std::string& in_iwata) const = 0;
  virtual void Tetsuo2(
      scoped_ptr<chromeos::dbus_utils::DBusMethodResponse<int64_t>> response,
      int32_t in_1) = 0;
};

// Interface adaptor for org::chromium::Test2.
class Test2Adaptor {
 public:
  Test2Adaptor(Test2Interface* interface) : interface_(interface) {}

  void RegisterWithDBusObject(chromeos::dbus_utils::DBusObject* object) {
    chromeos::dbus_utils::DBusInterface* itf =
        object->AddOrGetInterface("org.chromium.Test2");

    itf->AddSimpleMethodHandler(
        "Kaneda2",
        base::Unretained(interface_),
        &Test2Interface::Kaneda2);
    itf->AddMethodHandler(
        "Tetsuo2",
        base::Unretained(interface_),
        &Test2Interface::Tetsuo2);
  }

 private:
  Test2Interface* interface_;  // Owned by container of this adapter.

  DISALLOW_COPY_AND_ASSIGN(Test2Adaptor);
};

}  // namespace chromium
}  // namespace org
)literal_string";

}  // namespace
class AdaptorGeneratorTest : public Test {
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

TEST_F(AdaptorGeneratorTest, GenerateAdaptors) {
  Interface interface;
  interface.name = kInterfaceName;
  interface.path = "/org/chromium/Test";
  interface.methods.emplace_back(
      kMethod0Name,
      vector<Interface::Argument>{
          {kMethod0ArgumentName0, kMethod0Argument0},
          {kMethod0ArgumentName1, kMethod0Argument1}},
      vector<Interface::Argument>{{"", kMethod0Return}});
  interface.methods.emplace_back(
      kMethod1Name,
      vector<Interface::Argument>{{"", kMethod1Argument1}},
      vector<Interface::Argument>{{"", kMethod1Return}});
  interface.methods.emplace_back(kMethod2Name);
  // Interface methods with more than one return argument should be ignored.
  interface.methods.emplace_back(
      kMethod3Name,
      vector<Interface::Argument>{},
      vector<Interface::Argument>{
          {kMethod3ReturnName0, kMethod3Return0},
          {"", kMethod3Return1}});
  // Signals generate helper methods to send them.
  interface.signals.emplace_back(
      kSignal0Name,
      vector<Interface::Argument>{});
  interface.signals.emplace_back(
      kSignal1Name,
      vector<Interface::Argument>{
          {kSignal1ArgumentName0, kSignal1Argument0},
          {"", kSignal1Argument1}});
  interface.properties.emplace_back(
      kProperty0Name,
      kProperty0Type,
      kProperty0Access);
  interface.properties.emplace_back(
      kProperty1Name,
      kProperty1Type,
      kProperty1Access);

  Interface interface2;
  interface2.name = kInterfaceName2;
  interface2.methods.emplace_back(
      kMethod0Name2,
      vector<Interface::Argument>{
          {kMethod0ArgumentName0, kMethod0Argument0}},
      vector<Interface::Argument>{{"", kMethod0Return}});
  interface2.methods.back().is_const = true;
  interface2.methods.back().kind = Interface::Method::Kind::kSimple;
  interface2.methods.emplace_back(
      kMethod1Name2,
      vector<Interface::Argument>{{"", kMethod1Argument1}},
      vector<Interface::Argument>{{"", kMethod1Return}});
  interface2.methods.back().kind = Interface::Method::Kind::kAsync;

  base::FilePath output_path = temp_dir_.path().Append("output.h");
  EXPECT_TRUE(AdaptorGenerator::GenerateAdaptors({interface, interface2},
                                                 output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos, contents.find(kExpectedContent))
      << "Expected to find the following content...\n"
      << kExpectedContent << "...within content...\n" << contents;
}

}  // namespace chromeos_dbus_bindings
