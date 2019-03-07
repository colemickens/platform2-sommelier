// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONFIG_EC_CONFIG_H_
#define CHROMEOS_CONFIG_EC_CONFIG_H_

#include <stdint.h>
#include <stdlib.h>


enum stylus_category_type {
  STYLUS_CATEGORY_NONE = 0,
  STYLUS_CATEGORY_INTERNAL = 1,
  STYLUS_CATEGORY_EXTERNAL = 2
};

struct sku_info {
  const uint8_t sku;
  const uint8_t has_base_accelerometer :1;
  const uint8_t has_lid_accelerometer :1;
  const uint8_t is_lid_convertible :1;
  const uint8_t stylus_category :2;
};

extern const size_t NUM_SKUS;
extern const struct sku_info ALL_SKUS[];

#endif  // CHROMEOS_CONFIG_EC_CONFIG_H_
