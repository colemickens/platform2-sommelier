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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_IPOSTPROCDEF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_IPOSTPROCDEF_H_
#include <vector>
//
#include <mtkcam/def/common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/drv/iopipe/Port.h>
#include <mtkcam/drv/iopipe/PortMap.h>
#include "IPostProcFeFm.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {

/******************************************************************************
 * @define MAX_PIPE_USER_NUMBER
 *
 * @brief maximum user number of each pass2 pipe.
 *
 ******************************************************************************/
#define MAX_PIPE_USER_NUMBER 32
#define MAX_PIPE_WPEUSER_NUMBER 8

/******************************************************************************
 * @enum StreamPipeID
 *
 * @brief Enum ID for each stream pipe in pass2 control.
 *
 ******************************************************************************/
enum EStreamPipeID {
  EStreamPipeID_None = 0,
  EStreamPipeID_Normal,
  EStreamPipeID_WarpEG,
  EStreamPipeID_Total
};

/******************************************************************************
 * @struct MCropRect
 *
 * @brief Cropped Rectangle.
 *
 * @param[in] p_fractional: fractional part of left-top corner in pixels.
 *
 * @param[in] p_integral: integral part of left-top corner in pixels.
 *
 * @param[in] s: size (i.e. width and height) in pixels.
 *
 ******************************************************************************/
struct MCropRect {
  typedef int value_type;
  MPoint p_fractional;      //  left-top corner
  MPoint p_integral;        //  left-top corner
  MSize s;                  //  size: width, height
  value_type w_fractional;  //  float width;
  value_type h_fractional;  //  float height

 public:  ////                Instantiation.
  // we don't provide copy-ctor and copy assignment on purpose
  // because we want the compiler generated versions

  explicit inline MCropRect(int _w = 0, int _h = 0)
      : p_fractional(0, 0), p_integral(0, 0), s(_w, _h) {}

  inline MCropRect(MPoint const& topLeft, MPoint const& bottomRight)
      : p_fractional(0, 0), p_integral(topLeft), s(topLeft, bottomRight) {}

  inline MCropRect(MPoint const& _p, MSize const& _s)
      : p_fractional(0, 0), p_integral(_p), s(_s) {}

  explicit inline MCropRect(MRect const& _rect)
      : p_fractional(0, 0), p_integral(_rect.leftTop()), s(_rect.size()) {}
};

/******************************************************************************
 * @struct MCrpRsInfo
 *
 * @brief Cropped Rectangle and Resize Information for whole pipe.
 *
 * @param[in] mCropRect: cropped rectangle.
 *
 * @param[in] mResizeDst: resized size of current dst buffer.
 *
 * @param[in] mMdpGroup: group information for mdp crop. 0 stands for MDP_CROP,
 *1 stands for MDP_CROP2.
 *
 ******************************************************************************/
struct MCrpRsInfo {
  MUINT32 mFrameGroup;
  MINT32 mGroupID;
  MUINT32 mMdpGroup;
  MCropRect mCropRect;
  MSize mResizeDst;
  MCrpRsInfo()
      : mFrameGroup(0), mGroupID(0), mMdpGroup(0), mResizeDst(mCropRect.s) {}
};

/*****************************************************************************
 * @struct MCropPathInfo
 *
 * @brief Crop path information.
 *
 * @param[in] mGroupNum: number of crop group.
 *
 * @param[in] mGroupID: crop group id.
 *
 * @param[in] mvPorts: dma port in each crop group.
 *
 *****************************************************************************/
struct MCropPathInfo {
  MUINT32 mGroupIdx;
  std::vector<MUINT32> mvPorts;

 public:
  MCropPathInfo() { mGroupIdx = 0; }
};

///////////////////////////////////////////////////
// test struct
struct ExtraParams {
  unsigned int imgFmt;
  int imgw;
  int imgh;
  MUINTPTR memVA;
  MUINTPTR memPA;
  int memID;
  unsigned int memSize;
  int p2pxlID;
  ExtraParams(MUINT32 _imgFmt = 0x0,
              MINT32 _imgw = 0,
              MINT32 _imgh = 0,
              MUINT32 _size = 0,
              MINT32 _memID = -1,
              MUINTPTR _virtAddr = 0,
              MUINTPTR _phyAddr = 0,
              MINT32 _p2pxlID = 0)
      : imgFmt(_imgFmt),
        imgw(_imgw),
        imgh(_imgh),
        memVA(_virtAddr),
        memPA(_phyAddr),
        memID(_memID),
        memSize(_size),
        p2pxlID(_p2pxlID) {}
};
///////////////////////////////////////////////////

/******************************************************************************
 *
 * @struct ModuleInfo
 * @brief parameter for specific hw module or dma statistic data which need be
 *by frame set
 * @details
 *
 ******************************************************************************/
struct ModuleInfo {
  MUINT32 moduleTag;
  MINT32 frameGroup;
  MVOID* moduleStruct;

 public:  //// constructors.
  ModuleInfo() : moduleTag(0x0), frameGroup(0), moduleStruct(NULL) {}
  //
};

/******************************************************************************
 * @struct Input
 *
 * @brief Pipe input parameters.
 *
 * @param[in] mPortID: The input port ID of the pipe.
 *
 * @param[in] mBuffer: A pointer to an image buffer.
 *            Callee must lock, unlock, and signal release-fence.
 *
 * @param[in] mCropRect: Input CROP is applied BEFORE transforming and resizing.
 *
 * @param[in] mTransform: ROTATION CLOCKWISE is applied AFTER FLIP_{H|V}.
 *
 ******************************************************************************/
struct Input {
 public:  ////                    Fields (Info)
  PortID mPortID;
  IImageBuffer* mBuffer;

 public:  ////                    Fields (Operations)
  MINT32 mTransform;
  MUINT32 mOffsetInBytes;

 public:  ////                    Constructors.
  Input(PortID const& rPortID = PortID(),
        IImageBuffer* buffer = nullptr,
        MINT32 const transform = 0,
        MUINT32 const offsetInBytes = 0)
      : mPortID(rPortID),
        mBuffer(buffer),
        mTransform(transform),
        mOffsetInBytes(offsetInBytes) {}
};

/******************************************************************************
 * @struct EDIPInfoEnum
 *
 * @brief EDIPInfoEnum.
 ******************************************************************************/
enum EDIPInfoEnum { EDIPINFO_DIPVERSION, EDIPINFO_MAX };

/******************************************************************************
 * @struct EDIPInfoEnum
 *
 * @brief EDIPInfoEnum.
 ******************************************************************************/
enum EDIPHWVersionEnum {
  EDIPHWVersion_40 = 0x40,
  EDIPHWVersion_50 = 0x50,
  EDIPHWVersion_MAX
};

/******************************************************************************
 * @struct Output
 *
 * @brief Pipe output parameters.
 *
 * @param[in] mPortID: The output port ID of the pipe.
 *
 * @param[in] mBuffer: A pointer to an image buffer.
 *            Output CROP is applied AFTER the transform.
 *            Callee must lock, unlock, and signal release-fence.
 *
 * @param[in/out] mTransform: ROTATION CLOCKWISE is applied AFTER FLIP_{H|V}.
 *            The result of transform must be set by the pipe if the request of
 *            transform is not supported by the pipe.
 *
 ******************************************************************************/
struct Output {
 public:  ////                    Fields (Info)
  PortID mPortID;
  IImageBuffer* mBuffer;

 public:  ////                    Fields (Operations)
  MINT32 mTransform;
  MUINT32 mOffsetInBytes;

 public:  ////                    Constructors.
  Output(PortID const& rPortID = PortID(),
         IImageBuffer* buffer = nullptr,
         MINT32 const transform = 0,
         MUINT32 const offsetInBytes = 0)
      : mPortID(rPortID),
        mBuffer(buffer),
        mTransform(transform),
        mOffsetInBytes(offsetInBytes) {}
};

/******************************************************************************
 * @struct PQParam
 *
 * @brief PQParam.
 *
 * @param[in] CmdIdx: specific command index: EPIPE_MDP_PQPARAM_CMD
 *
 * @param[in] moduleStruct: specific structure: PQParam
 *
 * @param[in] s: WDMAPQParam  (i.e. DpPqParam) which is defined by MDP PQ owner
 * @param[in] s: WROTPQParam  (i.e. DpPqParam) which is defined by MDP PQ owner
 *
 ******************************************************************************/
struct PQParam {
  MVOID* WDMAPQParam;
  MVOID* WROTPQParam;
  PQParam() : WDMAPQParam(NULL), WROTPQParam(NULL) {}
};

/******************************************************************************
 * @struct CrspInfo
 *
 * @brief CrspInfo.
 *
 * @param[in] CmdIdx: specific command index: EPIPE_IMG3O_CRSPINFO_CMD
 *
 * @param[in] moduleStruct: specific structure: CrspInfo
 *
 * @param[in] s: this command will only use m_CrspInfo.p_integral.x,
 *m_CrspInfo.p_integral.y, m_CrspInfo.s.w and m_CrspInfo.s.h
 *
 ******************************************************************************/
struct CrspInfo {
  MCropRect m_CrspInfo;
};

/******************************************************************************
 * @struct ExtraPara
 *
 * @brief ExtraPara.
 *
 * @param[in] CmdIdx: specific command index to responding modulestruct
 *
 * @param[in] moduleStruct: specific structure accorind to command index
 *
 * @param[in] s: size (i.e. width and height) in pixels.
 *
 ******************************************************************************/
enum EPostProcCmdIndex {
  EPIPE_FE_INFO_CMD,      // FE
  EPIPE_FM_INFO_CMD,      // FM
  EPIPE_WPE_INFO_CMD,     // Wrapping Engine
  EPIPE_MDP_PQPARAM_CMD,  // MDP PQ Param, if MW have any requirement of MDP PQ,
                          // please use this command to pass the PQ param
  EPIPE_IMG3O_CRSPINFO_CMD,  // NR3D IMG3O CRSP used, some times the preview
                             // frame will use imgo output as imgi input.
  EPIPE_TOTAL_CMD,
};

struct ExtraParam {
  EPostProcCmdIndex CmdIdx;
  MVOID* moduleStruct;

 public:  //// constructors.
  ExtraParam() : CmdIdx(EPIPE_TOTAL_CMD), moduleStruct(NULL) {}
  //
};

/******************************************************************************
 *
 * @struct FrameParams
 *
 * @brief Queuing parameters for the pipe.
 *      input cropping -> resizing
 *      output flip_{H|V} -> output rotation -> output cropping
 *
 * @param[in] mpCookie: frame callback cookie; it shouldn't be modified by the
 *pipe.
 *
 * @param[in] mvIn: a vector of input parameters.
 *
 * @param[in] mvOut: a vector of output parameters.
 *
 * @param[in] mCropRsInfo: a array of pipe crop/resize information.
 *
 * @param[in] mvExtraParam: extra command information in this frame request.
 *
 ******************************************************************************/

struct FrameParams {
  MUINT32 FrameNo;
  MUINT32 RequestNo;
  MUINT32 Timestamp;
  MINT32 UniqueKey;
  MINT32 mStreamTag;
  MINT32 mSensorIdx;
  MVOID* mTuningData;
  MVOID* mpCookie;
  int mTuningData_fd;
  std::vector<Input> mvIn;
  std::vector<Output> mvOut;
  std::vector<MCrpRsInfo> mvCropRsInfo;
  std::vector<ModuleInfo> mvModuleData;
  std::vector<ExtraParam> mvExtraParam;
  FrameParams()
      : FrameNo(0),
        RequestNo(0),
        Timestamp(0),
        UniqueKey(0),
        mStreamTag(-1),
        mSensorIdx(-1),
        mTuningData(NULL),
        mpCookie(NULL),
        mTuningData_fd(-1),
        mvIn(),
        mvOut(),
        mvCropRsInfo(),
        mvModuleData(),
        mvExtraParam() {}
};

/******************************************************************************
 *
 * @struct QParams
 *
 * @brief Queuing parameters for the pipe.
 *      input cropping -> resizing
 *      output flip_{H|V} -> output rotation -> output cropping
 *
 * @param[in] mpfnCallback: a pointer to a callback function.
 *      If it is NULL, the pipe must put the result into its result queue, and
 *      then a user will get the result by deque() from the pipe later.
 *      If it is not NULL, the pipe does not put the result to its result queue.
 *      The pipe must invoke a callback with the result.
 *
 * @param[in] mpCookie: callback cookie; it shouldn't be modified by the pipe.
 *
 * @param[in] mpfnEnQFailCallback: call back for enque fail.
 *
 * @param[in] mpfnEnQBlockCallback: call back for enque blocking.
 *
 * @param[in]  mCropRsInfo: a array of pipe crop/resize information.
 *
 * @param[in]  mFrameNo: frame number, starting from 0.
 * @param[out] mDequeSuccess:  driver dequeue data status
 * @param[in/out] mvFrameParams,frame params vector
 *
 ******************************************************************************/
struct QParams {
  typedef MVOID (*PFN_CALLBACK_T)(QParams* pParams);
  PFN_CALLBACK_T mpfnCallback;
  PFN_CALLBACK_T mpfnEnQFailCallback;
  PFN_CALLBACK_T mpfnEnQBlockCallback;
  MVOID* mpCookie;
  MBOOL mDequeSuccess;
  std::vector<FrameParams> mvFrameParams;
  //
  QParams()
      : mpfnCallback(NULL),
        mpfnEnQFailCallback(NULL),
        mpfnEnQBlockCallback(NULL),
        mpCookie(NULL),
        mDequeSuccess(MFALSE),
        mvFrameParams() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSIoPipe
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_IPOSTPROCDEF_H_
