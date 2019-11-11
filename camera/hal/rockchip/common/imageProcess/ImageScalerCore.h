/*
 * Copyright (C) 2012-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _IMAGESCALER_CORE_H_
#define _IMAGESCALER_CORE_H_
#include <memory>
#include <vector>
#include "CameraBuffer.h"

NAMESPACE_DECLARATION {
/**
 * \class ImageScalerCore
 *
 */
class ImageScalerCore {
public:
    static status_t scaleFrame(std::shared_ptr<CameraBuffer> input,
                               std::shared_ptr<CameraBuffer> output);

    static status_t cropRotateScaleFrame(std::shared_ptr<CameraBuffer> input,
                                         std::shared_ptr<CameraBuffer> output,
                                         int angle,
                                         std::vector<uint8_t>& tempRotationBuffer,
                                         std::vector<uint8_t>& tempScaleBuffer);
};

} NAMESPACE_DECLARATION_END
#endif // _IMAGESCALER_CORE_H_
