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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_FLICKER_TABLE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_FLICKER_TABLE_H_

typedef struct {
  MINT32* pPreviewTable1;
  MINT32* pPreviewTable2;
  MINT32 previewTableSize;
  MINT32* pVideoTable1;
  MINT32* pVideoTable2;
  MINT32 videoTableSize;
  MINT32* pCaptureTable1;
  MINT32* pCaptureTable2;
  MINT32 captureTableSize;
  MINT32* pZsdTable1;
  MINT32* pZsdTable2;
  MINT32 zsdTableSize;
} FlckerTable, *PFlckerTable;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_FLICKER_TABLE_H_
