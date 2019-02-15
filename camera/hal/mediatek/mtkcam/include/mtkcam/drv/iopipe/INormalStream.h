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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_INORMALSTREAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_INORMALSTREAM_H_
//
#include <mtkcam/drv/def/IPostProcDef.h>
#include <mtkcam/pipeline/hwnode/P2Common.h>
//
#include <memory>
#include <vector>

using NSCam::v3::P2Common::StreamConfigure;

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v4l2 {

enum EJpgCmd {};

enum ESDCmd {
  ENormalStreamCmd_Debug = 0,
  ENormalStreamCmd_ISPOnly,
  ESDCmd_CONFIG_VENC_DIRLK,
  ESDCmd_RELEASE_VENC_DIRLK
};

enum ENormalStreamTag {
  ENormalStreamTag_Normal = 0,
  ENormalStreamTag_Normal_S,
  ENormalStreamTag_Prv,
  ENormalStreamTag_Prv_S,
  ENormalStreamTag_Cap,
  ENormalStreamTag_Cap_S,
  ENormalStreamTag_Rec,
  ENormalStreamTag_Rec_S,
  ENormalStreamTag_Rep,
  ENormalStreamTag_Rep_S,
  ENormalStreamTag_3DNR,
  ENormalStreamTag_Vss = ENormalStreamTag_Normal,
  ENormalStreamTag_vFB_FB,
  ENormalStreamTag_MFB_Bld,
  ENormalStreamTag_MFB_Mix,
  ENormalStreamTag_Bokeh,
  ENormalStreamTag_FE,
  ENormalStreamTag_FM,
  ENormalStreamTag_COLOR_EFT,
  ENormalStreamTag_DeNoise,
  ENormalStreamTag_WUV,
  ENormalStreamTag_Y16Dump,
  ENormalStreamTag_total
};

#define MAX_UNUSED_NODE_NUM_OF_TOPOLOGY (5)

#define V4L2_CID_PRIVATE_UT_NUM (V4L2_CID_USER_BASE | 0x1001)

#define V4L2_CID_PRIVATE_SET_CTX_MODE_NUM (V4L2_CID_PRIVATE_UT_NUM + 1)
#define V4L2_CID_PRIVATE_SET_BUFFER_USAGE (V4L2_CID_PRIVATE_UT_NUM + 2)

#define MTK_ISP_CTX_MODE_DEBUG_OFF (0)
#define MTK_ISP_CTX_MODE_DEBUG_BYPASS_JOB_TRIGGER (1)
#define MTK_ISP_CTX_MODE_DEBUG_BYPASS_ALL (2)

typedef enum EInBufferUsage_e {
  EINBUF_USAGE_RAW_INPUT = 0x10,
  EINBUF_USAGE_NR3D,  // VIPI
  EINBUF_USAGE_LCEI,  // LCEI
  EINBUF_USAGE_LSC,   // IMGCI
  EINBUF_USAGE_MAX
} EInBufferUsage;

typedef enum EOutBufferUsage_e {
  EOUTBUF_USAGE_MDP = 0x0,
  EOUTBUF_USAGE_FD,        // IMG2O
  EOUTBUF_USAGE_POSTPROC,  // IMG3O
  EOUTBUF_USAGE_MAX
} EOutBufferUsage;

/******************************************************************************
 *
 * @class INormalStream
 * @brief Post-Processing Pipe Interface for Normal Stream.
 * @details
 * The data path will be Mem --> ISP--XDP --> Mem.
 *
 ******************************************************************************/
class INormalStream {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Destructor.
  /**
   * @brief Disallowed to directly delete a raw pointer.
   */
  virtual ~INormalStream() {}

 public:  ////                    Attributes.
  /**
   * @brief init the pipe
   *
   * @details
   *
   * @note
   *
   * @param[in] szCallerName: caller name.
   *            mPipeID     : Stream PipeID
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL init(char const* szCallerName,
                     ENormalStreamTag streamTag = ENormalStreamTag_Normal,
                     bool hasTuning = true) = 0;

  virtual MBOOL init(char const* szCallerName,
                     const StreamConfigure config,
                     ENormalStreamTag streamTag = ENormalStreamTag_Normal,
                     bool hasTuning = true) = 0;
  /**
   * @brief uninit the pipe
   *
   * @details
   *
   * @note
   *
   * @param[in] szCallerName: caller name.
   *            mPipeID     : Stream PipeID
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL uninit(char const* szCallerName) = 0;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Buffer Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  /**
   * @brief En-queue a request into the pipe.
   *
   * @details
   *
   * @note
   *
   * @param[in] rParams: Reference to a request of QParams structure.
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by \n
   *   getLastErrorCode().
   */
  virtual MBOOL enque(NSCam::NSIoPipe::QParams* pParams) = 0;
  /**
   * @brief De-queue a result from the pipe.
   *
   * @details
   *
   * @note
   *
   * @param[in] rParams: Reference to a result of QParams structure.
   *
   * @param[in] i8TimeoutNs: timeout in nanoseconds \n
   *      If i8TimeoutNs > 0, a timeout is specified, and this call will \n
   *      be blocked until a result is ready. \n
   *      If i8TimeoutNs = 0, this call must return immediately no matter \n
   *      whether any buffer is ready or not. \n
   *      If i8TimeoutNs = -1, an infinite timeout is specified, and this call
   *      will block forever.
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL deque(NSCam::NSIoPipe::QParams* p_rParams,
                      MINT64 i8TimeoutNs = -1) = 0;

  virtual MBOOL requestBuffers(
      int type,
      IImageBufferAllocator::ImgParam imgParam,
      std::vector<std::shared_ptr<IImageBuffer>>* p_buffers) = 0;

  /**
   * @brief send isp extra command
   *
   * @details
   *
   * @note
   *
   * @param[in] cmd: command
   * @param[in] arg: arg
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * sendCommand().
   */
  virtual MBOOL sendCommand(ESDCmd cmd,
                            MINTPTR arg1 = 0,
                            MINTPTR arg2 = 0,
                            MINTPTR arg3 = 0) = 0;
};
/****************************************************************************
 *
 ****************************************************************************/
};      // namespace v4l2
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_INORMALSTREAM_H_
