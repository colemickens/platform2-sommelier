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

#define LOG_TAG "mtkcam-PipelineSettingPolicyFactory"

#include <memory>
#include <mtkcam/pipeline/policy/ICaptureStreamUpdaterPolicy.h>
#include <mtkcam/pipeline/policy/IConfigAppImageStreamInfoMaxBufNumPolicy.h>
#include <mtkcam/pipeline/policy/IConfigPipelineNodesNeedPolicy.h>
#include <mtkcam/pipeline/policy/IConfigStreamInfoPolicy.h>
#include <mtkcam/pipeline/policy/IFaceDetectionIntentPolicy.h>
#include <mtkcam/pipeline/policy/IIOMapPolicy.h>
#include <mtkcam/pipeline/policy/IPipelineSettingPolicy.h>
//
#include <mtkcam/pipeline/policy/IP1DmaNeedPolicy.h>
#include <mtkcam/pipeline/policy/IP1HwSettingPolicy.h>
#include <mtkcam/pipeline/policy/IP2NodeDecisionPolicy.h>
#include <mtkcam/pipeline/policy/IRequestMetadataPolicy.h>
#include <mtkcam/pipeline/policy/ISensorSettingPolicy.h>
//
#include <mtkcam/pipeline/policy/ITopologyPolicy.h>
//
#include "PipelineSettingPolicyImpl.h"
//
#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
// using namespace NSCam::v3::pipeline::policy::pipelinesetting;

/******************************************************************************
 *
 ******************************************************************************/
#define MAKE_PIPELINE_POLICY(_class_, ...)                                     \
  std::make_shared<_class_>(                                                   \
      NSCam::v3::pipeline::policy::pipelinesetting::                           \
          PipelineSettingPolicyImpl::CreationParams{                           \
              .pPipelineStaticInfo = params.pPipelineStaticInfo,               \
              .pPipelineUserConfiguration = params.pPipelineUserConfiguration, \
              .pPolicyTable = pPolicyTable,                                    \
              .pMediatorTable = pMediatorTable,                                \
          })

#define _POLICY_(_f_, _inst_) pPolicyTable->_f_ = _inst_

#define _POLICY_IF_EMPTY_(_f_, _inst_) \
  if (pPolicyTable->_f_ == nullptr) {  \
    _POLICY_(_f_, _inst_);             \
  }

#define _MEDIATOR_(_f_, _maker_)                                            \
  pMediatorTable->_f_ = (_maker_(                                           \
      NSCam::v3::pipeline::policy::pipelinesetting::MediatorCreationParams{ \
          .pPipelineStaticInfo = params.pPipelineStaticInfo,                \
          .pPipelineUserConfiguration = params.pPipelineUserConfiguration,  \
          .pPolicyTable = pPolicyTable,                                     \
      }))

#define _MEDIATOR_IF_EMPTY_(_f_, _maker_) \
  if (pMediatorTable->_f_ == nullptr) {   \
    _MEDIATOR_(_f_, _maker_);             \
  }

#define _FEATUREPOLICY_(_module_, _creator_)                                  \
  pPolicyTable->_module_ =                                                    \
      (_creator_(NSCam::v3::pipeline::policy::featuresetting::CreationParams{ \
          .pPipelineStaticInfo = params.pPipelineStaticInfo,                  \
          .pPipelineUserConfiguration = params.pPipelineUserConfiguration,    \
      }))

#define _FEATUREPOLICY_IF_EMPTY_(_module_, _creator_) \
  if (pPolicyTable->_module_ == nullptr) {            \
    _FEATUREPOLICY_(_module_, _creator_);             \
  }

#define _METADATAPOLICY_(_module_, _creator_)                                  \
  pPolicyTable->_module_ =                                                     \
      (_creator_(NSCam::v3::pipeline::policy::requestmetadata::CreationParams{ \
          .pPipelineStaticInfo = params.pPipelineStaticInfo,                   \
          .pPipelineUserConfiguration = params.pPipelineUserConfiguration,     \
          .pRequestMetadataPolicy = pPolicyTable->_module_,                    \
      }))

#define _METADATAPOLICY_IF_EMPTY_(_module_, _creator_) \
  if (pPolicyTable->_module_ == nullptr) {             \
    _METADATAPOLICY_(_module_, _creator_);             \
  }
/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<
    NSCam::v3::pipeline::policy::pipelinesetting::IConfigSettingPolicyMediator>
makeConfigSettingPolicyMediator_Default(
    NSCam::v3::pipeline::policy::pipelinesetting::
        MediatorCreationParams const&);
std::shared_ptr<
    NSCam::v3::pipeline::policy::pipelinesetting::IRequestSettingPolicyMediator>
makeRequestSettingPolicyMediator_Default(
    NSCam::v3::pipeline::policy::pipelinesetting::
        MediatorCreationParams const&);
std::shared_ptr<
    NSCam::v3::pipeline::policy::pipelinesetting::IRequestSettingPolicyMediator>
makeRequestSettingPolicyMediator_PDE(
    NSCam::v3::pipeline::policy::pipelinesetting::
        MediatorCreationParams const&);

/******************************************************************************
 *
 ******************************************************************************/
static auto decidePolicyAndMake(
    NSCam::v3::pipeline::policy::pipelinesetting::
        IPipelineSettingPolicyFactory::CreationParams const& params,
    std::shared_ptr<NSCam::v3::pipeline::policy::pipelinesetting::PolicyTable>
        pPolicyTable,
    std::shared_ptr<NSCam::v3::pipeline::policy::pipelinesetting::MediatorTable>
        pMediatorTable)
    -> std::shared_ptr<
        NSCam::v3::pipeline::policy::pipelinesetting::IPipelineSettingPolicy> {
  //  Default (use the default policy if it's empty.)
  {
    // policy (configuration)
    _POLICY_IF_EMPTY_(fConfigPipelineNodesNeed,
                      NSCam::v3::pipeline::policy::
                          makePolicy_Configuration_PipelineNodesNeed_Default());
    _POLICY_IF_EMPTY_(
        fSensorSetting,
        NSCam::v3::pipeline::policy::makePolicy_SensorSetting_Default());
    _POLICY_IF_EMPTY_(fConfigP1HwSetting,
                      NSCam::v3::pipeline::policy::
                          makePolicy_Configuration_P1HwSetting_Default());
    _POLICY_IF_EMPTY_(fConfigP1DmaNeed,
                      NSCam::v3::pipeline::policy::
                          makePolicy_Configuration_P1DmaNeed_Default());
    _POLICY_IF_EMPTY_(fConfigStreamInfo_P1,
                      NSCam::v3::pipeline::policy::
                          makePolicy_Configuration_StreamInfo_P1_Default());
    _POLICY_IF_EMPTY_(fConfigStreamInfo_NonP1,
                      NSCam::v3::pipeline::policy::
                          makePolicy_Configuration_StreamInfo_NonP1_Default());
    _POLICY_IF_EMPTY_(
        fConfigStreamInfo_AppImageStreamInfoMaxBufNum,
        NSCam::v3::pipeline::policy::
            makePolicy_Configuration_AppImageStreamInfoMaxBufNum_Default());

    // policy (request)
    _POLICY_IF_EMPTY_(
        fFaceDetectionIntent,
        NSCam::v3::pipeline::policy::makePolicy_FDIntent_Default());
    _POLICY_IF_EMPTY_(
        fP2NodeDecision,
        NSCam::v3::pipeline::policy::makePolicy_P2NodeDecision_Default());
    _POLICY_IF_EMPTY_(
        fTopology, NSCam::v3::pipeline::policy::makePolicy_Topology_Default());
    _POLICY_IF_EMPTY_(
        fCaptureStreamUpdater,
        NSCam::v3::pipeline::policy::makePolicy_CaptureStreamUpdater_Default());
    _POLICY_IF_EMPTY_(
        fIOMap_P2Node,
        NSCam::v3::pipeline::policy::makePolicy_IOMap_P2Node_Default());
    _POLICY_IF_EMPTY_(
        fIOMap_NonP2Node,
        NSCam::v3::pipeline::policy::makePolicy_IOMap_NonP2Node_Default());

    // RequestMetadata (request)
    _METADATAPOLICY_IF_EMPTY_(pRequestMetadataPolicy,
                              NSCam::v3::pipeline::policy::requestmetadata::
                                  makePolicy_RequestMetadata_Default);

    // feature
    _FEATUREPOLICY_IF_EMPTY_(mFeaturePolicy,
                             NSCam::v3::pipeline::policy::featuresetting::
                                 createFeatureSettingPolicyInstance);

    // mediator
    _MEDIATOR_IF_EMPTY_(pConfigSettingPolicyMediator,
                        makeConfigSettingPolicyMediator_Default);
    _MEDIATOR_IF_EMPTY_(pRequestSettingPolicyMediator,
                        makeRequestSettingPolicyMediator_Default);

    ////////////////////////////////////////////////////////////////////////////
    //  for Debug Dump, use decorator pattern
    ////////////////////////////////////////////////////////////////////////////
    {
      int debugProcRaw =
          property_get_int32("vendor.debug.camera.cfg.ProcRaw", -1);
      if (debugProcRaw > 0) {
        MY_LOGD("vendor.debug.camera.cfg.ProcRaw=%d", debugProcRaw);
        _METADATAPOLICY_(pRequestMetadataPolicy,
                         NSCam::v3::pipeline::policy::requestmetadata::
                             makePolicy_RequestMetadata_DebugDump);
      }
    }

    return MAKE_PIPELINE_POLICY(NSCam::v3::pipeline::policy::pipelinesetting::
                                    PipelineSettingPolicyImpl);
  }

  MY_LOGE("never be here");
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::pipeline::policy::pipelinesetting::
    IPipelineSettingPolicyFactory::createPipelineSettingPolicy(
        CreationParams const& params)
        -> std::shared_ptr<IPipelineSettingPolicy> {
  auto pPolicyTable = std::make_shared<PolicyTable>();
  if (CC_UNLIKELY(pPolicyTable == nullptr)) {
    MY_LOGE("Fail to make_shared<PolicyTable>");
    return nullptr;
  }

  auto pMediatorTable = std::make_shared<MediatorTable>();
  if (CC_UNLIKELY(pMediatorTable == nullptr)) {
    MY_LOGE("Fail to make_shared<MediatorTable>");
    return nullptr;
  }

  return decidePolicyAndMake(params, pPolicyTable, pMediatorTable);
}
