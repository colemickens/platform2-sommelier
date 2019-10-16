/*
 * Copyright (C) 2017 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file IPU3Types.h
 *
 * Used for IPU3-wide common type definitions and const definitions
 */

#ifndef PSL_IPU3_IPU3TYPES_H
#define PSL_IPU3_IPU3TYPES_H

#include <ia_types.h>

namespace cros {
namespace intel {

// typedefs
typedef ia_binary_data MakernoteData;

// consts
const int JPEG_QUALITY_DEFAULT = 95;
const int THUMBNAIL_QUALITY_DEFAULT = 90;

} // namespace intel
} // namespace cros


#endif // PSL_IPU3_IPU3TYPES_H
