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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_ScanlinePlugin.h"
#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG ScanlinePlugin
#define P2_TRACE TRACE_P2_SCANLINE_PLUGIN
#include "P2_LogHeader.h"
#include <memory>

namespace P2 {

P2ScanlinePlugin::P2ScanlinePlugin() : mScanline(nullptr) {
  TRACE_FUNC_ENTER();
  mEnabled = property_get_int32("vendor.debug.mtkcam.p2.scanline", 0);
  mScanline = std::make_unique<DebugScanLineImp>();
  TRACE_FUNC_EXIT();
}

P2ScanlinePlugin::~P2ScanlinePlugin() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL P2ScanlinePlugin::onPlugin(const P2Img* img) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (mScanline && isValid(img) && img->getDir() & IO_DIR_OUT) {
    IImageBuffer* ptr = img->getIImageBufferPtr();
    if (ptr) {
      if ((MUINT32)ptr->getImgSize().w == ptr->getBufStridesInBytes(0)) {
        mScanline->drawScanLine(ptr->getImgSize().w, ptr->getImgSize().h,
                                reinterpret_cast<void*>(ptr->getBufVA(0)),
                                ptr->getBufSizeInBytes(0),
                                ptr->getBufStridesInBytes(0));
        ret = MTRUE;
      }
    } else {
      MY_LOGE("invalid ptr=NULL img=%p", img);
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2ScanlinePlugin::isEnabled() const {
  return mEnabled;
}

}  // namespace P2
