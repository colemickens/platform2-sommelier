/*
 * Copyright (C) 2013-2019 Intel Corporation
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

#ifndef _CAMERA3_HAL_PROFILE_H_
#define _CAMERA3_HAL_PROFILE_H_

#include <string>
#include <vector>
#include <map>

#include "PlatformData.h"
#include "IPU3CameraCapInfo.h"

#define ANDROID_CONTROL_CAPTURE_INTENT_START 0x40000000
#define CAMERA_TEMPLATE_COUNT (ANDROID_CONTROL_CAPTURE_INTENT_MANUAL + 1)

namespace cros {
namespace intel {
class CameraCapInfo;
/**
 * \class CameraProfiles
 *
 * This is the base class for parsing the camera configuration file.
 * The configuration file is xml format.
 * This class will use the expat lib to do the xml parser.
 */
class CameraProfiles {
public:
    CameraProfiles(CameraHWInfo *cameraHWInfo);
    ~CameraProfiles();

    status_t init();
    camera_metadata_t *constructDefaultMetadata(int cameraId, int requestTemplate);
    const CameraCapInfo *getCameraCapInfo(int cameraId);
    int getXmlCameraId(int cameraId) const;
    static std::string getSensorMediaDevice();
    static std::string getImguMediaDevice();
    bool isFaceAeEnabled(int cameraId) const;
    int readNvmDataFromDevice(int cameraId);

public:
    std::vector<camera_metadata_t*> mStaticMeta;

    // one example: key: 0, value:"sensor_name"
    std::map<int, std::string> mCameraIdToSensorName;

private:
    enum DataField {
        FIELD_INVALID = 0,
        FIELD_ANDROID_STATIC_METADATA,
        FIELD_HAL_TUNING_IPU3,
        FIELD_SENSOR_INFO_IPU3,
        FIELD_MEDIACTL_ELEMENTS_IPU3,
        FIELD_COMMON
    } mCurrentDataField;

    struct MetaValueRefTable {
        const metadata_value_t* table;
        int tableSize;
    };

private:
    static void startElement(void *userData, const char *name, const char **atts);
    static void endElement(void *userData, const char *name);

    static std::string getMediaDeviceByName(std::string deviceName);

    status_t addCamera(int cameraId, const std::string &sensorName);
    void getDataFromXmlFile(void);
    void getGraphConfigFromXmlFile();
    void checkField(CameraProfiles *profiles, const char *name, const char **atts);
    bool isSensorPresent(std::vector<SensorDriverDescriptor> &detectedSensors,
                         const char* profileName, int cameraId) const;
    bool validateStaticMetadata(const char *name, const char **atts);
    const metadata_tag_t* findTagInfo(const char *name, const metadata_tag_t *tagsTable, unsigned int size);
    void handleAndroidStaticMetadata(const char *name, const char **atts);
    void handleHALTuning(const char *name, const char **atts);
    void handleSensorInfo(const char *name, const char **atts);
    void handleMediaCtlElements(const char *name, const char **atts);
    void handleCommon(const char *name, const char **atts);

    void dumpStaticMetadataSection(int cameraId);
    void dumpHalTuningSection(int cameraId);
    void dumpSensorInfoSection(int cameraId);
    void dumpMediaCtlElementsSection(int cameraId);
    void dumpCommonSection();
    void dump(void);

    // Helpers
    int convertEnum(void *dest, const char *src, int type,
                    const metadata_value_t *table, int tableLen,
                    void **newDest);
    int parseGenericTypes(const char *src,
                          const metadata_tag_t *tagInfo,
                          int metadataCacheSize,
                          int64_t *metadataCache);
    int parseEnum(const char *src,
                  const metadata_tag_t *tagInfo,
                  int metadataCacheSize,
                  int64_t *metadataCache);
    int parseEnumAndNumbers(const char *src,
                            const metadata_tag_t *tagInfo,
                            int metadataCacheSize,
                            int64_t *metadataCache);
    int parseStreamConfig(const char *src,
            const metadata_tag_t *tagInfo,
            std::vector<MetaValueRefTable> refTables,
            int metadataCacheSize,
            int64_t *metadataCache);
    int parseStreamConfigDuration(const char *src,
            const metadata_tag_t *tagInfo,
            std::vector<MetaValueRefTable> refTables,
            int metadataCacheSize,
            int64_t *metadataCache);
    int parseData(const char *src,
                  const metadata_tag_t *tagInfo,
                  int metadataCacheSize,
                  int64_t *metadataCache);
    int parseAvailableInputOutputFormatsMap(const char *src,
            const metadata_tag_t *tagInfo,
            std::vector<MetaValueRefTable> refTables,
            int metadataCacheSize,
            int64_t *metadataCache);
    int parseSizes(const char *src,
                   const metadata_tag_t *tagInfo,
                   int metadataCacheSize,
                   int64_t *metadataCache);
    int parseRectangle(const char *src,
                       const metadata_tag_t *tagInfo,
                       int metadataCacheSize,
                       int64_t *metadataCache);
    int parseBlackLevelPattern(const char *src,
                               const metadata_tag_t *tagInfo,
                               int metadataCacheSize,
                               int64_t *metadataCache);
    int parseImageFormats(const char* src,
                          const metadata_tag_t *tagInfo,
                          int metadataCacheSize,
                          int64_t* metadataCache);
    int parseAvailableKeys(const char  *src,
                           const metadata_tag_t *tagInfo,
                           int metadataCacheSize,
                           int64_t *metadataCache);

    const char *skipWhiteSpace(const char *src);
    int getIsysNodeNameAsValue(const char* isysNodeName);
    uint8_t selectAfMode(const camera_metadata_t *staticMeta, int reqTemplate);

private:
    static const int BUFFERSIZE = 4*1024;  // For xml file
    static const int METADATASIZE= 4096;
    static const int mMaxConfigNameLength = 64;

    int64_t *mMetadataCache;  // for metadata construct
    unsigned mXmlSensorIndex;
    unsigned mItemsCount;
    bool mUseEntry;
    bool mProfileEnd[MAX_CAMERAS];
    CameraHWInfo *mCameraCommon;
    std::vector<int> mCameraIdPool;
    std::vector<int> mCharacteristicsKeys[MAX_CAMERAS];

    bool mFaceAeEnabled[MAX_CAMERAS];

    std::vector<SensorDriverDescriptor> mSensorNames;
    std::vector<CameraCapInfo *> mCaps;
    std::vector<std::string> mElementNames;
    std::vector<camera_metadata_t *> mDefaultRequests;
};

} /* namespace intel */
} /* namespace cros */
#endif // _CAMERA3_HAL_PROFILE_H_
