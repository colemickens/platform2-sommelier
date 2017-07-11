/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#ifndef CAMERA3_HAL_IPU3CAPTUREDSTATISTICS_H_
#define CAMERA3_HAL_IPU3CAPTUREDSTATISTICS_H_

#include <memory>
#include "Intel3aPlus.h"

namespace android {
namespace camera2 {

/**
 * \struct IPU3CapturedStatistics
 *
 * This is the structure used to communicate new 3A statistics between capture
 * unit and control unit.
 * It can store one or more types of statistics (AF,AWB,AE).
 *
 * This structure adds the pointers to the structures with the actual
 * memory that are pooled in the capture unit.
 *
 * Normal flow of this structures is:
 * 1- Pools initialized at capture unit.
 * 2- Captured stats passed from capture unit to control unit.
 * 3 -Control unit will return the statistics once it has consumed them.
 */
struct IPU3CapturedStatistics: public RequestStatistics {
    /**
     * Cleanup before recycle
     *
     * This method is called by the SharedPoolItem when the item is recycled
     * At this stage we can cleanup before recycling the struct.
     * In this case we reset the TracingSP of individual stats buffers
     * this reference is holding. Other references may be still alive.
     *
     * \param[in] me: reference to self since this is a static method.
     */
    static void recyclerReset(IPU3CapturedStatistics *me)
    {
        if (CC_LIKELY(me != nullptr)) {
            me->pooledAfGrid.reset();
            me->pooledRGBSGrid.reset();
            me->pooledHistogram.reset();
        } else {
            LOGE("Trying to reset a null IPU3CapturedStatistics !!- BUG ");
        }
    }
    /**
     * pointers to the structure pooled in the capture unit, used for tracking
     * purposes and to detect what statistics are provided
     * Do not use directly. Use the ones from the base class.
     **/
    std::shared_ptr<ia_aiq_af_grid> pooledAfGrid;
    std::shared_ptr<ia_aiq_rgbs_grid> pooledRGBSGrid;
    std::shared_ptr<ia_aiq_histogram> pooledHistogram;
};

} //namespace android
} //namespace camera2

#endif /* CAMERA3_HAL_IPU3CAPTUREDSTATISTICS_H_ */
