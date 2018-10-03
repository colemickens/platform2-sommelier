// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ec_config.h"

#include "compile_time_macros.h"



const struct sku_info ALL_SKUS[] = {
  {.sku = 0,
   .has_base_accelerometer = 1,
   .has_base_gyroscope = 1,
   .has_lid_accelerometer = 1,
   .is_lid_convertible = 1},
  {.sku = 1,
   .has_base_accelerometer = 0,
   .has_base_gyroscope = 0,
   .has_lid_accelerometer = 0,
   .is_lid_convertible = 0},
  {.sku = 8,
   .has_base_accelerometer = 0,
   .has_base_gyroscope = 0,
   .has_lid_accelerometer = 0,
   .is_lid_convertible = 0},
  {.sku = 9,
   .has_base_accelerometer = 0,
   .has_base_gyroscope = 0,
   .has_lid_accelerometer = 0,
   .is_lid_convertible = 0}
};


const size_t NUM_SKUS = ARRAY_SIZE(ALL_SKUS);
