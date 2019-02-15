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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_FDVT_4_0_CAM_FDVT_V4L2_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_FDVT_4_0_CAM_FDVT_V4L2_H_

#include <errno.h>
#include <fcntl.h>
#include <mtkcam/def/common.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/media.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>

#include <libcamera_feature/libfdft_lib/include/MTKDetection.h>

#define FRAME_NUM_WITHOUT_FACE_TO_DO_ROTATION_SEARCH 60
#define FRAME_DETECT_DIVISION 1
#define GFD_BOUNDARY_OFF_RATIO 0

#define SUCCEEDED(Status) ((MRESULT)(Status) >= 0)
#define FAILED(Status) ((MRESULT)(Status) < 0)

#define MODULE_MTK_Detection (0)  // Temp value

#define FD_ARRAY_SCALE_NUM \
  (FD_SCALE_NUM + 1)  // FACE_SIZE_NUM_MAX + 1, first scale for input image W/H

#define LEARNDATA_NUM 18
#define EXTRA_LEARNDATA_NUM 18
#define FDVT_PARA_NUM 256
#define FDVT_BUFF_NUM 1024

#define FD_RESULT_MAX_SIZE (1024 * 16 + 16)  // 1024 faces, 16 bytes/face
#define RS_BUFFER_MAX_SIZE (1144394 * 2)

#define REG_RMAP 0x05230401      // FD3.5+
#define REG_RMAP_LFD 0x05230400  // FD3.5+ LFD
#define MATCH_NAME_STR_SIZE_MAX 32

// Detection error code
#define S_Detection_OK 0x0000
#define E_Detection_NEED_OVER_WRITE 0x0001
#define E_Detection_NULL_OBJECT 0x0002
#define E_Detection_WRONG_STATE 0x0003
#define E_Detection_WRONG_CMD_ID 0x0004
#define E_Detection_WRONG_CMD_PARAM 0x0005
#define E_Detection_Driver_Fail 0x0010

#define FD_POSE_OFFEST 1
#define FD_POSE_1 0
#define FD_POSE_2 1
#define FD_POSE_3 2
#define FD_POSE_4 3

const uint16_t pose[] = {FD_POSE_1, FD_POSE_1, FD_POSE_1, FD_POSE_2,
                         FD_POSE_3, FD_POSE_2, FD_POSE_3, FD_POSE_2,
                         FD_POSE_3, FD_POSE_4, FD_POSE_4, FD_POSE_4};

struct fd_face_result {
  __u64 face_idx : 12, type : 1, x0 : 10, y0 : 10, x1 : 10, y1 : 10, fcv1 : 11;
  __u64 fcv2 : 7, rip_dir : 4, rop_dir : 3, det_size : 5;
};

struct fd_user_output {
  struct fd_face_result face[MAX_FACE_SEL_NUM];
  __u16 face_number;
};

typedef enum {
  FDVT_MODE_IDLE = 0,
  FDVT_MODE_GFD = 0x01,
  FDVT_MODE_LFD = 0x02,
  FDVT_MODE_OT = 0x04,
  FDVT_MODE_SD = 0x08,
} FDVT_OPERATION_MODE;

/* In FD HW, uses Little Endian of 32 bits, but in SW, uses byte address, so the
 * order of byte should be opposite in HW: SW[YUYV] = HW [VYUY]  */
typedef enum {
  FMT_YUYV = 5,  // SW YUYV = HW VYUY
  FMT_YVYU = 4,  // SW YVYU = HW UYVY
  FMT_UYVY = 3,  // SW UYVU = HW YVYU
  FMT_VYUY = 2,  // SW VYUY = HW YUYV
} INPUT_FORMAT;

struct FdDrv_input_struct {
  MUINT8 fd_mode;
  MUINT64* source_img_address;
  MUINT64* source_img_address_UV;
  MUINT16 source_img_width[FD_ARRAY_SCALE_NUM];
  MUINT16 source_img_height[FD_ARRAY_SCALE_NUM];
  MUINT8 RIP_feature;
  MUINT8 GFD_skip;
  MUINT8 GFD_skip_V;
  MUINT8 feature_threshold;
  MUINT8 source_img_fmt;
  bool scale_from_original;
  bool scale_manual_mode = 0;
  MUINT8 scale_num_from_user = 0;  // Only work when scale_manual_mode = 1
  bool dynamic_change_model[18] = {0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0};
  int memFd;
};

typedef struct {
  // Search range
  MINT32 x0;  //   9 bit
  MINT32 y0;  //   8 bit
  MINT32 x1;  //   9 bit
  MINT32 y1;  //   8 bit

  // Direction information
  MUINT64 pose;  //  60 bit (0-11: ROP00, 12-23: ROP+50, 24-35: ROP-50, 36-47:
                 //  ROP+90, 48-59: ROP-90)
} GFD_Info_Struct;

typedef struct {
  MUINT32* integral_img;    // Pointer to integral Image buffer
  MUINT16* prz_buffer_ptr;  // Pointer to a cacheable buffer copied from prz
                            // output buffer
  MUINT8* srcbuffer_phyical_addr;

  const MUINT32* detect_face_size_lut;  // Pointer to face size table

  MUINT8
  feature_select_sequence_index;    // Current feature select seq. index for
                                    // g_direction_feature_sequence table
  MUINT8 current_fd_detect_column;  // Current frame detect division index
  MUINT8 current_direction;  // Current phone direction (1: H(0), 2: CR(-90), 3:
                             // CCR(90), 4: INV(-180))
  MUINT8 current_feature_index;  // Current feature index for learning data
  MUINT8 current_scale;

  MUINT16 new_face_number;  // Face number detected by GFD
  MUINT16 lfd_face_number;  // Face number tracked by LFD

  MUINT8 fd_priority[MAX_FACE_SEL_NUM];  // face priority array, 0:highest
  MBOOL display_flag[MAX_FACE_SEL_NUM];  // Record if need to display for each
                                         // face bin
  MUINT32 face_reliabiliy_value[MAX_FACE_SEL_NUM];  // Record the reliability
                                                    // value for each face bin

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

  MUINT16 img_width_array[FD_ARRAY_SCALE_NUM];
  MUINT16 img_height_array[FD_ARRAY_SCALE_NUM];
  MUINT8* img_array[FD_ARRAY_SCALE_NUM];
  MUINT32* integral_img_array[FD_ARRAY_SCALE_NUM];

  MUINT8 scale_frame_division[FD_ARRAY_SCALE_NUM];
  MUINT8 scale_detect_column[FD_ARRAY_SCALE_NUM];
} FdDrv_output_struct;

MINT32 VISIBILITY_PUBLIC FDVT_OpenDriverWithUserCount(MUINT32 learning_type);
MINT32 VISIBILITY_PUBLIC FDVT_CloseDriverWithUserCount();
MINT32 VISIBILITY_PUBLIC FDVT_GetModelVersion();
void FDVT_RIPindexFromHWtoFW(fd_user_output* FD_Result);
void VISIBILITY_PUBLIC FDVT_Enque(FdDrv_input_struct* FdDrv_input);
void VISIBILITY_PUBLIC FDVT_Deque(FdDrv_output_struct* FdDrv_output);

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_FDVT_4_0_CAM_FDVT_V4L2_H_
