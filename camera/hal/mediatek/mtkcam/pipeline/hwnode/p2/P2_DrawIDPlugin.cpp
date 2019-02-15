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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_DrawIDPlugin.h"
#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG DrawIDPlugin
#define P2_TRACE TRACE_P2_DRAWID_PLUGIN
#include "P2_LogHeader.h"
#include <memory>

namespace P2 {

P2DrawIDPlugin::P2DrawIDPlugin() {
  TRACE_FUNC_ENTER();
  mDrawIDUtil = std::make_shared<DebugDrawID>();
  TRACE_FUNC_EXIT();
}

P2DrawIDPlugin::~P2DrawIDPlugin() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL P2DrawIDPlugin::onPlugin(const P2Img* img) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (mDrawIDUtil && isValid(img) && img->getDir() & IO_DIR_OUT &&
      img->getID() == OUT_YUV) {
    IImageBuffer* ptr = img->getIImageBufferPtr();
    if (ptr) {
      TRACE_FUNC("draw + img(%s)", img->getHumanName());
      mDrawIDUtil->draw(
          img->getMagic3A(), reinterpret_cast<char*>(ptr->getBufVA(0)),
          ptr->getImgSize().w, ptr->getImgSize().h,
          ptr->getBufStridesInBytes(0), ptr->getBufSizeInBytes(0));
      TRACE_FUNC("draw -");
    } else {
      MY_LOGE("invalid ptr=NULL img=%p", img);
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2DrawIDPlugin::isEnabled() const {
  return (mDrawIDUtil != nullptr) ? mDrawIDUtil->needDraw() : MFALSE;
}

}  // namespace P2
