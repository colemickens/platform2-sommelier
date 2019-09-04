// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <brillo/dbus/exported_property_set.h>
#include <dbus/property.h>
#include <gtest/gtest.h>

#include "bluetooth/common/property.h"

using ::testing::_;

namespace bluetooth {

namespace {

constexpr char kTestInterfaceName[] = "org.example.Interface";
constexpr char kTestObjectPath[] = "/org/example/Object";
constexpr char kTestPropertyName1[] = "SomeProperty1";
constexpr char kTestPropertyName2[] = "SomeProperty2";
constexpr char kTestServiceName[] = "org.example.Default";

}  // namespace

class PropertySetTest : public ::testing::Test {
 public:
  void SetUp() { bus_ = new dbus::MockBus(dbus::Bus::Options()); }

  void OnExportedPropertyUpdated(
      const brillo::dbus_utils::ExportedPropertyBase* exported_property_base) {
    exported_property_update_count_++;
  }

 protected:
  scoped_refptr<dbus::MockBus> bus_;

  // Counts the number of times an exported property is updated.
  int exported_property_update_count_ = 0;
};

TEST_F(PropertySetTest, PropertyFactory) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<int> property_factory;
  std::unique_ptr<dbus::PropertyBase> property_base =
      property_factory.CreateProperty();
  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();
  dbus::Property<int>* property =
      static_cast<dbus::Property<int>*>(property_base.get());
  brillo::dbus_utils::ExportedProperty<int>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<int>*>(
          exported_property_base.get());
  dbus::PropertySet property_set(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base->Init(&property_set, kTestPropertyName1);
  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  int expected_value = 3;
  EXPECT_CALL(*object_proxy, MockCallMethodAndBlock(_, _)).Times(1);
  property->SetAndBlock(expected_value);
  property->ReplaceValueWithSetValue();
  property->set_valid(true);
  ASSERT_EQ(expected_value, property->value());

  EXPECT_EQ(0, exported_property_update_count_);
  property_factory.MergePropertiesToExportedProperty(
      {property_base.get()}, exported_property_base.get());
  // exported_property should be updated to be the same as property.
  EXPECT_EQ(expected_value, exported_property->value());
  EXPECT_EQ(1, exported_property_update_count_);

  property_factory.MergePropertiesToExportedProperty(
      {property_base.get()}, exported_property_base.get());
  // Now that the values are already the same, exported_property shouldn't get
  // updated.
  EXPECT_EQ(1, exported_property_update_count_);

  // Now set property to another value.
  expected_value = 4;
  EXPECT_CALL(*object_proxy, MockCallMethodAndBlock(_, _)).Times(1);
  property->SetAndBlock(expected_value);
  property->ReplaceValueWithSetValue();
  property->set_valid(true);
  ASSERT_EQ(expected_value, property->value());
  property_factory.MergePropertiesToExportedProperty(
      {property_base.get()}, exported_property_base.get());
  // exported_property should be updated to be the same as property.
  EXPECT_EQ(expected_value, exported_property->value());
  EXPECT_EQ(2, exported_property_update_count_);
}

TEST_F(PropertySetTest, PropertySet) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertySet property_set(object_proxy.get(), kTestInterfaceName,
                           base::Bind([](const std::string& property_name) {}));

  auto int_property = std::make_unique<dbus::Property<int>>();
  auto bool_property = std::make_unique<dbus::Property<int>>();
  dbus::PropertyBase* expected_int_property = int_property.get();
  dbus::PropertyBase* expected_bool_property = bool_property.get();
  // Before the properties are registered, name() should be empty strings.
  ASSERT_EQ("", expected_int_property->name());
  ASSERT_EQ("", expected_bool_property->name());

  property_set.RegisterProperty(kTestPropertyName1, std::move(int_property));
  property_set.RegisterProperty(kTestPropertyName2, std::move(bool_property));

  // GetProperty should remember the registered properties.
  EXPECT_EQ(expected_int_property,
            property_set.GetProperty(kTestPropertyName1));
  EXPECT_EQ(expected_bool_property,
            property_set.GetProperty(kTestPropertyName2));
  // The dbus::PropertyBase::name() should return the registered name.
  EXPECT_EQ(kTestPropertyName1, expected_int_property->name());
  EXPECT_EQ(kTestPropertyName2, expected_bool_property->name());
}

TEST_F(PropertySetTest, PropertyMergingDefault) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<int> property_factory(MergingRule::DEFAULT);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<int>* property1 =
      static_cast<dbus::Property<int>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<dbus::PropertyBase> property_base2 =
      property_factory.CreateProperty();
  dbus::Property<int>* property2 =
      static_cast<dbus::Property<int>*>(property_base2.get());
  dbus::PropertySet property_set2(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base2->Init(&property_set2, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();
  brillo::dbus_utils::ExportedProperty<int>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<int>*>(
          exported_property_base.get());

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  int value1 = 3;
  int value2 = 5;
  property1->SetAndBlock(value1);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  property2->SetAndBlock(value2);
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(value1, exported_property->value());
}

TEST_F(PropertySetTest, PropertyMergingAnd) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<bool> property_factory(MergingRule::AND);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<bool>* property1 =
      static_cast<dbus::Property<bool>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<dbus::PropertyBase> property_base2 =
      property_factory.CreateProperty();
  dbus::Property<bool>* property2 =
      static_cast<dbus::Property<bool>*>(property_base2.get());
  dbus::PropertySet property_set2(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base2->Init(&property_set2, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();
  brillo::dbus_utils::ExportedProperty<bool>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<bool>*>(
          exported_property_base.get());

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock(true);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  property2->SetAndBlock(false);
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(false, exported_property->value());

  property2->SetAndBlock(true);
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(true, exported_property->value());
}

TEST_F(PropertySetTest, PropertyMergingAndInvalidType) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<int> property_factory(MergingRule::AND);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<int>* property1 =
      static_cast<dbus::Property<int>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock(1);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  EXPECT_DEATH(property_factory.MergePropertiesToExportedProperty(
                   {property_base1.get()}, exported_property_base.get()),
               "AND merging not supported for the given value type");
}

TEST_F(PropertySetTest, PropertyMergingOr) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<bool> property_factory(MergingRule::OR);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<bool>* property1 =
      static_cast<dbus::Property<bool>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<dbus::PropertyBase> property_base2 =
      property_factory.CreateProperty();
  dbus::Property<bool>* property2 =
      static_cast<dbus::Property<bool>*>(property_base2.get());
  dbus::PropertySet property_set2(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base2->Init(&property_set2, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();
  brillo::dbus_utils::ExportedProperty<bool>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<bool>*>(
          exported_property_base.get());

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock(false);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  property2->SetAndBlock(true);
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(true, exported_property->value());

  property2->SetAndBlock(false);
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(false, exported_property->value());
}

TEST_F(PropertySetTest, PropertyMergingOrInvalidType) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<int> property_factory(MergingRule::OR);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<int>* property1 =
      static_cast<dbus::Property<int>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock(1);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  EXPECT_DEATH(property_factory.MergePropertiesToExportedProperty(
                   {property_base1.get()}, exported_property_base.get()),
               "OR merging not supported for the given value type");
}

TEST_F(PropertySetTest, PropertyMergingUnion) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<std::vector<std::string>> property_factory(
      MergingRule::UNION);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<std::vector<std::string>>* property1 =
      static_cast<dbus::Property<std::vector<std::string>>*>(
          property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<dbus::PropertyBase> property_base2 =
      property_factory.CreateProperty();
  dbus::Property<std::vector<std::string>>* property2 =
      static_cast<dbus::Property<std::vector<std::string>>*>(
          property_base2.get());
  dbus::PropertySet property_set2(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base2->Init(&property_set2, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();
  brillo::dbus_utils::ExportedProperty<std::vector<std::string>>*
      exported_property = static_cast<
          brillo::dbus_utils::ExportedProperty<std::vector<std::string>>*>(
          exported_property_base.get());

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock({"string1"});
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  property2->SetAndBlock({"string2"});
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  std::vector<std::string> expected_value = {"string1", "string2"};
  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(expected_value, exported_property->value());

  property2->SetAndBlock({"string1"});
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  expected_value = {"string1"};
  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ(expected_value, exported_property->value());
}

TEST_F(PropertySetTest, PropertyMergingUnionInvalidType) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<int> property_factory(MergingRule::UNION);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<int>* property1 =
      static_cast<dbus::Property<int>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock(1);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  EXPECT_DEATH(property_factory.MergePropertiesToExportedProperty(
                   {property_base1.get()}, exported_property_base.get()),
               "UNION merging not supported for the given value type");
}

TEST_F(PropertySetTest, PropertyMergingCancatenation) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<std::string> property_factory(MergingRule::CONCATENATION);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<std::string>* property1 =
      static_cast<dbus::Property<std::string>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<dbus::PropertyBase> property_base2 =
      property_factory.CreateProperty();
  dbus::Property<std::string>* property2 =
      static_cast<dbus::Property<std::string>*>(property_base2.get());
  dbus::PropertySet property_set2(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base2->Init(&property_set2, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();
  brillo::dbus_utils::ExportedProperty<std::string>* exported_property =
      static_cast<brillo::dbus_utils::ExportedProperty<std::string>*>(
          exported_property_base.get());

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock("string1");
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  property2->SetAndBlock("string2");
  property2->ReplaceValueWithSetValue();
  property2->set_valid(true);

  property_factory.MergePropertiesToExportedProperty(
      {property_base1.get(), property_base2.get()},
      exported_property_base.get());
  EXPECT_EQ("string1 string2", exported_property->value());
}

TEST_F(PropertySetTest, PropertyMergingConcatenationInvalidType) {
  scoped_refptr<dbus::MockObjectProxy> object_proxy = new dbus::MockObjectProxy(
      bus_.get(), kTestServiceName, dbus::ObjectPath(kTestObjectPath));

  PropertyFactory<int> property_factory(MergingRule::CONCATENATION);
  std::unique_ptr<dbus::PropertyBase> property_base1 =
      property_factory.CreateProperty();
  dbus::Property<int>* property1 =
      static_cast<dbus::Property<int>*>(property_base1.get());
  dbus::PropertySet property_set1(
      object_proxy.get(), kTestInterfaceName,
      base::Bind([](const std::string& property_name) {}));
  property_base1->Init(&property_set1, kTestPropertyName1);

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
      exported_property_base = property_factory.CreateExportedProperty();

  exported_property_base->SetUpdateCallback(base::Bind(
      &PropertySetTest::OnExportedPropertyUpdated, base::Unretained(this)));

  property1->SetAndBlock(1);
  property1->ReplaceValueWithSetValue();
  property1->set_valid(true);

  EXPECT_DEATH(property_factory.MergePropertiesToExportedProperty(
                   {property_base1.get()}, exported_property_base.get()),
               "CONCATENATION merging not supported for the given value type");
}

}  // namespace bluetooth
