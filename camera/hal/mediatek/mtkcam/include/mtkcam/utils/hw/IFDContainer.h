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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IFDCONTAINER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IFDCONTAINER_H_

#include <cstdint>  // int64_t, intptr_t
#include <memory>
#include <vector>  // std::vector

#include <mtkcam/def/common.h>  // MBOOL
#include <faces.h>  // MtkCameraFaceMetadata, MtkCameraFace, MtkFaceInfo

using std::vector;

struct MTKFDContainerInfo {
  MtkCameraFaceMetadata facedata;
  MtkCameraFace faces[15];
  MtkFaceInfo posInfo[15];
  MTKFDContainerInfo() {
    memset(&facedata, 0, sizeof(MtkCameraFaceMetadata));
    memset(&faces[0], 0, 15 * sizeof(MtkCameraFace));
    memset(&posInfo[0], 0, 15 * sizeof(MtkFaceInfo));
    facedata.faces = faces;
    facedata.posInfo = posInfo;
  }
  ~MTKFDContainerInfo() {}
};
#define FD_DATATYPE MTKFDContainerInfo

namespace NSCam {
class VISIBILITY_PUBLIC IFDContainer {
  /* enums */
 public:
  enum eFDContainer_Opt {
    eFDContainer_Opt_Read = 0x1,
    eFDContainer_Opt_Write = 0x1 << 1,
    eFDContainer_Opt_RW = eFDContainer_Opt_Read | eFDContainer_Opt_Write,
  };

  /* interfaces */
 public:
  /**
   *  For eFDContainer_Opt_Read
   *  To get all avaliable fd info
   *
   *  notice:             the mrmory of fd info is managed by IFDContainer
   *                      others please don't delete it
   */
  virtual vector<FD_DATATYPE*> queryLock(void) = 0;

  /**
   *  For eFDContainer_Opt_Read
   *  To get the fd info in range [ts_start, ts_end]
   *  @param ts_start     timestamp from
   *  @param ts_end       timestamp until
   *
   *  notice:             the mrmory of fd info is managed by IFDContainer
   *                      others please don't delete it
   */
  virtual vector<FD_DATATYPE*> queryLock(const int64_t& ts_start,
                                         const int64_t& ts_end) = 0;

  /**
   *  For eFDContainer_Opt_Read
   *  To get the fd info in the giving set
   *  @param vecTss       a set of timestamps
   *
   *  notice:             the mrmory of fd info is managed by IFDContainer
   *                      others please don't delete it
   */
  virtual vector<FD_DATATYPE*> queryLock(const vector<int64_t>& vecTss) = 0;

  /**
   *  For eFDContainer_Opt_Read
   *  To unregister the usage of a set of fd infos
   *  @param vecInfos     a set of fd infos get from queryLock
   *
   *  notice:             the mrmory of fd info is managed by IFDContainer
   *                      others please don't delete it
   */
  virtual MBOOL queryUnlock(const vector<FD_DATATYPE*>& vecInfos) = 0;

  /**
   *  For eFDContainer_Opt_Write
   *  To get the fd info for edit and assign the key as input timestamp
   *  @param timestamp    the unique key for query fd info
   *
   *  notice:             the mrmory of fd info is managed by IFDContainer
   *                      others please don't delete it
   */
  virtual FD_DATATYPE* editLock(int64_t timestamp) = 0;

  /**
   *  For eFDContainer_Opt_Write
   *  To publish the fd info editing
   *  @param info         the fd info get from editLock
   *
   *  notice:             the mrmory of fd info is managed by IFDContainer
   *                      others please don't delete it
   */
  virtual MBOOL editUnlock(FD_DATATYPE* info) = 0;

  /**
   *  To dump all fd infos
   */
  virtual void dumpInfo(void) = 0;

 public:
  static std::shared_ptr<IFDContainer> createInstance(char const* userId,
                                                      eFDContainer_Opt opt);

  ~IFDContainer() {}
};
}; /* namespace NSCam */

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IFDCONTAINER_H_
