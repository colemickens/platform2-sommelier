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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_LMVInfo.h"
#define P2_CLASS_TAG LMVInfo
#define P2_TRACE TRACE_LMV_INFO
#include "P2_LogHeader.h"

namespace P2 {

LMVInfo extractLMVInfo(const ILog& log, const IMetadata* halMeta) {
  TRACE_S_FUNC_ENTER(log);

  LMVInfo lmvInfo;
  IMetadata::IEntry entry;
  IMetadata::IEntry validityEntry;

  if (halMeta == NULL) {
    MY_LOGW("invalid metadata = NULL");
  } else {
    entry = halMeta->entryFor(MTK_EIS_REGION);
    if (entry.count() <= LMV_REGION_INDEX_HEIGHT && log.getLogLevel() >= 1) {
      MY_LOGW("meta=%p size=%d no MTK_EIS_REGION count=%d", halMeta,
              halMeta->count(), entry.count());
    }
    validityEntry = halMeta->entryFor(MTK_LMV_VALIDITY);
  }

  auto t32 = Type2Type<MINT32>();

  if (validityEntry.count() > 0) {
    lmvInfo.is_valid = (validityEntry.itemAt(0, t32) == 1) ? MTRUE : MFALSE;
  }

  if (entry.count() > LMV_REGION_INDEX_HEIGHT) {
    lmvInfo.x_int = entry.itemAt(LMV_REGION_INDEX_XINT, t32);
    lmvInfo.x_float = entry.itemAt(LMV_REGION_INDEX_XFLOAT, t32);
    lmvInfo.y_int = entry.itemAt(LMV_REGION_INDEX_YINT, t32);
    lmvInfo.y_float = entry.itemAt(LMV_REGION_INDEX_YFLOAT, t32);
    lmvInfo.s.w = entry.itemAt(LMV_REGION_INDEX_WIDTH, t32);
    lmvInfo.s.h = entry.itemAt(LMV_REGION_INDEX_HEIGHT, t32);
  }
  if (entry.count() > LMV_REGION_INDEX_ISFROMRZ) {
    MINT32 xmv = entry.itemAt(LMV_REGION_INDEX_MV2CENTERX, t32);
    MINT32 ymv = entry.itemAt(LMV_REGION_INDEX_MV2CENTERY, t32);
    lmvInfo.is_from_zzr = entry.itemAt(LMV_REGION_INDEX_ISFROMRZ, t32);
    MBOOL xmv_neg = xmv >> 31;
    MBOOL ymv_neg = ymv >> 31;
    if (xmv_neg) {
      xmv = ~xmv + 1;
    }
    if (ymv_neg) {
      ymv = ~ymv + 1;
    }
    lmvInfo.x_mv_int = (xmv & (~0xFF)) >> 8;
    lmvInfo.x_mv_float = (xmv & (0xFF)) << 31;
    if (xmv_neg) {
      lmvInfo.x_mv_int = ~lmvInfo.x_mv_int + 1;
    }
    if (xmv_neg) {
      lmvInfo.x_mv_float = ~lmvInfo.x_mv_float + 1;
    }
    lmvInfo.y_mv_int = (ymv & (~0xFF)) >> 8;
    lmvInfo.y_mv_float = (ymv & (0xFF)) << 31;
    if (ymv_neg) {
      lmvInfo.y_mv_int = ~lmvInfo.y_mv_int + 1;
    }
    if (ymv_neg) {
      lmvInfo.y_mv_float = ~lmvInfo.y_mv_float + 1;
    }
  }
  if (entry.count() > LMV_REGION_INDEX_GMVY) {
    lmvInfo.gmvX = entry.itemAt(LMV_REGION_INDEX_GMVX, t32);
    lmvInfo.gmvY = entry.itemAt(LMV_REGION_INDEX_GMVY, t32);
  }
  if (entry.count() > LMV_REGION_INDEX_LWTS) {
    lmvInfo.confX = entry.itemAt(LMV_REGION_INDEX_CONFX, t32);
    lmvInfo.confY = entry.itemAt(LMV_REGION_INDEX_CONFY, t32);
    lmvInfo.expTime = entry.itemAt(LMV_REGION_INDEX_EXPTIME, t32);
    lmvInfo.ihwTS = entry.itemAt(LMV_REGION_INDEX_HWTS, t32);
    lmvInfo.ilwTS = entry.itemAt(LMV_REGION_INDEX_LWTS, t32);
    lmvInfo.ts = ((MINT64)(lmvInfo.ihwTS & 0xFFFFFFFF)) << 32;
    lmvInfo.ts += (MINT64)(lmvInfo.ilwTS & 0xFFFFFFFF);
  }
  if (entry.count() > LMV_REGION_INDEX_ISFRONTBIN) {
    lmvInfo.isFrontBin =
        (entry.itemAt(LMV_REGION_INDEX_ISFRONTBIN, t32) == 1) ? MTRUE : MFALSE;
  }
  if (entry.count() > LMV_REGION_INDEX_MAX_GMV) {
    lmvInfo.gmvMax = entry.itemAt(LMV_REGION_INDEX_MAX_GMV, t32);
  }

  MY_LOGD(
      "is_valid(%d),x_int(%d),x_float(%d),y_int(%d),y_float(%d),s(%dx%d),"
      "x_mv_int(%d),x_mv_float(%d),y_mv_int(%d),y_mv_float(%d),is_from_zzr(%d),"
      "gmvX(%d),gmvY(%d),gmvMax(%d),"
      "confX(%d),confY(%d),expTime(%d),ihwTS(%d),ilwTS(%d),ts(%" PRId64
      "),isFrontBin(%d)",
      lmvInfo.is_valid, lmvInfo.x_int, lmvInfo.x_float, lmvInfo.y_int,
      lmvInfo.y_float, lmvInfo.s.w, lmvInfo.s.h, lmvInfo.x_mv_int,
      lmvInfo.x_mv_float, lmvInfo.y_mv_int, lmvInfo.y_mv_float,
      lmvInfo.is_from_zzr, lmvInfo.gmvX, lmvInfo.gmvY, lmvInfo.gmvMax,
      lmvInfo.confX, lmvInfo.confY, lmvInfo.expTime, lmvInfo.ihwTS,
      lmvInfo.ilwTS, lmvInfo.ts, lmvInfo.isFrontBin);

  TRACE_S_FUNC_EXIT(log);
  return lmvInfo;
}

}  // namespace P2
