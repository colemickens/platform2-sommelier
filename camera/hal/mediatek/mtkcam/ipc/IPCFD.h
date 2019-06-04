/*
 * Copyright (C) 2019 Mediatek Corporation.
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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCFD_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCFD_H_
#include <faces.h>
#include <mtkcam/def/common.h>
#include <MTKDetection.h>

struct FDCommonParams {
  MINT32 m_i4SensorIdx;
  MUINTPTR bufferva;
};

struct FDCreateInfo {
  FDCommonParams common;
  DRVFDObject_s FDMode;
};

struct FDDestoryInfo {
  FDCommonParams common;
};

struct FDIPCInitInfo {
  MUINT32 FDThreadNum;   // default 1, suggest range: 1~2
  MUINT32 FDThreshold;   // default 32, suggest range: 29~35 bigger is harder
  MUINT32 DisLimit;      // default 4, suggest range: 1 ~ 4
  MUINT32 DecreaseStep;  // default 384, suggest range: 0 ~ 384
  MUINT8 ModelVersion;
  MUINT32 FDMINSZ;
  MUINT32 DelayThreshold;  // default 83, under this goes to median reliability,
                           // above goes high
  MUINT32 DelayCount;  // default 2, for median reliability face, should have
                       // deteced in continous frame
  MUINT32 MajorFaceDecision;  // default 1, 0: Size fist.  1: Center first.   2:
                              // Size first perframe. 3: Center first per frame
  MUINT8 OTBndOverlap;        // default 8, suggest range: 6 ~ 9
  MUINT32 OTRatio;            // default 960, suggest range: 640~1200
  MUINT32 OTds;               // default 2, suggest range: 1~2
  MUINT32 OTtype;             // default 1, suggest range: 0~1
  MUINT32 SmoothLevel;        // default 8, suggest range: 0~16
  MUINT32 Momentum;           // default 1, suggest range: 0~3
  MUINT32 MaxTrackCount;      // default 10, suggest range: 0~120
  MUINT8 SilentModeFDSkipNum;  // default 2, suggest range: 2
  MUINT32 FDSkipStep;          // default 4, suggest range: 2~6
  MUINT32 FDRectify;    // default 10000000 means disable and 0 means disable as
                        // well. suggest range: 5~10
  MUINT32 FDRefresh;    // default 70, suggest range: 30~120
  MUINT32 FDBufWidth;   // preview width
  MUINT32 FDBufHeight;  // preview height
  MUINT32 FDSrcWidth;   // source width
  MUINT32 FDSrcHeight;  // source height
  MUINT32 FDTBufWidth;  // preview2 width
  MUINT32 FDTBufHeight;    // preview2 height
  MUINT32 FDMinFaceLevel;  // max face detected level:suggest range 0~13
  MUINT32 FDMaxFaceLevel;  // min face detected level:suggest range 0~13
  MUINT32 FDImageArrayNum;
  FACEDETECT_IMG_TYPE FDImgFmtCH1;
  FACEDETECT_IMG_TYPE FDImgFmtCH2;
  FACEDETECT_IMG_TYPE SDImgFmtCH1;
  FACEDETECT_IMG_TYPE SDImgFmtCH2;
  MUINT32 SDThreshold;     // default 32, suggest range: 29~38 bigger is harder
  MUINT32 SDMainFaceMust;  // default 1 , only capture when main face is smiling
  MUINT32 SDMaxSmileNum;   // default 3, max faces applied smile detection
  MUINT32 GSensor;         // default 1, means g-sensor is on the phone
  MUINT32 GenScaleImageBySw;
  MUINT8 FDModel;
  MUINT8 OTFlow;
  MUINT8 FDCurrent_mode;  // 0:FD, 1:SD, 2:vFB  3:CFB
  MUINT8 FDVersion;
  MUINT8 FLDAttribConfig;  // 0: Turn off attrib, other: Turn on attrib
  MBOOL
  FDManualMode;  // 0: HW GFD use hard-coded scale table, 1:use user defined
                 // scale table(FDImageWidthArray/FDImageHeightArray)
  bool ParallelRGB565Conversion;
  MINT32 LandmarkEnableCnt;
  MUINT8 GenderEnableCnt;
  MUINT8 PoseEnableCnt;
};

struct FDIPCMainParam {
  FDVT_OPERATION_MODE_ENUM fd_state;
  FACEDETECT_GSENSOR_DIRECTION direction;
  int fd_scale_count;  // by frame set how many scales should GFD do detection
  int fd_scale_start_position;  // by frame set which scale should GFD start
                                // detection
  int gfd_fast_mode;
  MBOOL ae_stable;
  MBOOL af_stable;
  MUINT8 LV;
  int curr_gtype;
  int inputPlaneCount;
  bool doPhase2;  // for HAL to decide whether calling HW & phase2 or not.
  bool doGender;
  bool doPose;
  bool P2input;
  FDVT_OPERATION_MODE_ENUM ForceFDMode;
  MUINT16 YUVstsHratio;
  MUINT16 YUVstsWratio;
  MUINT8 startW;
  MUINT8 startH;
  MUINT8 model_version;
};

struct FDIPCCalData {
  MUINT32 inputPlaneCount;
  MUINT8
  feature_select_sequence_index;    // Current feature select seq. index for
                                    // g_direction_feature_sequence table
  MUINT8 current_fd_detect_column;  // Current frame detect division index
  MUINT8 current_direction;  // Current phone direction (1: H(0), 2: CR(-90), 3:
                             // CCR(90), 4: INV(-180))
  MUINT8 current_feature_index;  // Current feature index for learning data
  MUINT8 current_scale;

  MUINT8 new_face_number;  // Face number detected by GFD
  MUINT8 lfd_face_number;  // Face number tracked by LFD

  MUINT8 fd_priority[MAX_FACE_SEL_NUM];     // face priority array, 0:highest
  kal_bool display_flag[MAX_FACE_SEL_NUM];  // Record if need to display for
                                            // each face bin
  MUINT32 face_reliabiliy_value[MAX_FACE_SEL_NUM];  // Record the reliability
                                                    // value for each face bin
  face_result_enum
      result_type[MAX_FACE_SEL_NUM];  // Record the detected result type for
                                      // each face bin (GFD_RST_TYPE,
                                      // LFD_RST_TYPE, COLOR_COMP_RST_TYPE)

  MUINT8 detected_face_size_label[MAX_FACE_SEL_NUM];  // Record face size label
                                                      // for each face bin
  MUINT8 face_feature_set_index[MAX_FACE_SEL_NUM];    // Record used feature set
                                                      // index for each face bin

  // FD 4.0
  MUINT8 rip_dir[MAX_FACE_SEL_NUM];  // keep rip_dir
  MUINT8 rop_dir[MAX_FACE_SEL_NUM];  // keep rop_dir

  MINT32
  face_candi_pos_x0[MAX_FACE_SEL_NUM];  // Position of the faces candidates
  MINT32 face_candi_pos_y0[MAX_FACE_SEL_NUM];
  MINT32 face_candi_pos_x1[MAX_FACE_SEL_NUM];
  MINT32 face_candi_pos_y1[MAX_FACE_SEL_NUM];
  MINT32 face_candi_cv[MAX_FACE_SEL_NUM];
  MINT32 face_candi_model[MAX_FACE_SEL_NUM];

  MUINT16 img_width_array[FD_SCALE_NUM];
  MUINT16 img_height_array[FD_SCALE_NUM];

  MUINT8 scale_frame_division[FD_SCALE_NUM];
  MUINT8 scale_detect_column[FD_SCALE_NUM];

  MBOOL fd_manual_mode;

  int fd_scale_count;
  int fd_scale_start_position;
  MUINT16 fd_img_src_width;
  MUINT16 fd_img_src_height;
};

struct FDIPCResult {
  /**
   * The number of detected faces in the frame.
   */
  MINT32 number_of_faces;

  /**
   * An array of the detected faces. The length is number_of_faces.
   */
  MINT32 faces_type[FD_MAX_FACE_NUM];
  MINT32 motion[FD_MAX_FACE_NUM][2];

  MINT32 ImgWidth;
  MINT32 ImgHeight;

  MINT32 leyex0[FD_MAX_FACE_NUM];
  MINT32 leyey0[FD_MAX_FACE_NUM];
  MINT32 leyex1[FD_MAX_FACE_NUM];
  MINT32 leyey1[FD_MAX_FACE_NUM];
  MINT32 reyex0[FD_MAX_FACE_NUM];
  MINT32 reyey0[FD_MAX_FACE_NUM];
  MINT32 reyex1[FD_MAX_FACE_NUM];
  MINT32 reyey1[FD_MAX_FACE_NUM];
  MINT32 nosex[FD_MAX_FACE_NUM];
  MINT32 nosey[FD_MAX_FACE_NUM];
  MINT32 mouthx0[FD_MAX_FACE_NUM];
  MINT32 mouthy0[FD_MAX_FACE_NUM];
  MINT32 mouthx1[FD_MAX_FACE_NUM];
  MINT32 mouthy1[FD_MAX_FACE_NUM];
  MINT32 leyeux[FD_MAX_FACE_NUM];
  MINT32 leyeuy[FD_MAX_FACE_NUM];
  MINT32 leyedx[FD_MAX_FACE_NUM];
  MINT32 leyedy[FD_MAX_FACE_NUM];
  MINT32 reyeux[FD_MAX_FACE_NUM];
  MINT32 reyeuy[FD_MAX_FACE_NUM];
  MINT32 reyedx[FD_MAX_FACE_NUM];
  MINT32 reyedy[FD_MAX_FACE_NUM];
  MINT32 fa_cv[FD_MAX_FACE_NUM];
  MINT32 fld_rip[FD_MAX_FACE_NUM];
  MINT32 fld_rop[FD_MAX_FACE_NUM];
  MINT32 YUVsts[FD_MAX_FACE_NUM][5];
  MUINT8 fld_GenderLabel[FD_MAX_FACE_NUM];
  MINT32 fld_GenderInfo[FD_MAX_FACE_NUM];
  MUINT8 GenderLabel[FD_MAX_FACE_NUM];
  MUINT8 GenderCV[FD_MAX_FACE_NUM];
  MUINT8 RaceLabel[FD_MAX_FACE_NUM];
  MUINT8 RaceCV[FD_MAX_FACE_NUM][4];
  int64_t timestamp;
  MtkCNNFaceInfo CNNFaces;
};

struct FDInitInfo {
  FDCommonParams common;
  FDIPCInitInfo initInfo;
  MUINT32 FDImageWidthArray[FD_SCALE_NUM];
  MUINT32 FDImageHeightArray[FD_SCALE_NUM];
};

struct FDMainParam {
  FDCommonParams common;
  FDIPCMainParam mainParam;
  int fdBuffer;
};

struct FDMainPhase2 {
  FDCommonParams common;
};

struct FDCalData {
  FDCommonParams common;
  FDIPCCalData calData;
};

struct FDResult {
  FDCommonParams common;
  FDIPCResult result;
  MtkCameraFace faces[FD_MAX_FACE_NUM];
  MtkFaceInfo posInfo[FD_MAX_FACE_NUM];
};

struct FDGetResultInfo {
  FDCommonParams common;
  FDResult FaceResult;
  MUINT32 width;
  MUINT32 height;
};

struct FDReset {
  FDCommonParams common;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCFD_H_
