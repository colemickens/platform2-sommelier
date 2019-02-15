/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_CUSTOMER_CUSTOMER_SCENARIO_MGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_CUSTOMER_CUSTOMER_SCENARIO_MGR_H_
//
#include <mtkcam/def/common.h>
//
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
//
#include <mtkcam/3rdparty/common/scenario_type.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace scenariomgr {

/**
 * get camera capture scenario for features strategy
 */
auto customer_get_capture_scenario(
    int32_t* pScenario, /*eCameraScenarioMtk or eCameraScenarioCustomer*/
    const ScenarioHint& scenarioHint,
    IMetadata const* pAppMetadata /*for extension*/) -> bool;

/**
 * get camera streaming scenario for features strategy
 */
auto customer_get_streaming_scenario(
    int32_t* pScenario, /*eCameraScenarioMtk or eCameraScenarioCustomer*/
    const ScenarioHint& scenarioHint,
    IMetadata const* pAppMetadata /*for extension*/) -> bool;

/**
 * get features strategy table by camera scenario for camera features strategy
 */
auto customer_get_features_table_by_scenario(
    int32_t openId,
    int32_t const scenario, /*eCameraScenarioMtk or eCameraScenarioCustomer*/
    ScenarioFeatures* pScenarioFeatures) -> bool;

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace scenariomgr
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_CUSTOMER_CUSTOMER_SCENARIO_MGR_H_
