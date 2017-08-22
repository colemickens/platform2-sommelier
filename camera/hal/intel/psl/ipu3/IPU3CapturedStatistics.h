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
 * It can store one or more types of statistics (AF,AWB,AE).
 *
 * Firstly, the statistics will be gotten
 * from the node "3a stat" of IMGU in StatisticsWorker.
 * Then the statistics will be sent to the ControlUnit.
 * In the ControlUnit before running AE, AF and AWB,
 * the statistics is set to the algo via ia_aiq_statistics_set().
 */
struct IPU3CapturedStatistics {
    int id;     /* request Id */

    /* the buffers are from mAfFilterBuffPool and mRgbsGridBuffPool in StatisticsWorker */
    std::shared_ptr<ia_aiq_af_grid> pooledAfGrid;
    std::shared_ptr<ia_aiq_rgbs_grid> pooledRGBSGrid;

    ia_aiq_statistics_input_params aiqStatsInputParams;

#define MAX_NUM_RGBS_GRIDS 1
#define MAX_NUM_AF_GRIDS 1
    const ia_aiq_rgbs_grid* rgbsGridArray[MAX_NUM_RGBS_GRIDS];
    const ia_aiq_af_grid* afGridArray[MAX_NUM_AF_GRIDS];
    ia_aiq_af_results af_results;
    uint32_t frameSequence;
};

} //namespace android
} //namespace camera2

#endif /* CAMERA3_HAL_IPU3CAPTUREDSTATISTICS_H_ */
