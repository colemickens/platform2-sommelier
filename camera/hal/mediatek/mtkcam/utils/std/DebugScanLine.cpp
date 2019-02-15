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

#define LOG_TAG "MtkCam/DebugScanLine"

#include <mtkcam/utils/std/DebugScanLine.h>
#include <mtkcam/utils/std/Log.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DebugScanLineImp::DebugScanLineImp() : drawCount(0) {}

DebugScanLineImp::~DebugScanLineImp() {}

void DebugScanLineImp::drawScanLine(
    int imgWidth, int imgHeight, void* virtAddr, int bufSize, int imgStride) {
  char propertyValue[PROPERTY_VALUE_MAX] = {'\0'};
  int i, value = 0, height = 0, width = 0, widthShift = 0, speed = 0,
         lineHeight = 0, displacement = 0, fullscreen;

  if (bufSize < imgStride * imgHeight) {
    MY_LOGD("buffer size < stride*height, doesn't draw scan line");
    return;
  }

  property_get("vendor.debug.cam.scanline.value", propertyValue, "255");
  value = atoi(propertyValue);
  property_get("vendor.debug.cam.scanline.height", propertyValue, "100");
  height = atoi(propertyValue);
  property_get("vendor.debug.cam.scanline.width", propertyValue, "0");
  width = atoi(propertyValue);
  property_get("vendor.debug.cam.scanline.widthShift", propertyValue, "0");
  widthShift = atoi(propertyValue);
  property_get("vendor.debug.cam.scanline.speed", propertyValue, "100");
  speed = atoi(propertyValue);

  lineHeight = imgHeight * height / 800;
  displacement = (speed * drawCount / 5) % (imgHeight - lineHeight);

  MY_LOGD(
      "para:(w,h,s,VA,size)=(%d,%d,%d,%p,%d) prop:(v,h,w,s)=(%d,%d,%d,%d) "
      "line:(h,d)=(%d,%d)",
      imgWidth, imgHeight, imgStride, virtAddr, bufSize, value, width, height,
      speed, lineHeight, displacement);

  property_get("vendor.debug.cam.scanline.fullscreen", propertyValue, "0");
  fullscreen = atoi(propertyValue);

  if (width > 0) {
    if (fullscreen) {
      for (i = 0; i < imgHeight; i++) {
        memset(reinterpret_cast<void*>(reinterpret_cast<char*>(virtAddr) +
                                       widthShift + imgStride * i),
               value, width);
      }
    } else {
      for (i = 0; i < lineHeight; i++) {
        memset(reinterpret_cast<void*>(reinterpret_cast<char*>(virtAddr) +
                                       widthShift + imgStride * displacement +
                                       imgStride * i),
               value, width);
      }
    }
  } else if (fullscreen) {
    memset(reinterpret_cast<void*>(virtAddr), value, bufSize);
  } else {
    memset(reinterpret_cast<void*>(reinterpret_cast<char*>(virtAddr) +
                                   imgStride * displacement),
           value, imgStride * lineHeight);
  }
  drawCount += 1;
  return;
}
