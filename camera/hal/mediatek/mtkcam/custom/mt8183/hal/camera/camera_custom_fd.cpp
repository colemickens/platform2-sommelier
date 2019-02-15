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

#include <camera_custom_fd.h>

void get_fd_CustomizeData(FD_Customize_PARA* FDDataOut) {
  FDDataOut->FDThreadNum = 1;
  FDDataOut->FDThreshold = 276;
  FDDataOut->MajorFaceDecision = 1;
  FDDataOut->OTRatio = 800;
  FDDataOut->SmoothLevel = 8;
  FDDataOut->Momentum = 0;
  FDDataOut->MaxTrackCount = 11;
  FDDataOut->FDSkipStep = 2;
  FDDataOut->FDRectify = 10;
  FDDataOut->FDRefresh = 3;
  FDDataOut->SDThreshold = 69;
  FDDataOut->SDMainFaceMust = 1;
  FDDataOut->SDMaxSmileNum = 3;
  FDDataOut->GSensor = 1;
  FDDataOut->FDModel = 1;
  FDDataOut->OTFlow =
      1;  // 0:Original Flow (FDRefresh:60)  , 1:New Flow (FDRefresh:3)
  FDDataOut->UseCustomScale = 1;
  FDDataOut->FDSizeRatio = 0.0;  // float:0~1
  FDDataOut->SkipPartialFD = 0;
  FDDataOut->SkipAllFD = 0;
}
