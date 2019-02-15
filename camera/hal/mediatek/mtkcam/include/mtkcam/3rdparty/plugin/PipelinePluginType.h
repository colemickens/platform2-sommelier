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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PIPELINEPLUGINTYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PIPELINEPLUGINTYPE_H_

#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
#include <mtkcam/3rdparty/plugin/Reflection.h>

// feature index for pipeline plugin
#include <mtkcam/3rdparty/customer/customer_feature_type.h>
#include <mtkcam/3rdparty/mtk/mtk_feature_type.h>

// policy and feature related info
#include <mtkcam/def/common.h>
#include <mtkcam/drv/IHalSensor.h>         // sensopr mode define
#include <mtkcam/pipeline/policy/types.h>  // zsl policy

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

/******************************************************************************
 * Common
 ******************************************************************************/

enum BufferCondition {
  eCond_AFStable = 1 << 0,
  eCond_AFSyncDone = 1 << 1,
  eCond_AEStable = 1 << 2,
  eCond_AESyncDone = 1 << 3,
};

enum ThumbnailTiming {
  eTiming_P2,
  eTiming_PLUGIN,
  eTiming_MDP,
  eTiming_JPEG,
};

enum Priority {
  ePriority_Lowest = 0x00,
  ePriority_Normal = 0x10,
  ePriority_Default = 0x80,
  ePriority_Highest = 0xFF,
};

enum FaceData {
  eFD_None,
  eFD_Cache,
  eFD_Current,
};

enum InitPhase {
  ePhase_OnPipeInit,
  ePhase_OnRequest,
};

enum SelStage {
  eSelStage_CFG,
  eSelStage_P1,
  eSelStage_P2,
};

enum JoinEntry {
  eJoinEntry_S_YUV,
  eJoinEntry_S_RAW,
  eJoinEntry_S_ASYNC,
  eJoinEntry_S_DISP_ONLY,
};

/* provide strategy info for plugin provider during negotiate */
struct StrategyInfo {
  bool isZslModeOn = false;  // is Zsl buffers pool created.
  bool isZslFlowOn = false;  // is Zsl capture behavior request.
  bool isFlashOn = false;
  uint32_t exposureTime = 0;
  uint32_t realIso = 0;
  uint32_t customHint = 0;

  // sensor info for feature strategy
  int32_t sensorId = -1;
  uint32_t sensorMode = SENSOR_SCENARIO_ID_UNNAMED_START;
  int32_t sensorFps = 0;
  MSize sensorSize = MSize(0, 0);
  MSize rawSize = MSize(0, 0);
  // reserver
};

/* plugin provider retrun required config info during negotiate */
struct RequestInfo {
  // sensor setting (ex: sensor mode)
  MUINT sensorMode = SENSOR_SCENARIO_ID_UNNAMED_START;

  // zsl policy and request
  bool needZslFlow = false;
  v3::pipeline::policy::ZslPolicyParams zslPolicyParams;

  // reserver
};

/******************************************************************************
 * RAW Interface
 ******************************************************************************/
class VISIBILITY_PUBLIC Raw {
 public:
  struct Property {
    FIELDS((const char*)mName, (MUINT64)mFeatures, (MBOOL)mInPlace)
  };

  struct Selection {
    FIELDS((BufferSelection)mIBufferFull,
           (BufferSelection)mOBufferFull,
           (MetadataSelection)mIMetadataDynamic,
           (MetadataSelection)mIMetadataApp,
           (MetadataSelection)mIMetadataHal,
           (MetadataSelection)mOMetadataApp,
           (MetadataSelection)mOMetadataHal)
    StrategyInfo mIStrategyInfo;
    RequestInfo mORequestInfo;
  };

  struct Request {
    FIELDS((BufferHandle::Ptr)mIBufferFull,
           (BufferHandle::Ptr)mIBufferLCS,
           (BufferHandle::Ptr)mOBufferFull,
           (MetadataHandle::Ptr)mIMetadataDynamic,
           (MetadataHandle::Ptr)mIMetadataApp,
           (MetadataHandle::Ptr)mIMetadataHal,
           (MetadataHandle::Ptr)mOMetadataApp,
           (MetadataHandle::Ptr)mOMetadataHal)
  };
};

typedef PipelinePlugin<Raw> RawPlugin;

/******************************************************************************
 * YUV Interface
 ******************************************************************************/

class VISIBILITY_PUBLIC Yuv {
 public:
  struct Property {
    FIELDS((const char*)mName,
           (MUINT64)mFeatures,
           (MBOOL)mInPlace,
           (MUINT8)mFaceData,
           (MUINT8)mInitPhase,  // init at pipeline init
           (MUINT8)mPriority,
           (MUINT8)mPosition,  // YUV plugin point: 0->YUV, 1->YUV2
           (MBOOL)mSupportCrop,
           (MBOOL)mSupportScale)
  };

  struct Selection {
    FIELDS((BufferSelection)mIBufferFull,
           (BufferSelection)mIBufferLCS,
           (BufferSelection)mOBufferFull,
           (BufferSelection)mOBufferCropA,
           (BufferSelection)mOBufferCropB,
           (MetadataSelection)mIMetadataDynamic,
           (MetadataSelection)mIMetadataApp,
           (MetadataSelection)mIMetadataHal,
           (MetadataSelection)mOMetadataApp,
           (MetadataSelection)mOMetadataHal)
    StrategyInfo mIStrategyInfo;
    RequestInfo mORequestInfo;
  };

  struct Request {
    FIELDS((BufferHandle::Ptr)mIBufferFull,
           (BufferHandle::Ptr)mIBufferLCS,
           (BufferHandle::Ptr)mOBufferFull,
           (BufferHandle::Ptr)mOBufferCropA,
           (BufferHandle::Ptr)mOBufferCropB,
           (MetadataHandle::Ptr)mIMetadataDynamic,
           (MetadataHandle::Ptr)mIMetadataApp,
           (MetadataHandle::Ptr)mIMetadataHal,
           (MetadataHandle::Ptr)mOMetadataApp,
           (MetadataHandle::Ptr)mOMetadataHal)
  };
};

typedef PipelinePlugin<Yuv> YuvPlugin;

/******************************************************************************
 * Join Interface
 ******************************************************************************/
class Join {
 public:
  struct Property {
    FIELDS((const char*)mName, (MUINT64)mFeatures)
  };

  struct Selection {
    FIELDS((MUINT32)mSelStage,
           (MUINT32)mCfgOrder,
           (MUINT32)mCfgJoinEntry,
           (MBOOL)mCfgInplace,
           (MBOOL)mCfgEnableFD,
           (MBOOL)mCfgRun,
           (MBOOL)mP2Run,
           (BufferSelection)mIBufferMain1,
           (BufferSelection)mIBufferMain2,
           (BufferSelection)mIBufferDownscale,
           (BufferSelection)mIBufferDepth,
           (BufferSelection)mIBufferLCS1,
           (BufferSelection)mIBufferLCS2,
           (BufferSelection)mIBufferRSS1,
           (BufferSelection)mIBufferRSS2,
           (BufferSelection)mOBufferMain1,
           (BufferSelection)mOBufferMain2,
           (BufferSelection)mOBufferDepth,
           (MetadataSelection)mIMetadataDynamic1,
           (MetadataSelection)mIMetadataDynamic2,
           (MetadataSelection)mIMetadataApp,
           (MetadataSelection)mIMetadataHal1,
           (MetadataSelection)mIMetadataHal2,
           (MetadataSelection)mOMetadataApp,
           (MetadataSelection)mOMetadataHal)
  };

  struct Request {
    FIELDS((BufferHandle::Ptr)mIBufferMain1,
           (BufferHandle::Ptr)mIBufferMain2,
           (BufferHandle::Ptr)mIBufferDownscale,
           (BufferHandle::Ptr)mIBufferDepth,
           (BufferHandle::Ptr)mIBufferLCS1,
           (BufferHandle::Ptr)mIBufferLCS2,
           (BufferHandle::Ptr)mIBufferRSS1,
           (BufferHandle::Ptr)mIBufferRSS2,
           (BufferHandle::Ptr)mOBufferMain1,
           (BufferHandle::Ptr)mOBufferMain2,
           (BufferHandle::Ptr)mOBufferDepth,
           (MetadataHandle::Ptr)mIMetadataDynamic1,
           (MetadataHandle::Ptr)mIMetadataDynamic2,
           (MetadataHandle::Ptr)mIMetadataApp,
           (MetadataHandle::Ptr)mIMetadataHal1,
           (MetadataHandle::Ptr)mIMetadataHal2,
           (MetadataHandle::Ptr)mOMetadataApp,
           (MetadataHandle::Ptr)mOMetadataHal)
  };
};

typedef PipelinePlugin<Join> JoinPlugin;

};      // namespace NSPipelinePlugin
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PIPELINEPLUGINTYPE_H_
