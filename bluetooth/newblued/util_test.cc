// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/util.h"

#include <stdint.h>

#include <newblue/gatt.h>
#include <newblue/uuid.h>

#include <vector>

#include <gtest/gtest.h>

#include "bluetooth/common/uuid.h"

namespace bluetooth {

TEST(UtilTest, ConvertToGattService) {
  // Service handles and UUID.
  uint16_t sfh = 0x0001;
  uint16_t slh = 0x0003;
  uint16_t suv = 0x1800;
  Uuid service_uuid(std::vector<uint8_t>({0x18, 0x00}));

  // Included service handles and UUID.
  uint16_t isfh = 0x0009;
  uint16_t islh = 0x000C;
  uint16_t isih = 0x000A;
  uint16_t isuv = 0x1801;
  Uuid included_service_uuid(std::vector<uint8_t>({0x18, 0x01}));

  // Characteristic handles, property and UUID.
  uint16_t cfh = 0x0004;
  uint16_t clh = 0x0006;
  uint16_t cvh = 0x0005;
  uint16_t cuv = 0x1802;
  uint8_t cp = 0xAB;
  Uuid characteristic_uuid(std::vector<uint8_t>({0x18, 0x02}));

  // Descriptor handles and UUIDs.
  uint16_t dh = 0x0007;
  uint16_t dh2 = 0x0008;
  uint16_t duv = 0x1803;
  uint16_t duv2 = 0x1804;
  Uuid descriptor_uuid(std::vector<uint8_t>({0x18, 0x03}));
  Uuid descriptor_uuid2(std::vector<uint8_t>({0x18, 0x04}));

  // Construct struct GattTraversedService.
  struct GattTraversedServiceCharDescr descriptors[2];
  descriptors[0].handle = dh;
  uuidFromUuid16(&descriptors[0].uuid, duv);
  descriptors[1].handle = dh2;
  uuidFromUuid16(&descriptors[1].uuid, duv2);

  struct GattTraversedServiceChar characteristic;
  uuidFromUuid16(&characteristic.uuid, cuv);
  characteristic.charProps = cp;
  characteristic.valHandle = cvh;
  characteristic.firstHandle = cfh;
  characteristic.lastHandle = clh;
  characteristic.numDescrs = 2;
  characteristic.descrs = descriptors;

  struct GattTraversedServiceInclSvc included_service;
  uuidFromUuid16(&included_service.uuid, isuv);
  included_service.includeDefHandle = isih;
  included_service.firstHandle = isfh;
  included_service.lastHandle = islh;

  struct GattTraversedService service;
  uuidFromUuid16(&service.uuid, suv);
  service.firstHandle = sfh;
  service.lastHandle = slh;
  service.numChars = 1;
  service.chars = &characteristic;
  service.numInclSvcs = 1;
  service.inclSvcs = &included_service;

  // Perform the conversion.
  std::unique_ptr<GattService> s = ConvertToGattService(service);

  // Verify service content.
  EXPECT_NE(nullptr, s);
  EXPECT_FALSE(s->HasOwner());
  EXPECT_TRUE(s->device_address().empty());
  EXPECT_EQ(sfh, s->first_handle());
  EXPECT_EQ(slh, s->last_handle());
  EXPECT_TRUE(s->primary());
  EXPECT_EQ(service_uuid, s->uuid());

  // Verify included service content.
  EXPECT_EQ(1, s->included_services_.size());
  EXPECT_EQ(s.get(), s->included_services_[isfh]->service());
  EXPECT_EQ(isih, s->included_services_[isfh]->included_handle());
  EXPECT_EQ(isfh, s->included_services_[isfh]->first_handle());
  EXPECT_EQ(islh, s->included_services_[isfh]->last_handle());
  EXPECT_EQ(included_service_uuid, s->included_services_[isfh]->uuid());

  // Verify characteristic content.
  EXPECT_EQ(1, s->characteristics_.size());
  EXPECT_EQ(s.get(), s->characteristics_[cfh]->service());
  EXPECT_EQ(cvh, s->characteristics_[cfh]->value_handle());
  EXPECT_EQ(cfh, s->characteristics_[cfh]->first_handle());
  EXPECT_EQ(clh, s->characteristics_[cfh]->last_handle());
  EXPECT_EQ(cp, s->characteristics_[cfh]->properties());
  EXPECT_EQ(characteristic_uuid, s->characteristics_[cfh]->uuid());
  EXPECT_TRUE(s->characteristics_[cfh]->value().empty());
  EXPECT_EQ(GattCharacteristic::NotifySetting::NONE,
            s->characteristics_[cfh]->notify_setting());

  // Verify descriptors content.
  EXPECT_EQ(2, s->characteristics_[cfh]->descriptors_.size());
  EXPECT_EQ(s->characteristics_[cfh].get(),
            s->characteristics_[cfh]->descriptors_[dh]->characteristic());
  EXPECT_EQ(dh, s->characteristics_[cfh]->descriptors_[dh]->handle());
  EXPECT_EQ(descriptor_uuid,
            s->characteristics_[cfh]->descriptors_[dh]->uuid());
  EXPECT_EQ(s->characteristics_[cfh].get(),
            s->characteristics_[cfh]->descriptors_[dh2]->characteristic());
  EXPECT_EQ(dh2, s->characteristics_[cfh]->descriptors_[dh2]->handle());
  EXPECT_EQ(descriptor_uuid2,
            s->characteristics_[cfh]->descriptors_[dh2]->uuid());
}

}  // namespace bluetooth
