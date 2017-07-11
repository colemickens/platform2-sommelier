/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#ifndef _CAMERA3_HAL_CAMERA_CONFIGURATION_H_
#define _CAMERA3_HAL_CAMERA_CONFIGURATION_H_

#include <sys/stat.h>
#include <string>
#include <vector>
#include <map>
#include <utils/Errors.h>
#include <memory>

#include <system/camera_metadata.h>
#include "ia_cmc_parser.h"
#include "Metadata.h"

NAMESPACE_DECLARATION {
class CameraHWInfo;

#define CPF_MODE_DEFAULT "default"
#define CPF_MODE_STILL   "still"
#define CPF_MODE_VIDEO   "video"
#define CPF_MODE_PREVIEW "preview"

class AiqConf
{
public:
    explicit AiqConf(const int size = 0);
    ~AiqConf();
    ia_cmc_t* getCMCHandler() const { return mCMC; }
    status_t fillStaticMetadataFromCMC(camera_metadata_t * metadata);
    status_t initCMC();
    void *ptr() const { return mPtr; }
    int size() const { return mSize; }

private:
    void *mPtr;
    int mSize;
    typedef std::multimap<cmc_light_source, int> LightSrcMap;
    status_t initMapping(LightSrcMap &lightSourceMap);
    status_t fillLensStaticMetadata(camera_metadata_t * metadata);
    status_t fillLightSourceStaticMetadata(camera_metadata_t * metadata);
    status_t fillSensorStaticMetadata(camera_metadata_t * metadata);
    status_t fillLscSizeStaticMetadata(camera_metadata_t * metadata);

private:
    ia_cmc_t *mCMC; /* CameraConf owns mCMC */
    camera_metadata_t* mMetadata; /* CameraConf doesn't own mMetadata */
};

class CpfStore
{
public:
    explicit CpfStore(const int xmlCameraId, CameraHWInfo * cameraHWInfo);
    virtual ~CpfStore();
public:
    std::multimap<std::string, AiqConf*> mAiqConfig;
private:
    status_t initFileNames(std::vector<std::string> &cpfFileNames);
    status_t findConfigFile(const std::string &path, std::vector<std::string> &cpfFileNames);
    status_t getCpfFileList(const std::string &path, std::vector<std::string> &cpfFileNames);
    status_t loadConf(const std::string &cpfFileName);
    void filterKnownSensors(std::vector<std::string> &allCpfFileNames,
                            std::vector<std::string> &registeredCpfFileNames);
    void getCpfFileMode(const std::string &cpfFileName, std::string &mode);
private:
    std::vector<AiqConf *> mAiqConf;
    int mCameraId;
    bool mHasMediaController;  // TODO: REMOVE. Hack to workaround mCameraId to ISP port comparison issue.
    bool mIsOldConfig;
    std::vector<std::string> mCpfFileNames;
    std::vector<struct SensorDriverDescriptor> mRegisteredDrivers;
    // Disallow copy and assignment
    CpfStore(const CpfStore&);
    void operator=(const CpfStore&);
};

} NAMESPACE_DECLARATION_END
#endif // _CAMERA3_HAL_CAMERA_CONFIGURATION_H_
