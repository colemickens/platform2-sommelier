// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/gatt_attributes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace bluetooth {

class GattAttributesTest : public ::testing::Test {
 public:
  GattAttributesTest()
      : address_("01:02:03:0A:0B:0C"),
        address2_("0D:0E:0F:10:11:12"),
        service_uuid_(std::vector<uint8_t>({0x12, 0x34})),
        service_uuid2_(std::vector<uint8_t>({0xAB, 0xCD})),
        characteristic_uuid_(std::vector<uint8_t>({0x56, 0x78})),
        characteristic_uuid2_(std::vector<uint8_t>({0xAB, 0xCD})),
        descriptor_uuid_(std::vector<uint8_t>({0x56, 0x78})),
        included_service_uuid_(std::vector<uint8_t>({0x9A, 0xBC})) {}

  // Device addresses.
  std::string address_;
  std::string address2_;
  Uuid service_uuid_;
  Uuid service_uuid2_;
  Uuid characteristic_uuid_;
  Uuid characteristic_uuid2_;
  Uuid descriptor_uuid_;
  Uuid included_service_uuid_;
};

TEST_F(GattAttributesTest, AttributesInit) {
  std::string empty_address("");

  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  // Included service handles.
  uint16_t isfh = 0x0001;
  uint16_t islh = 0x0003;
  uint16_t isih = 0x0002;

  // Characteristic handles and property.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;

  // Descriptor handle.
  uint16_t dh = 0x0006;

  // GattService fails to init.
  EXPECT_DEATH(GattService(empty_address, sfh, slh, true, service_uuid_), "");
  EXPECT_DEATH(GattService(address_, slh, sfh, true, service_uuid_), "");
  EXPECT_DEATH(GattService(slh, sfh, true, service_uuid_), "");

  // GattService inits successfully.
  GattService s(address_, sfh, slh, true, service_uuid_);
  EXPECT_EQ(address_, s.device_address().value());
  EXPECT_EQ(sfh, s.first_handle());
  EXPECT_EQ(slh, s.last_handle());
  EXPECT_TRUE(s.primary().value());
  EXPECT_EQ(service_uuid_, s.uuid().value());
  EXPECT_TRUE(s.HasOwner());
  GattService s2(sfh, slh, false, service_uuid_);
  EXPECT_EQ("", s2.device_address().value());
  EXPECT_EQ(sfh, s2.first_handle());
  EXPECT_EQ(slh, s2.last_handle());
  EXPECT_FALSE(s2.primary().value());
  EXPECT_EQ(service_uuid_, s2.uuid().value());
  EXPECT_FALSE(s2.HasOwner());

  // GattIncludedService fails to init.
  EXPECT_DEATH(
      GattIncludedService(nullptr, isih, isfh, islh, included_service_uuid_),
      "");
  EXPECT_DEATH(
      GattIncludedService(&s, isih, islh, isfh, included_service_uuid_), "");

  // GattIncludedService inits successfully.
  GattIncludedService ins(&s, isih, isfh, islh, included_service_uuid_);
  EXPECT_EQ(&s, ins.service());
  EXPECT_EQ(isih, ins.included_handle());
  EXPECT_EQ(isfh, ins.first_handle());
  EXPECT_EQ(islh, ins.last_handle());
  EXPECT_EQ(included_service_uuid_, ins.uuid());

  // Gatt Characteristic fails to init.
  EXPECT_DEATH(
      GattCharacteristic(nullptr, cvh, cfh, clh, cp, characteristic_uuid_), "");
  EXPECT_DEATH(GattCharacteristic(&s, cvh, clh, cfh, cp, characteristic_uuid_),
               "");

  // GattCharacteristic inits successfully.
  GattCharacteristic c(&s, cvh, cfh, clh, cp, characteristic_uuid_);
  EXPECT_EQ(&s, c.service().value());
  EXPECT_EQ(cvh, c.value_handle());
  EXPECT_EQ(cfh, c.first_handle());
  EXPECT_EQ(clh, c.last_handle());
  EXPECT_EQ(cp, c.properties().value());
  EXPECT_EQ(characteristic_uuid_, c.uuid().value());
  EXPECT_TRUE(c.value().value().empty());
  EXPECT_EQ(GattCharacteristic::NotifySetting::NONE,
            c.notify_setting().value());

  // GattDescriptor fails to init.
  EXPECT_DEATH(GattDescriptor(nullptr, dh, descriptor_uuid_), "");

  // GattDescriptor inits successfully.
  GattDescriptor d(&c, dh, descriptor_uuid_);
  EXPECT_EQ(&c, d.characteristic().value());
  EXPECT_EQ(dh, d.handle());
  EXPECT_EQ(descriptor_uuid_, d.uuid().value());
}

TEST_F(GattAttributesTest, GattServiceSetter) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  GattService s(sfh, slh, false, service_uuid_);
  EXPECT_EQ("", s.device_address().value());
  EXPECT_FALSE(s.HasOwner());
  EXPECT_DEATH(s.SetDeviceAddress(""), "");
  s.SetDeviceAddress(address_);
  EXPECT_EQ(address_, s.device_address().value());
}

TEST_F(GattAttributesTest, GattServiceAddIncludedServiceCharacteristic) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;
  uint16_t sfh2 = 0x0004;
  uint16_t slh2 = 0x0006;

  // Included service handles.
  uint16_t isfh = 0x0001;
  uint16_t islh = 0x0003;
  uint16_t isih = 0x0002;

  // Characteristic handles and property.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;

  GattService s(sfh, slh, false, service_uuid_);
  GattService s2(sfh2, slh2, false, service_uuid2_);

  auto ins = std::make_unique<GattIncludedService>(&s, isih, isfh, islh,
                                                   included_service_uuid_);
  EXPECT_DEATH(s2.AddIncludedService(std::move(ins)), "");
  s.AddIncludedService(std::move(ins));
  EXPECT_EQ(1, s.included_services_.size());
  EXPECT_EQ(&s, s.included_services_[isfh]->service());
  EXPECT_EQ(isfh, s.included_services_[isfh]->first_handle());
  EXPECT_EQ(islh, s.included_services_[isfh]->last_handle());
  EXPECT_EQ(isih, s.included_services_[isfh]->included_handle());
  EXPECT_EQ(included_service_uuid_, s.included_services_[isfh]->uuid());

  auto c = std::make_unique<GattCharacteristic>(&s, cvh, cfh, clh, cp,
                                                characteristic_uuid_);
  EXPECT_DEATH(s2.AddCharacteristic(std::move(c)), "");
  s.AddCharacteristic(std::move(c));
  EXPECT_EQ(1, s.characteristics_.size());
  EXPECT_EQ(&s, s.characteristics_[cfh]->service().value());
  EXPECT_EQ(cfh, s.characteristics_[cfh]->first_handle());
  EXPECT_EQ(clh, s.characteristics_[cfh]->last_handle());
  EXPECT_EQ(cvh, s.characteristics_[cfh]->value_handle());
  EXPECT_EQ(cp, s.characteristics_[cfh]->properties().value());
  EXPECT_EQ(characteristic_uuid_, s.characteristics_[cfh]->uuid().value());
  EXPECT_TRUE(s.characteristics_[cfh]->value().value().empty());
  EXPECT_EQ(GattCharacteristic::NotifySetting::NONE,
            s.characteristics_[cfh]->notify_setting().value());
}

TEST_F(GattAttributesTest, GattCharacteristicAddDescriptor) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  // Characteristic handles and properties.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;
  uint16_t cfh2 = 0x000A;
  uint16_t clh2 = 0x000F;
  uint16_t cvh2 = 0x000C;
  uint8_t cp2 = 0x12;

  // Descriptor handle.
  uint16_t dh = 0x0006;

  GattService s(sfh, slh, false, service_uuid_);
  GattCharacteristic c(&s, cvh, cfh, clh, cp, characteristic_uuid_);
  GattCharacteristic c2(&s, cvh2, cfh2, clh2, cp2, characteristic_uuid2_);

  auto d = std::make_unique<GattDescriptor>(&c, dh, descriptor_uuid_);
  EXPECT_DEATH(c2.AddDescriptor(std::move(d)), "");
  c.AddDescriptor(std::move(d));
  EXPECT_EQ(1, c.descriptors_.size());
  EXPECT_EQ(&c, c.descriptors_[dh]->characteristic().value());
  EXPECT_EQ(dh, c.descriptors_[dh]->handle());
  EXPECT_TRUE(c.descriptors_[dh]->value().value().empty());
}

TEST_F(GattAttributesTest, GattCharacteristicSetValue) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  // Characteristic handles and properties.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;

  GattService s(sfh, slh, false, service_uuid_);
  GattCharacteristic c(&s, cvh, cfh, clh, cp, characteristic_uuid_);

  EXPECT_TRUE(c.value().value().empty());

  c.SetValue({0x11, 0x22});
  EXPECT_EQ(std::vector<uint8_t>({0x11, 0x22}), c.value().value());
}

TEST_F(GattAttributesTest, GattDescriptorSetValue) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  // Characteristic handles and properties.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;

  // Descriptor handle.
  uint16_t dh = 0x0006;

  std::vector<uint8_t> value({0x33, 0x44, 0x55});

  GattService s(sfh, slh, false, service_uuid_);
  GattCharacteristic c(&s, cvh, cfh, clh, cp, characteristic_uuid_);
  GattDescriptor d(&c, dh, descriptor_uuid_);

  EXPECT_TRUE(d.value().value().empty());

  d.SetValue(value);
  EXPECT_EQ(value, d.value().value());
}

TEST_F(GattAttributesTest, GattCharacteristicResetPropertiesUpdated) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  // Characteristic handles and properties.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;

  std::vector<uint8_t> value({0x11, 0x22});

  GattService s(sfh, slh, false, service_uuid_);
  GattCharacteristic c(&s, cvh, cfh, clh, cp, characteristic_uuid_);

  EXPECT_FALSE(c.value().updated());
  EXPECT_TRUE(c.value().value().empty());

  c.SetValue(value);
  EXPECT_TRUE(c.value().updated());
  EXPECT_EQ(value, c.value().value());

  c.ResetPropertiesUpdated();
  EXPECT_FALSE(c.value().updated());
  EXPECT_EQ(value, c.value().value());
}

TEST_F(GattAttributesTest, GattDescriptorResetPropertiesUpdated) {
  // Service handles.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;

  // Characteristic handles and properties.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint8_t cp = 0xAB;

  // Descriptor handle.
  uint16_t dh = 0x0006;

  std::vector<uint8_t> value({0x33, 0x44, 0x55});

  GattService s(sfh, slh, false, service_uuid_);
  GattCharacteristic c(&s, cvh, cfh, clh, cp, characteristic_uuid_);
  GattDescriptor d(&c, dh, descriptor_uuid_);

  EXPECT_FALSE(d.value().updated());
  EXPECT_TRUE(d.value().value().empty());

  d.SetValue(value);
  EXPECT_TRUE(d.value().updated());
  EXPECT_EQ(value, d.value().value());

  d.ResetPropertiesUpdated();
  EXPECT_FALSE(d.value().updated());
  EXPECT_EQ(value, d.value().value());
}

}  // namespace bluetooth
