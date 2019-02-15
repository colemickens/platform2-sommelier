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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_DumpPlugin.h"

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG P2DumpPlugin
#define P2_TRACE TRACE_P2_DUMP_PLUGIN
#include "P2_LogHeader.h"

#define STR_DUMP_ENABLE "vendor.debug.p2f.dump.enable"
#define STR_DUMP_MODE "vendor.debug.p2f.dump.mode"
#define STR_DUMP_START "vendor.debug.p2f.dump.start"
#define STR_DUMP_COUNT "vendor.debug.p2f.dump.count"
#define STR_DUMP_IN_MASK "vendor.debug.p2f.dump.in"
#define STR_DUMP_OUT_MASK "vendor.debug.p2f.dump.out"
#define STR_DUMP_NDD_MASK "vendor.debug.camera.preview.dump"

namespace P2 {

P2DumpPlugin::P2DumpPlugin() {
  MBOOL enable = property_get_int32(STR_DUMP_ENABLE, 0);
  if (enable) {
    mMode = (P2DumpType)property_get_int32(STR_DUMP_MODE, P2_DUMP_DEBUG);
    mStart = property_get_int32(STR_DUMP_START, 0);
    mCount = property_get_int32(STR_DUMP_COUNT, 0);
    mInMask = property_get_int32(STR_DUMP_IN_MASK, ~0);
    mOutMask = property_get_int32(STR_DUMP_OUT_MASK, ~0);
  }
  MY_LOGI("mode/start/count(%d/%d/%d) mask: in/out(0x%x/0x%x)", mMode, mStart,
          mCount, mInMask, mOutMask);
}

P2DumpPlugin::~P2DumpPlugin() {}

MBOOL P2DumpPlugin::onPlugin(const P2Img* img) {
  MBOOL ret = MFALSE;
  if (needDump(img)) {
    if (isNddMode()) {
      img->dumpNddBuffer();
    } else {
      img->dumpBuffer();
    }
    ret = MTRUE;
  }
  return ret;
}

MBOOL P2DumpPlugin::isNddMode() const {
  return mMode == P2_DUMP_NDD;
}

MBOOL P2DumpPlugin::isDebugMode() const {
  return mMode == P2_DUMP_DEBUG;
}

P2DumpType P2DumpPlugin::needDumpFrame(MINT32 frameNo) const {
  P2DumpType type = P2_DUMP_NONE;
  if (isNddMode()) {
    if (property_get_int32(STR_DUMP_NDD_MASK, 0) > 0) {
      type = P2_DUMP_NDD;
    }
  } else if (isDebugMode()) {
    if ((mStart < 0) ||
        ((frameNo >= mStart) && ((MUINT32)(frameNo - mStart) < mCount))) {
      type = P2_DUMP_DEBUG;
    }
  }
  return type;
}

MBOOL P2DumpPlugin::needDumpIn(DUMP_IN mask) const {
  return (mInMask & (1 << mask));
}

MBOOL P2DumpPlugin::needDumpOut(DUMP_OUT mask) const {
  return (mOutMask & (1 << mask));
}

MBOOL P2DumpPlugin::needDump(const P2Img* img) const {
  MBOOL ret = MFALSE;
  if (img && img->isValid()) {
    ID_IMG imgID = img->getID();
    switch (imgID) {
      case IN_FULL:
      case IN_FULL_2:
        ret = needDumpIn(DUMP_IN_IMGO);
        break;
      case IN_RESIZED:
      case IN_RESIZED_2:
        ret = needDumpIn(DUMP_IN_RRZO);
        break;
      case IN_LCSO:
      case IN_LCSO_2:
        ret = needDumpIn(DUMP_IN_LCSO);
        break;
      case OUT_YUV: {
        if (img->isDisplay()) {
          ret = needDumpOut(DUMP_OUT_DISPLAY);
        } else if (img->isRecord()) {
          ret = needDumpOut(DUMP_OUT_RECORD);
        } else {
          ret = needDumpOut(DUMP_OUT_PREVIEWCB);
        }
      } break;
      default:
        break;
    }
  }
  return ret;
}

}  // namespace P2
