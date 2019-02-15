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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_PORTMAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_PORTMAP_H_
//
#include <mtkcam/drv/def/ispio_port_index.h>
#include "Port.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {
//
static const NSCam::NSIoPipe::PortID PORT_IMGO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMGO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_UFEO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_UFEO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_RRZO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_RRZO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_CAMSV_IMGO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_CAMSV_IMGO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_CAMSV2_IMGO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_CAMSV2_IMGO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);

static const NSCam::NSIoPipe::PortID PORT_LCSO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_LCSO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_AAO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_AAO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_AFO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_AFO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_PDO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_PDO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_PSO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_PSO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_EISO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_EISO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_FLKO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_FLKO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_RSSO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_RSSO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);

static const NSCam::NSIoPipe::PortID PORT_IMGI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMGI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_IMGBI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMGBI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_IMGCI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMGCI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_VIPI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_VIPI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_VIP2I(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_VIP2I,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_VIP3I(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_VIP3I,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_DEPI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_DEPI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_UFDI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_UFDI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_LCEI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_LCEI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_DMGI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_DMGI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_REGI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_REGI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);

static const NSCam::NSIoPipe::PortID PORT_IMG2O(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMG2O,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_IMG3O(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMG3O,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_IMG3BO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMG3BO /*index*/,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_IMG3CO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_IMG3CO /*index*/,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_MFBO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_MFBO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_PAK2O(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_PAK2O,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_FEO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_FEO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);

static const NSCam::NSIoPipe::PortID PORT_WROTO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_WROTO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_WDMAO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_WDMAO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_JPEGO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_JPEGO /*index*/,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_VENC_STREAMO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_VENC_STREAMO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_WPEO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_WPEO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_MSKO(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_MSKO,
    1 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_WPEI(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_WPEI,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_TUNING(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_TUNING,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_META1(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_META1,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_META2(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_META2,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);
static const NSCam::NSIoPipe::PortID PORT_UNKNOWN(
    EPortType_Memory,
    NSImageio::NSIspio::EPortIndex_UNKNOWN,
    0 /*in/out*/,
    EPortCapbility_None /*capbility*/,
    0 /*frame group*/);

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSIoPipe
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_PORTMAP_H_
