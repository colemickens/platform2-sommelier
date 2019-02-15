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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_DUMPPLUGIN_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_DUMPPLUGIN_H_

#include "P2_Request.h"
#include <memory>

namespace P2 {

class P2DumpPlugin : virtual public P2ImgPlugin {
 public:
  enum DUMP_IN {
    DUMP_IN_RRZO,
    DUMP_IN_IMGO,
    DUMP_IN_LCSO,
  };

  enum DUMP_OUT {
    DUMP_OUT_DISPLAY,
    DUMP_OUT_RECORD,
    DUMP_OUT_FD,
    DUMP_OUT_PREVIEWCB,
  };

 public:
  P2DumpPlugin();
  virtual ~P2DumpPlugin();
  virtual MBOOL onPlugin(const P2Img* img);
  MBOOL isNddMode() const;
  MBOOL isDebugMode() const;
  P2DumpType needDumpFrame(MINT32 frameNo) const;
  MBOOL needDumpIn(DUMP_IN mask) const;
  MBOOL needDumpOut(DUMP_OUT mask) const;
  MBOOL needDump(const P2Img* img) const;

 private:
  P2DumpType mMode = P2_DUMP_NONE;
  MINT32 mStart = 0;
  MUINT32 mCount = 0;
  MUINT32 mInMask = 0;
  MUINT32 mOutMask = 0;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_DUMPPLUGIN_H_
