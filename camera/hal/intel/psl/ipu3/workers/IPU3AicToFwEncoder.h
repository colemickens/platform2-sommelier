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

#ifndef PSL_IPU3_WORKERS_IPU3AICTOFWENCODER_H_
#define PSL_IPU3_WORKERS_IPU3AICTOFWENCODER_H_

#include "utils/Errors.h"

struct aic_config;
struct ipu3_uapi_params;

namespace cros {
namespace intel {

namespace IPU3AicToFwEncoder {
    status_t encodeParameters(aic_config *config, ipu3_uapi_params *params);
}

} /* namespace intel */
} /* namespace cros */

#endif /* PSL_IPU3_WORKERS_IPU3AICTOFWENCODER_H_ */
