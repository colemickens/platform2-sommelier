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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_CAMIO_V4L2IHALCAMIO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_CAMIO_V4L2IHALCAMIO_H_

#include <memory>
#include <vector>

#include "mtkcam/def/common.h"
#include "mtkcam/drv/def/ICam_type.h"
#include "mtkcam/drv/IHalSensor.h"
#include "mtkcam/drv/iopipe/Port.h"
#include "mtkcam/drv/iopipe/PortMap.h"
#include "mtkcam/utils/imgbuf/IImageBuffer.h"

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

enum EPipeSignal {
  EPipeSignal_NONE = 0x0000,    /*!< signal None */
  EPipeSignal_SOF = 0x0001,     /*!< signal Start Of Frame */
  EPipeSignal_EOF = 0x0002,     /*!< signal End of Frame */
  EPipeSignal_VSYNC = 0x0003,   // vsync
  EPipeSignal_AFDONE = 0x0004,  // AF done
  EPipeSignal_TG_INT = 0x0005,  // TG interrupt
  EPipeSignal_NUM,
};

typedef enum {
  eCamHwPathCfg_One_TG,
  eCamHwPathCfg_Two_TG,
  eCamHwPathCfg_Num,
} E_CamHwPathCfg;

struct QueryInData_t {
  MUINT32 sensorIdx;
  MUINT32 scenarioId;
  MUINT32 rrz_out_w;
  E_CamPattern pattern;

  QueryInData_t()
      : sensorIdx(0), scenarioId(0), rrz_out_w(0), pattern(eCAM_NORMAL) {}
};

struct QueryOutData_t {
  MUINT32 sensorIdx;
  MBOOL isTwin;
  E_CamIQLevel IQlv;
  MUINT32 clklv;
  MBOOL result;

  QueryOutData_t()
      : sensorIdx(0),
        isTwin(MFALSE),
        IQlv(eCamIQ_MAX),
        clklv(0),
        result(MFALSE) {}
};

enum EPipeSelect {
  EPipeSelect_None = 0x00000000,
  EPipeSelect_Normal = 0x00000001,
  EPipeSelect_NormalSv = 0x00000010,
};

typedef enum {
  E_1_SEN = 0,  // total have 1 input sensor
  E_2_SEN = 1,  // total have 2 input sensor like pip/daul_cam
} E_SEN;

struct SEN_INFO {
  MUINT32 sensorIdx;
  MUINT32 scenarioId;
  MUINT32 rrz_out_w;
  E_CamPattern pattern;
  MBOOL bin_off;  // force bin off
  MBOOL stt_off;  // no demand for output statistic data

  SEN_INFO()
      : sensorIdx(0),
        scenarioId(0),
        rrz_out_w(0),
        pattern(eCAM_NORMAL),
        bin_off(MFALSE),
        stt_off(MFALSE) {}
};

struct PIPE_SEL {
  MUINT32 sensorIdx;
  MUINT32 pipeSel;

  PIPE_SEL() : sensorIdx(0), pipeSel(EPipeSelect_None) {}
};

typedef enum {
  ENPipe_UNKNOWN = 0x0,
  ENPipe_CAM_A = 0x01,
  ENPipe_CAM_B = 0x02,
  ENPipe_CAM_C = 0x10,
  ENPipe_CAM_MAX = 4,  // number of above enum
} ENPipe_CAM;

#define IOPIPE_MAX_SENSOR_CNT (5)
#define IOPIPE_MAX_NUM_USERS (16)
#define IOPIPE_MAX_USER_NAME_LEN (32)

enum ENPipeCmd {
  ENPipeCmd_GET_TG_INDEX = 0x0800,
  ENPipeCmd_GET_BURSTQNUM = 0x0801,
  ENPipeCmd_SET_STT_SOF_CB = 0x0802,
  ENPipeCmd_CLR_STT_SOF_CB = 0x0803,
  ENPipeCmd_GET_LAST_ENQ_SOF = 0x0804,
  ENPipeCmd_SET_MEM_CTRL = 0x0805,
  ENPipeCmd_SET_IDLE_HOLD_CB = 0x0806,
  ENPipeCmd_SET_SUSPEND_STAT_CB = 0x0807,
  ENPipeCmd_GET_STT_CUR_BUF = 0x0812,
  ENPipeCmd_SET_STT_SUSPEND_CB = 0x0813,
  ENPipeCmd_SET_HW_PATH_CFG = 0x0814,
  ENPipeCmd_GET_HW_PATH_CFG = 0x0815,
  ENPipeCmd_SET_ENQ_THRD_CB = 0x0816,

  ENPIPECmd_AE_SMOOTH = 0x1105,
  ENPipeCmd_HIGHSPEED_AE = 0x1106,
  ENPipeCmd_SET_TG_INT_LINE = 0x110E,
  ENPipeCmd_GET_TG_OUT_SIZE = 0x110F,
  ENPipeCmd_GET_RMX_OUT_SIZE = 0x1110,  //
  ENPipeCmd_GET_HBIN_INFO = 0x1111,     //
  ENPipeCmd_GET_EIS_INFO = 0x1112,
  ENPipeCmd_GET_UNI_INFO = 0x1113,
  ENPipeCmd_GET_BIN_INFO = 0x1114,
  ENPipeCmd_GET_RSS_INFO = 0x1115,
  ENPipeCmd_SET_EIS_CBFP = 0x1117,
  ENPipeCmd_SET_LCS_CBFP = 0x1118,
  ENPipeCmd_SET_SGG2_CBFP = 0X1119,
  ENPipeCmd_SET_RSS_CBFP = 0X111A,
  ENPipeCmd_GET_PMX_INFO = 0X111B,
  ENPipeCmd_GET_IMGO_INFO = 0x111C,
  ENPipeCmd_GET_CUR_FRM_STATUS = 0x111D,
  ENPipeCmd_GET_CUR_SOF_IDX = 0x111E,
  ENPipeCmd_GET_RCP_SIZE = 0x111F,  // arg1's data type: V_NormalPipe_CROP_INFO
  ENPipeCmd_UNI_SWITCH = 0x1120,
  ENPipeCmd_GET_UNI_SWITCH_STATE = 0x1121,
  ENPipeCmd_GET_MAGIC_REG_ADDR =
      0x1122,  // arg1: output the register address of magic number. data type:
               // V_NormalPipe_MagReg.
  ENPipeCmd_GET_DTwin_INFO =
      0x1123,  // arg1: output dynamic twin is turned ON/OFF , only
               // Bianco/Vinson/Cannon support.
  ENPipeCmd_GET_TWIN_REG_ADDR =
      0x1124,  // arg1: output the register address of twin status. data type:
               // V_NormalPipe_TwinReg.
  ENPipeCmd_SET_FAST_AF = 0x1127,
  ENPipeCmd_SET_RRZ_CBFP = 0x1128,
  ENPipeCmd_SET_TUNING_CBFP = 0x1129,
  ENPipeCmd_SET_REGDUMP_CBFP = 0x1130,
  ENPipeCmd_GET_FLK_INFO = 0x113A,
  ENPipeCmd_SET_QUALITY = 0x113B,
  ENPipeCmd_GET_QUALITY =
      0x113C, /*configpipe result is also use this cmd to get first IQ value*/
  ENPipeCmd_GET_RMB_INFO = 0x113D,
  ENPipeCmd_SET_AWB_CBFP = 0x113E,
  // cmd for p1hwcfg, only isp3.0 support
  ENPipeCmd_SET_MODULE_EN = 0x1401,
  ENPipeCmd_SET_MODULE_SEL = 0x1402,
  ENPipeCmd_SET_MODULE_CFG = 0x1403,
  ENPipeCmd_GET_MODULE_HANDLE = 0x1404,
  ENPipeCmd_SET_MODULE_CFG_DONE = 0x1405,
  ENPipeCmd_RELEASE_MODULE_HANDLE = 0x1406,
  ENPipeCmd_SET_MODULE_DBG_DUMP = 0x1407,
  // cmd for AF special HBIN1 request
  ENPipeCmd_GET_HBIN1_INFO = 0x1408,

  // V4L2 extension
  ENPipeCmd_GEN_MAGIC_NUM = 0x4001,  // arg1: address of an uint32_t, the magic
                                     // number, arg2: keeps it 0
  ENPipeCmd_SET_META2_DISABLED = 0x4002,  // disable link between meta2

  ENPipeCmd_MAX
};

enum ENPipeQueryCmd {
  ENPipeQueryCmd_UNKNOWN = 0x00000000,
  ENPipeQueryCmd_X_PIX = 0x00000001,
  ENPipeQueryCmd_X_BYTE = 0x00000002,
  ENPipeQueryCmd_CROP_X_PIX =
      0x00000004,  // query when using cropping, (cropping size != input size)
  ENPipeQueryCmd_CROP_X_BYTE =
      0x00000008,  // query when using cropping, (cropping size != input size)
  ENPipeQueryCmd_CROP_START_X = 0x00000010,  // query when using cropping, the
                                             // unit of crop start x is pixel.
  ENPipeQueryCmd_STRIDE_PIX = 0x00000020,
  ENPipeQueryCmd_STRIDE_BYTE = 0x00000040,
  ENPipeQueryCmd_CONSTRAIN = 0x00000080,  // constrain size related cmd
  //
  ENPipeQueryCmd_MAX_SEN_NUM = 0x00000100,
  ENPipeQueryCmd_PIPELINE_BITDEPTH = 0x00000200,
  ENPipeQueryCmd_IQ_LEVEL = 0x00000400,
  ENPipeQueryCmd_ISP_RES = 0x00000800,
  //
  ENPipeQueryCmd_BURST_NUM = 0x00001000,
  ENPipeQueryCmd_SUPPORT_PATTERN = 0x00002000,
  ENPipeQueryCmd_MAX_PREVIEW_SIZE = 0x00004000,
  //
  ENPipeQueryCmd_QUERY_FMT = 0x10000000,
  ENPipeQueryCmd_BS_RATIO =
      0x20000000,                      // bayer scaler max scaling ratio,unit:%
  ENPipeQueryCmd_D_Twin = 0x40000000,  // query dynamic twin is supported or not
  ENPipeQueryCmd_D_BayerEnc =
      0x80000000,  // query dynamic Bayer encoder is supported or not
  //
  ENPipeQueryCmd_UNI_NUM = 0x01000000,  // query numbers of UNI(ex.EIS/FLK..)
  ENPipeQueryCmd_DYNAMIC_PAK =
      0x02000000,  // query dynamic pak is supported or not
  ENPipeQueryCmd_RSV = 0x04000000,
  // query hw-function is supported or not at current sensor.(ref to
  // struct:Que_Func)
  ENPipeQueryCmd_FUNC = (ENPipeQueryCmd_RSV | ENPipeQueryCmd_MAX_SEN_NUM |
                         ENPipeQueryCmd_D_Twin | ENPipeQueryCmd_D_BayerEnc),

  ENPipeQueryCmd_HW_RES_MGR =
      0x00100000,  // hardware resource management, query which kind of pipeline
                   // to use, cam?, camsv?, dcif?, or stagger?
  ENPipeQueryCmd_PDO_AVAILABLE =
      0x00200000,  // query whether platform supports pdo or not
  ENPipeQueryCmd_HW_RES_ALLOC = 0x00400000,
};

enum EPipeRawFmt {
  EPipe_PROCESSED_RAW = 0x0000, /*!< Processed raw */
  EPipe_PURE_RAW = 0x0001,      /*!< Pure raw */
  /*for pso*/
  EPipe_BEFORE_LSC = 0x0000, /*!< Before LSC*/
  EPipe_BEFORE_OB = 0x0001,  /*!< Before OB */
};

enum BufPlaneID {
  kPlane_1st = 0,
  kPlane_2nd,
  kPlane_3rd,
  kPlane_max,
};

struct NormalPipe_QueryIn {
  NormalPipe_QueryIn(MUINT32 _width = 0,
                     EImageFormat _img_fmt = eImgFmt_UNKNOWN,
                     E_CamPixelMode _pix_mode = ePixMode_NONE)
      : width(_width), img_fmt(_img_fmt), pix_mode(_pix_mode) {}
  MUINT32 width;  // unit:pix
  EImageFormat img_fmt;
  E_CamPixelMode pix_mode;
};

struct NormalPipe_InputInfo {
  EImageFormat format;
  MUINT32 width;
  E_CamPixelMode pixelMode;
  MUINT32 sensorIdx;
  MUINT32 scenarioId;
  MUINT32 rrz_out_w;
  E_CamPattern pattern;
  MBOOL bin_off;

  NormalPipe_InputInfo(EImageFormat _format = eImgFmt_IMPLEMENTATION_DEFINED,
                       MUINT32 _width = 0,
                       E_CamPixelMode _pixelMode = ePixMode_NONE)
      : format(_format),
        width(_width),
        pixelMode(_pixelMode),
        sensorIdx(0),
        scenarioId(0),
        rrz_out_w(0),
        pattern(eCAM_NORMAL),
        bin_off(MFALSE) {}
};

union Que_Func {
  struct {
    MUINT32 D_TWIN : 1;      // 1: dynamic twin is ON, 0: dynamic twin is OFF.
    MUINT32 SensorNum : 2;   // max sensor number for direct-link
    MUINT32 D_BayerENC : 1;  // 1: dynamic bayer encorder(perframe ctrl) , 0:
                             // static bayer encoder.
    MUINT32 rsv : 28;
  } Bits;
  MUINT32 Raw;
};

struct NormalPipe_QueryInfo {
  MUINT32 x_pix;        // horizontal resolution, unit:pix
  MUINT32 stride_pix;   // stride, uint:pix. this is a approximative value under
                        // pak mode
  MUINT32 stride_byte;  // stride, uint:byte
  MUINT32 stride_B[3];  // 3-plane stride. unit:byte
  MUINT32 xsize_byte;
  MUINT32 crop_x;                              // crop start point-x , unit:pix
  std::vector<NSCam::EImageFormat> query_fmt;  // query support fmt
  MUINT32 bs_ratio;          // bayer scaler scaling ratio, unit:%
  MUINT32 pipelinebitdepth;  // how many bits pipeline deal with
  MBOOL D_TWIN;              // 1: dynamic twin is ON, 0: dynamic twin is OFF.
  MBOOL IQlv;
  MUINT32 sen_num;
  Que_Func function;
  MUINT32 uni_num;   // the number of uni
  MBOOL D_Pak;       // 1: support dynamic pak  0: no support dynamic pak.
  MUINT32 burstNum;  // support burst number
  MUINT32 pattern;   // support pattern
  NormalPipe_QueryInfo(MUINT32 _x_pix = 0,
                       MUINT32 _stride_pix = 0,
                       MUINT32 _stride_byte = 0,
                       MUINT32 _xsize_byte = 0,
                       MUINT32 _crop_x = 0,
                       MUINT32 _bs_ratio = 100,
                       MUINT32 _pipelinebitdepth = CAM_Pipeline_12BITS)
      : x_pix(_x_pix),
        stride_pix(_stride_pix),
        stride_byte(_stride_byte),
        xsize_byte(_xsize_byte),
        crop_x(_crop_x),
        bs_ratio(_bs_ratio),
        pipelinebitdepth(_pipelinebitdepth),
        D_TWIN(0),
        IQlv(0),
        sen_num(2),
        uni_num(1),
        D_Pak(MFALSE),
        burstNum(0),
        pattern(0) {
    stride_B[0] = stride_B[1] = stride_B[2] = 0;
    query_fmt.clear();
    D_TWIN = 0;
    sen_num = 2;
    function.Raw = 0;
    function.Bits.D_TWIN = D_TWIN;
    function.Bits.SensorNum = sen_num;
    function.Bits.D_BayerENC = 0;
  }
};

/* V4L2 related info */
enum PipeTag {
  kPipeTag_Unknown = 0x00000000,
  kPipeTag_Tuning = 0x00010000,
  /** 1 dma output */
  kPipeTag_Out1 = 0x00000001,
  /** 2 dma output */
  kPipeTag_Out2 = 0x00000002,
  /** 1 dma output + tuning */
  kPipeTag_Out1_Tuning = kPipeTag_Tuning | kPipeTag_Out1,
  /** 2 dma output + tuning */
  kPipeTag_Out2_Tuning = kPipeTag_Tuning | kPipeTag_Out2,
  /** pipe tag count */
  kPipeTag_Num
};

/* ISP pipe type  */
enum IspPipeType {
  /** this pipe is used to control the path of imgo, rrzo */
  kPipeNormal = 0,
  /** this pipe is used to control the path of meta 1 output */
  kPipeStt,
  /** this pipe is used to control the path of meta 2 output */
  kPipeStt2,
  /** this pipe is used to control the path of tuning input */
  kPipeTuning,
  /** this pipe is used to subscribe hardware events */
  kPipeHwEvent,
};

enum PipeSensorIdx {
  kPipe_Sensor_0 = 0,
  kPipe_Sensor_1,
  kPipe_Sensor_2,
  kPipe_Sensor_3,
  kPipe_Sensor_RSVD,
};

enum PipeRawFmt {
  kPipe_PROCESSED_RAW = 0x0000, /*!< Processed raw */
  kPipe_PURE_RAW = 0x0001,      /*!< Pure raw */
  /*for pso*/
  kPipe_BEFORE_LSC = 0x0000, /*!< Before LSC*/
  kPipe_BEFORE_OB = 0x0001,  /*!< Before OB */
};

struct ResultMetadata {
  MRect mCrop_s;   // scaler crop
  MSize mDstSize;  // scaler scaledown size
  MRect mCrop_d;   // damo   crop
  MUINT32 mTransform;
  MUINT32 mMagicNum_hal;
  MUINT32 mMagicNum_tuning;
  MUINT32 mRawType;     // 0 represents processed raw, 1 represents pure raw
  MINT64 mTimeStamp;    // mono-time
  MINT64 mTimeStamp_B;  // boot-time
  MVOID* mPrivateData;  // set to internal static mem space
  MUINT32 mPrivateDataSize;
  MBOOL mHighlightData;
  E_CamIQLevel eIQlv;

  ResultMetadata(MRect rCropRect = MRect(MPoint(0, 0), MSize(0, 0)),
                 MUINT32 rTranform = 0,
                 MUINT32 rNum1 = 0,
                 MUINT32 rNum2 = 0,
                 MUINT32 rRawType = 0,
                 MUINT64 TimeStamp = 0,
                 MVOID* privateData = 0,
                 MUINT32 privateDataSize = 0,
                 MBOOL rHighlightData = 0)
      : mCrop_s(rCropRect),
        mCrop_d(rCropRect),
        mTransform(rTranform),
        mMagicNum_hal(rNum1),
        mMagicNum_tuning(rNum2),
        mRawType(rRawType),
        mTimeStamp(TimeStamp),
        mPrivateData(privateData),
        mPrivateDataSize(privateDataSize),
        mHighlightData(rHighlightData) {
    mDstSize = MSize(0, 0);
    eIQlv = eCamIQ_MAX;
    mTimeStamp_B = 0;
  }
};

struct BufInfo {
  PortID mPortID;  // deque: portID.                enque: PortID
  IImageBuffer*
      mBuffer;  // deque: input buf infor.       enque: output buf infor
  ResultMetadata mMetaData;  // deuqe: deque result.          enque: bypass
  MINT32 mBufIdx;            // deque: bypass.                enque: bypass
  MUINT32 mRawOutFmt;        // deque: bypass.                enque: pure_raw,
                             // processed_raw

  struct {
    MUINT32 mMagicNum_tuning;
    MSize mDstSize;   // w/h of out image which's on dram
    MRect mCropRect;  // crop image in TG coordinate axis(before scaler)
    MUINT32 mSOFidx;  // deque: bypass.           enque: sof idx for camera 3.0
  } FrameBased;
  MUINT32 mSize;
  MUINTPTR mVa;
  MUINTPTR mPa;
  MUINTPTR mPa_offset;
  MUINT32 mStride;  // unit:byte.

  BufInfo(PortID const& rPortID,
          IImageBuffer* buffer,
          MUINT32 size,
          MUINTPTR va,
          MUINTPTR pa)
      : mPortID(rPortID), mBuffer(buffer), mSize(size), mVa(va), mPa(pa) {
    mBufIdx = 0xFFFF;
    mRawOutFmt = kPipe_PROCESSED_RAW;
    FrameBased.mMagicNum_tuning = 0x40000000;
    FrameBased.mDstSize = MSize(0, 0);
    FrameBased.mCropRect = MRect(MPoint(0, 0), MSize(0, 0));
    FrameBased.mSOFidx = 0xFFFF;
    mStride = 0;
  }

  BufInfo(PortID const& rPortID,
          IImageBuffer* buffer,
          MUINT32 size,
          MUINTPTR va,
          MUINTPTR pa,
          MINT32 BufIdx)
      : mPortID(rPortID),
        mBuffer(buffer),
        mBufIdx(BufIdx),
        mSize(size),
        mVa(va),
        mPa(pa) {
    mRawOutFmt = kPipe_PROCESSED_RAW;
    FrameBased.mMagicNum_tuning = 0x40000000;
    FrameBased.mDstSize = MSize(0, 0);
    FrameBased.mCropRect = MRect(MPoint(0, 0), MSize(0, 0));
    FrameBased.mSOFidx = 0xFFFF;
    mStride = 0;
  }

  // ALPS_camera 1.0
  BufInfo(PortID const& rPortID, IImageBuffer* buffer, MINT32 idx = 0xFFFF)
      : mPortID(rPortID), mBuffer(buffer), mBufIdx(idx) {
    mStride = 0;
  }

  // ALPS_camera 3.0
  BufInfo(PortID const& rPortID,
          IImageBuffer* buffer,
          MSize outsize,
          MRect crop,
          MUINT32 magic,
          MINT32 idx = 0xFFFF)
      : mPortID(rPortID), mBuffer(buffer), mBufIdx(idx) {
    mRawOutFmt = kPipe_PROCESSED_RAW;
    FrameBased.mDstSize = outsize;
    FrameBased.mCropRect = crop;
    FrameBased.mMagicNum_tuning = magic;
    FrameBased.mSOFidx = 0xFFFF;
    mVa = 0;
    mPa = 0;
    mPa_offset = 0;
    mSize = 0;
    mStride = 0;
  }
  // ALPS_camera 3.0, iHDR
  BufInfo(PortID const& rPortID,
          IImageBuffer* buffer,
          MSize outsize,
          MRect crop,
          MUINT32 magic,
          MUINT32 sofidx,
          MINT32 idx = 0xFFFF)
      : mPortID(rPortID), mBuffer(buffer), mBufIdx(idx) {
    mRawOutFmt = kPipe_PROCESSED_RAW;
    FrameBased.mDstSize = outsize;
    FrameBased.mCropRect = crop;
    FrameBased.mMagicNum_tuning = magic;
    FrameBased.mSOFidx = sofidx;
    mVa = 0;
    mPa = 0;
    mPa_offset = 0;
    mSize = 0;
    mStride = 0;
  }
  // ALPS_camera 3.0, iHDR, RawOurFormat
  BufInfo(PortID const& rPortID,
          IImageBuffer* buffer,
          MSize outsize,
          MRect crop,
          MUINT32 magic,
          MUINT32 sofidx,
          MUINT32 rawFmt,
          MINT32 idx = 0xFFFF)
      : mPortID(rPortID), mBuffer(buffer), mBufIdx(idx) {
    mRawOutFmt = rawFmt;
    FrameBased.mDstSize = outsize;
    FrameBased.mCropRect = crop;
    FrameBased.mMagicNum_tuning = magic;
    FrameBased.mSOFidx = sofidx;
    mVa = 0;
    mPa = 0;
    mPa_offset = 0;
    mSize = 0;
    mStride = 0;
  }
  BufInfo() {
    mRawOutFmt = kPipe_PROCESSED_RAW;
    FrameBased.mMagicNum_tuning = 0x40000000;
    FrameBased.mDstSize = MSize(0, 0);
    FrameBased.mCropRect = MRect(MPoint(0, 0), MSize(0, 0));
    FrameBased.mSOFidx = 0xFFFF;
    mVa = 0;
    mPa = 0;
    mPa_offset = 0;
    mBuffer = nullptr;
    mBufIdx = 0;
    mSize = 0;
    mStride = 0;
  }
};

struct QPortID {
  std::vector<PortID> mvPortId;
};

struct QBufInfo {
  MVOID* mpCookie;
  MUINT64 mShutterTimeNs;
  std::vector<BufInfo> mvOut;
  QBufInfo() : mpCookie(NULL), mShutterTimeNs(0), mvOut() {}
};

struct PortInfo {
  PortID mPortID;
  int mFmt;
  MSize mDstSize;
  MRect mCropRect;
  int mStride[3];
  MBOOL mPureRaw;
  MBOOL mPureRawPak;
  int mBufPoolSize;
  PortInfo(PortID _mPortID,
           int _mFmt,
           MSize _mDstSize,
           MRect _mCropRect,
           int _mStride0,
           int _mStride1,
           int _mStride2,
           MBOOL _mPureRaw = 0,
           MBOOL _mPureRawPak = 0,
           int _mBufPoolSize = 0)
      : mPortID(_mPortID),
        mFmt(_mFmt),
        mDstSize(_mDstSize),
        mCropRect(_mCropRect),
        mPureRaw(_mPureRaw),
        mPureRawPak(_mPureRawPak),
        mBufPoolSize(_mBufPoolSize) {
    mStride[0] = _mStride0;
    mStride[1] = _mStride1;
    mStride[2] = _mStride2;
  }
};

struct QInitParam {
  MUINT32 mRawType;  // ctrl sensor output test pattern or not
  std::vector<IHalSensor::ConfigParam> mSensorCfg;
  std::vector<PortInfo> mPortInfo;
  void* m_returnCookie;
  MBOOL m_DynamicRawType;  // 1:dynamically switch processed/pure raw
  MBOOL m_bOffBin;         // 1. : force to off frontal binning
  MBOOL m_bN3D;

  E_CAM_PipelineBitDepth_SEL
      m_pipelinebitdepth;  // Choose how many bits will be output after TG.
  MBOOL
  m_DynamicTwin;  // 1 for turn on dynamic twin, 0 for turn off dynamic twin
  E_CamIQLevel m_IQlv;  // after ISP5.0 use IQlv instead of OffBin
  MUINT mSensorFormatOrder;
  //
  QInitParam() = default;
  QInitParam(MUINT32 rRawType,
             std::vector<IHalSensor::ConfigParam> const& rSensorCfg,
             std::vector<PortInfo> const& rPortInfo)
      : mRawType(rRawType), mSensorCfg(rSensorCfg), mPortInfo(rPortInfo) {
    m_returnCookie = NULL;
    m_DynamicRawType = MTRUE;
    m_bOffBin = MFALSE;
    m_bN3D = MFALSE;
    m_pipelinebitdepth = CAM_Pipeline_12BITS;
    m_DynamicTwin = MFALSE;
    m_IQlv = eCamIQ_MAX;
    mSensorFormatOrder = SENSOR_FORMAT_ORDER_NONE;
  }
  //
  QInitParam(MUINT32 rRawType,
             std::vector<IHalSensor::ConfigParam> const& rSensorCfg,
             std::vector<PortInfo> const& rPortInfo,
             MBOOL dynamicSwt)
      : mRawType(rRawType),
        mSensorCfg(rSensorCfg),
        mPortInfo(rPortInfo),
        m_DynamicRawType(dynamicSwt) {
    m_returnCookie = NULL;
    m_bOffBin = MFALSE;
    m_bN3D = MFALSE;
    m_pipelinebitdepth = CAM_Pipeline_12BITS;
    m_DynamicTwin = MFALSE;
    m_IQlv = eCamIQ_MAX;
    mSensorFormatOrder = SENSOR_FORMAT_ORDER_NONE;
  }
  QInitParam(MUINT32 rRawType,
             std::vector<IHalSensor::ConfigParam> const& rSensorCfg,
             std::vector<PortInfo> const& rPortInfo,
             MBOOL dynamicSwt,
             MBOOL bN3d)
      : mRawType(rRawType),
        mSensorCfg(rSensorCfg),
        mPortInfo(rPortInfo),
        m_DynamicRawType(dynamicSwt),
        m_bN3D(bN3d) {
    m_returnCookie = NULL;
    m_bOffBin = MFALSE;
    m_pipelinebitdepth = CAM_Pipeline_12BITS;
    m_DynamicTwin = MFALSE;
    m_IQlv = eCamIQ_MAX;
    mSensorFormatOrder = SENSOR_FORMAT_ORDER_NONE;
  }
};

/* check if the tuning has been enabled from the given PipeTag t */
inline bool is_enable_tuning(PipeTag t) {
  return !!(t & kPipeTag_Tuning);
}

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_CAMIO_V4L2IHALCAMIO_H_
