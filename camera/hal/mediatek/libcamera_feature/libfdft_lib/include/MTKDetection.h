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

#ifndef CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBFDFT_LIB_INCLUDE_MTKDETECTION_H_
#define CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBFDFT_LIB_INCLUDE_MTKDETECTION_H_

#include <mtkcam/def/BuiltinTypes.h>

#define SmileDetect (1)

#define FD_MAX_FACE_NUM (15)
#define FD_SCALE_NUM (14)
#define CAMERA_FD_MAX_NO (1024)
#define MAX_FACE_SEL_NUM (CAMERA_FD_MAX_NO + 2)
#define UTL_NUMBER_OF_BINS (12)  // Confidence table size for one feature
#define FD_POSE_NUM (12)         // Total pose number of each degree
#define FD_CASCADE_LAYER_MAX_NUM \
  (1000)  // maximum cascade layer number of 24x24 learning data
#define FACE_SIZE_NUM_MAX \
  (14)  // The max number of face sizes could be detected, for feature scaling
#define UTL_HAAR_PIX_MAX \
  (2)  // 2 for shrink, 8 for all, pixel position numbers for one feature

#define MAX_CROP_NUM (16)  // maximum number of crop patches
#define MAX_CROP_W (256)
#define MAX_AIE_FMAP_SZ (1024)
#define MAX_AIE_ATTR_TYPE (2)

typedef enum DRVFDObject_s {
  DRV_FD_OBJ_NONE = 0,
  DRV_FD_OBJ_SW,
  DRV_FD_OBJ_HW,
  DRV_FD_OBJ_FDFT_SW,
  DRV_FD_OBJ_UNKNOWN = 0xFF,
} DrvFDObject_e;

typedef enum {
  FDVT_IDLE_MODE = 0,
  FDVT_GFD_MODE = 0x01,
  FDVT_LFD_MODE = 0x02,
  FDVT_OT_MODE = 0x04,
  FDVT_SD_MODE = 0x08,
} FDVT_OPERATION_MODE_ENUM;

typedef enum {
  FACEDETECT_TRACKING_REALPOS = 0,
  FACEDETECT_TRACKING_DISPLAY,
} FACEDETECT_TRACKING_RESULT_TYPE_ENUM;

typedef enum {
  //!!! Related with AppFDFT_SW::gSensorDirToPoseTable[5] mapping, take care !!!
  FACEDETECT_GSENSOR_DIRECTION_0 = 0,
  FACEDETECT_GSENSOR_DIRECTION_90,
  FACEDETECT_GSENSOR_DIRECTION_270,
  FACEDETECT_GSENSOR_DIRECTION_180,
  FACEDETECT_GSENSOR_DIRECTION_NO_SENSOR,
} FACEDETECT_GSENSOR_DIRECTION;

typedef enum {
  FACEDETECT_IMG_Y_SINGLE = 0,
  FACEDETECT_IMG_YUYV_SINGLE,
  FACEDETECT_IMG_Y_SCALES,
  FACEDETECT_IMG_RGB565,
} FACEDETECT_IMG_TYPE;

struct result {
  unsigned int id;
  bool af_face_indicator;  // face detected flag
  int face_index;          // priority of this face
  int type;                // means this face is GFD, LFD, OT face
  int x0;                  // up-left x pos
  int y0;                  // up-left y pos
  int x1;                  // down-right x pos
  int y1;                  // down-right y pos
  int fcv;                 // confidence value
  int rip_dir;             // in plane rotate direction
  int rop_dir;             // out plane rotate direction(0/1/2/3/4/5 =
                           // ROP00/ROP+50/ROP-50/ROP+90/ROP-90)
  int size_index;          // face size index
  int face_num;            // total face number
  int motion[2];           // face motion against prev frame

  //<<<<<<<<<<<<<<<<<<<< for dump fd result
  int x0real;
  int y0real;
  int x1real;
  int y1real;
  //>>>>>>>>>>>>>>>>>>>> for dump fd result

  // dlfld
  int dl_leyex0;
  int dl_leyey0;
  int dl_leyex1;
  int dl_leyey1;
  int dl_reyex0;
  int dl_reyey0;
  int dl_reyex1;
  int dl_reyey1;
  int dl_nosex;
  int dl_nosey;
  int dl_mouthx0;
  int dl_mouthy0;
  int dl_mouthx1;
  int dl_mouthy1;
  int dl_bbox_flag;
  int rop_from_dlfld;

  // 20160106: add for FaceAlignment
  int leyex0;
  int leyey0;
  int leyex1;
  int leyey1;
  int reyex0;
  int reyey0;
  int reyex1;
  int reyey1;
  int nosex;
  int nosey;
  int mouthx0;
  int mouthy0;
  int mouthx1;
  int mouthy1;
  int fa_cv;
  int fld_rip;
  int fld_rop;
  int YUVsts[5];

  int leyeux;
  int leyeuy;
  int leyedx;
  int leyedy;
  int reyeux;
  int reyeuy;
  int reyedx;
  int reyedy;
  /* fld gender classifier */
  unsigned char fld_oGenderLabel;
  unsigned char fld_GenderLabel;
  int fld_oGenderScore;
  int fld_GenderInfo;

  unsigned char GenderLabel;
  unsigned char GenderCV;
  unsigned char RaceLabel;
  unsigned char RaceCV[4];
};

typedef struct {
  MINT16 wLeft;
  MINT16 wTop;
  MINT16 wWidth;
  MINT16 wHeight;
} FACEDETECT_RECT;

struct MTKFDFTInitInfo {
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
  MUINT32 WorkingBufSize;  // working buffer size
  MUINT8* WorkingBufAddr;  // working buffer
  MUINT32* FDImageWidthArray;
  MUINT32* FDImageHeightArray;
  MUINT32* PThreadAttr;
  pthread_mutex_t* gender_status_mutexAddr;
#ifdef _SIM_PC_
  int CoreIdx;
  void (*LockOtBufferFunc)(int);
  void (*UnlockOtBufferFunc)(int);
#else
  void (*LockOtBufferFunc)(void*);
  void (*UnlockOtBufferFunc)(void*);
  void* lockAgent;
#endif

  MTKFDFTInitInfo() { SilentModeFDSkipNum = 2; }
};

/*******************************************************************************
 *
 ******************************************************************************/
struct FdOptions {
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
  MUINT8* ImageScaleBuffer;
  MUINT8* ImageBufferRGB565;
  MUINT8* ImageBufferSrcVirtual;
  MUINT8* ImageBufferPhyPlane1;
  MUINT8* ImageBufferPhyPlane2;
  MUINT8* ImageBufferPhyPlane3;
};

// svm related
typedef enum {
  GFD_RST_TYPE = 0,
  LFD_RST_TYPE,
  COLOR_COMP_RST_TYPE,
  OT_RST_TYPE
} face_result_enum;

typedef struct {
  MINT16 pix_data_x[UTL_HAAR_PIX_MAX];
  MINT16 pix_data_y[UTL_HAAR_PIX_MAX];
} UTL_PIX_POSITION_STRUCT, *P_UTL_PIX_POSITION_STRUCT;

typedef struct {
  MINT8 bin_value_table[UTL_NUMBER_OF_BINS];
  MINT8 threshold;
  MINT8 threshold2;
  MUINT8 feature_range;
  MINT8 feature_value_8bit_min;
} UTL_CASCADED_CLASSIFIERS_STRUCT, *P_UTL_CASCADED_CLASSIFIERS_STRUCT;

typedef struct {
  UTL_CASCADED_CLASSIFIERS_STRUCT
  cascaded_classifiers[FD_CASCADE_LAYER_MAX_NUM +
                       FD_CASCADE_LAYER_MAX_NUM];  // 0 degree & 30 degree
  MINT8 pattern_index[(FD_POSE_NUM / 3) * FD_CASCADE_LAYER_MAX_NUM +
                      ((FD_POSE_NUM * 2) / 3) * FD_CASCADE_LAYER_MAX_NUM];
  UTL_PIX_POSITION_STRUCT
  scaled_posed_pix_position[FACE_SIZE_NUM_MAX * (FD_CASCADE_LAYER_MAX_NUM +
                                                 FD_CASCADE_LAYER_MAX_NUM)];
} fd_data_struct;  // learning_data_24x24_struct_fd

struct fd_ensemble_svm_model_int {
  const MINT32* beta;
  const MINT32* omega;
  const MUINT8* fpx;
  const MUINT8* fpy;
  const MINT8* parities;
  const MINT32* ths;
  const MUINT32* alphas;
  MUINT8 n;
  MUINT16 d;
  MUINT8 hog_cell_size;
};
//

typedef struct {
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

  ///////////////////////////////////////////////////////////////
  kal_bool rotation_search;  // Phone rotation request flag
  MUINT8 skip_pattern;       // Skip pixel counter used in GFD
  MUINT8
  top_skip_pattern_idx;  // Skip pixel counter used in GFD in top direction
  MUINT8
  left_skip_pattern_idx;  // Skip pixel counter used in GFD in top direction
  MUINT8 right_skip_pattern_idx;   // Skip pixel counter used in GFD in top
                                   // direction
  MUINT8 bottom_skip_pattern_idx;  // Skip pixel counter used in GFD in top
                                   // direction
  kal_bool is_first_frame;         // First frame check
  MUINT8 dir_cycle_count;          // Detection cycle count
  MUINT8
  color_compensate_face_number;  // Face number tracked by color compensate
  //  kal_bool  af_face_indicator[MAX_FACE_SEL_NUM];  // Specify which face has
  //  highest priority for focus
  kal_bool box_display_position_update_flag
      [MAX_FACE_SEL_NUM];  // Record if need to update face rectnage
  kal_bool
      execute_skin_color_track[MAX_FACE_SEL_NUM];   // Record if need to execute
                                                    // color compensate
  MUINT8 non_lfd_tracking_count[MAX_FACE_SEL_NUM];  // Record the non-lfd (color
                                                    // comp.) tracking count
  MUINT8 avg_r_value[MAX_FACE_SEL_NUM]; /* Record color statistics      */
  MUINT8 avg_g_value[MAX_FACE_SEL_NUM]; /* for color compensate */
  MUINT8 avg_b_value[MAX_FACE_SEL_NUM]; /* for each face bin        */
  float avg_div_rg[MAX_FACE_SEL_NUM];
  MUINT32
  continuous_lfd_tracking_count[MAX_FACE_SEL_NUM];  // Record continuous lfd
                                                    // tracking count
  MINT32
  face_display_pos_x0[MAX_FACE_SEL_NUM];  // Position of the faces to disply
  MINT32 face_display_pos_y0[MAX_FACE_SEL_NUM];
  MINT32 face_display_pos_x1[MAX_FACE_SEL_NUM];
  MINT32 face_display_pos_y1[MAX_FACE_SEL_NUM];

  // for FDVersion 50 -- only FD_MAX_FACE_NUM
  MINT32 fld_leye_x0[FD_MAX_FACE_NUM];
  MINT32 fld_leye_y0[FD_MAX_FACE_NUM];
  MINT32 fld_leye_x1[FD_MAX_FACE_NUM];
  MINT32 fld_leye_y1[FD_MAX_FACE_NUM];
  MINT32 fld_reye_x0[FD_MAX_FACE_NUM];
  MINT32 fld_reye_y0[FD_MAX_FACE_NUM];
  MINT32 fld_reye_x1[FD_MAX_FACE_NUM];
  MINT32 fld_reye_y1[FD_MAX_FACE_NUM];
  MINT32 fld_nose_x[FD_MAX_FACE_NUM];
  MINT32 fld_nose_y[FD_MAX_FACE_NUM];
  MINT32 fld_mouth_x0[FD_MAX_FACE_NUM];
  MINT32 fld_mouth_y0[FD_MAX_FACE_NUM];
  MINT32 fld_mouth_x1[FD_MAX_FACE_NUM];
  MINT32 fld_mouth_y1[FD_MAX_FACE_NUM];

  MUINT8 face_lum[MAX_FACE_SEL_NUM];  // Face luminance for Face AE

  // HAL Add
  // FACE_DIR_OFST_0, FACE_DIR_OFST_270,
  // FACE_DIR_OFST_180, FACE_DIR_OFST_90
  MUINT8 direction_offset;
  MUINT8 fd_level;
  MUINT16 svm_candidate_num;

  MUINT32* integral_img;    // Pointer to integral Image buffer
  MUINT16* prz_buffer_ptr;  // Pointer to a cacheable buffer copied from prz
                            // output buffer
  MUINT8* srcbuffer_phyical_addr_plane1;
  MUINT8* srcbuffer_phyical_addr_plane2;
  MUINT8* srcbuffer_phyical_addr_plane3;

  const MUINT32* detect_face_size_lut;  // Pointer to face size table
  const fd_data_struct*
      learned_cascaded_classifiers;  // Pointer to 24x24 learning data
  const fd_ensemble_svm_model_int* fd_svm_model_00_;  // Pointer to svm 00 data
  const fd_ensemble_svm_model_int* fd_svm_model_30_;  // Pointer to svm 30 data
  MUINT8* img_array[FD_SCALE_NUM];
  MUINT32* integral_img_array[FD_SCALE_NUM];
  MUINT8* srcbuffer_phyical_addr;
} fd_cal_struct;

typedef struct {
  MUINT16 new_face_number;  // Face number detected by GFD
  MUINT8 face_feature_set_index[MAX_FACE_SEL_NUM];  // Record the reliability
                                                    // value for each face bin
  MUINT8 rip_dir[MAX_FACE_SEL_NUM];                 // keep rip_dir
  MUINT8 rop_dir[MAX_FACE_SEL_NUM];                 // keep rop_dir
  MINT32
  face_candi_pos_x0[MAX_FACE_SEL_NUM];  // Position of the faces candidates
  MINT32 face_candi_pos_y0[MAX_FACE_SEL_NUM];
  MINT32 face_candi_pos_x1[MAX_FACE_SEL_NUM];
  MINT32 face_candi_pos_y1[MAX_FACE_SEL_NUM];
  MUINT32 face_reliabiliy_value[MAX_FACE_SEL_NUM];
} fd_drv_output;

/*******************************************************************************
 *
 ******************************************************************************/
class MTKDetection {
 public:
  static MTKDetection* createInstance(DrvFDObject_e eobject);
  virtual void destroyInstance() = 0;

  virtual ~MTKDetection() {}
  virtual void FDVTInit(MUINT32* fd_tuning_data);
  virtual void FDVTInit(MTKFDFTInitInfo* init_data);
  virtual void FDVTMain(FdOptions* options);
  virtual fd_cal_struct* FDGetCalData(void);
  virtual void FDVTMainPhase2();
  virtual void FDVTMainFastPhase(int* gtype_with_gamma);
  virtual void FDVTMainCropPhase(unsigned char todo_list[][MAX_CROP_NUM],
                                 unsigned char buf_status[][MAX_CROP_NUM],
                                 unsigned char** workbuf,
                                 int* patchszarr,
                                 int* pNGender);
  virtual void FDVTMainPostPhase();
  virtual void FDVTMainJoinPhase(unsigned char* gender_buf_status,
                                 MINT16 feature_map_in_hal[][MAX_AIE_FMAP_SZ],
                                 int idx_attr);
  virtual void FDVTReset(void);
  virtual MUINT32 FDVTGetResultSize(void);
  virtual MUINT8 FDVTGetResult(
      MUINT8* FD_result_Adr, FACEDETECT_TRACKING_RESULT_TYPE_ENUM result_type);
  virtual void FDVTGetICSResult(MUINT8* a_FD_ICS_Result,
                                MUINT8* a_FD_Results,
                                MUINT32 Width,
                                MUINT32 Height,
                                MUINT32 LCM,
                                MUINT32 Sensor,
                                MUINT32 Camera_TYPE,
                                MUINT32 Draw_TYPE);
  virtual void FDVTGetFDInfo(MUINT32 FD_Info_Result);
// virtual void FDVTDrawFaceRect(MUINT32 image_buffer_address,MUINT32
// Width,MUINT32 Height,MUINT32 OffsetW,MUINT32 OffsetH,MUINT8 orientation);
#ifdef SmileDetect
  virtual void FDVTSDDrawFaceRect(MUINT32 image_buffer_address,
                                  MUINT32 Width,
                                  MUINT32 Height,
                                  MUINT32 OffsetW,
                                  MUINT32 OffsetH,
                                  MUINT8 orientation);
  virtual MUINT8 FDVTGetSDResult(MUINT32 FD_result_Adr);
  virtual void FDVTGetMode(FDVT_OPERATION_MODE_ENUM* mode);
#endif

 private:
};

class AppFDTmp : public MTKDetection {
 public:
  //
  static MTKDetection* getInstance();
  virtual void destroyInstance();
  //
  AppFDTmp() {}
  virtual ~AppFDTmp() {}
};

#endif  // CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBFDFT_LIB_INCLUDE_MTKDETECTION_H_
