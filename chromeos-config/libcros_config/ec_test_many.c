// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ec_config.h"

#include "compile_time_macros.h"


#if defined(BOARD_SOME)

const struct sku_info ALL_SKUS[] = {
  {.sku = 9,
   .has_base_accelerometer = 1,
   .has_lid_accelerometer = 1,
   .is_lid_convertible = 0,
   .stylus_category = 2},
  {.sku = 99,
   .has_base_accelerometer = 0,
   .has_lid_accelerometer = 1,
   .is_lid_convertible = 1,
   .stylus_category = 0}
};

#elif defined(BOARD_ANOTHER)

const struct sku_info ALL_SKUS[] = {
  {.sku = 40,
   .has_base_accelerometer = 1,
   .has_lid_accelerometer = 1,
   .is_lid_convertible = 0,
   .stylus_category = 0}
};

#endif


const size_t NUM_SKUS = ARRAY_SIZE(ALL_SKUS);
