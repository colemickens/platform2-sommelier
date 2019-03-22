/*
 * Copyright (C) 2014-2019 Intel Corporation
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

#define LOG_TAG "JpegMaker"

#include "CameraMetadataHelper.h"
#include "JpegMaker.h"
#include "LogHelper.h"
#include "PlatformData.h"

namespace cros {
namespace intel {

JpegMaker::JpegMaker(int cameraid) :
    mExifMaker(new EXIFMaker),
    mCameraId(cameraid)
{
    LOG1("@%s", __FUNCTION__);
}

JpegMaker::~JpegMaker()
{
    LOG1("@%s", __FUNCTION__);
}

status_t JpegMaker::setupExifWithMetaData(int output_width, int output_height,
                                          ExifMetaData* metaData, const Camera3Request& request)
{
    LOG2("@%s", __FUNCTION__);

    auto* settings = request.getSettings();
    CheckError(settings == nullptr, UNKNOWN_ERROR, "@%s, There is no settings in the request",
               __FUNCTION__);

    processJpegSettings(settings, request.shouldSwapWidthHeight(), metaData);
    processExifSettings(settings, metaData);

    if (request.shouldSwapWidthHeight()) {
        mExifMaker->initialize(output_height, output_width);
    } else {
        mExifMaker->initialize(output_width, output_height);
    }
    mExifMaker->pictureTaken(*metaData);
    if (metaData->mIspMkNote)
        mExifMaker->setDriverData(*metaData->mIspMkNote);
    if (metaData->mIa3AMkNote)
        mExifMaker->setMakerNote(*metaData->mIa3AMkNote);
    if (metaData->mAeConfig)
        mExifMaker->setSensorAeConfig(*metaData->mAeConfig);

    mExifMaker->enableFlash(metaData->mFlashFired, metaData->mV3AeMode, metaData->mFlashMode);

    mExifMaker->initializeLocation(*metaData);

    if (metaData->mSoftware)
        mExifMaker->setSoftware(metaData->mSoftware);

    return NO_ERROR;
}

status_t JpegMaker::getExif(ImgEncoder::EncodePackage& thumbnailPackage, uint8_t* exifPtr,
                            uint32_t* exifSize)
{
    LOG2("@%s:", __FUNCTION__);
    CheckError(exifSize == nullptr, UNKNOWN_ERROR, "@%s, exifSize is a nullptr", __FUNCTION__);

    if (thumbnailPackage.encodedDataSize > 0) {
        mExifMaker->setThumbnail(static_cast<unsigned char*>(thumbnailPackage.output->data()),
                                 thumbnailPackage.encodedDataSize, thumbnailPackage.output->width(),
                                 thumbnailPackage.output->height());
    }
    *exifSize = mExifMaker->makeExif(exifPtr);
    return NO_ERROR;
}

void JpegMaker::processExifSettings(const android::CameraMetadata* settings, ExifMetaData* metaData)
{
    LOG2("@%s:", __FUNCTION__);

    processAwbSettings(settings, metaData);
    processGpsSettings(settings, metaData);
    processScalerCropSettings(settings, metaData);
    processEvCompensationSettings(settings, metaData);
}

/**
 * processJpegSettings
 *
 * Store JPEG settings to the exif metadata
 *
 * \param settings [IN] Metadata of target request
 * \param shouldSwapWidthHeight [IN] The shouldSwapWidthHeight() value of target request
 * \param metadata [IN] The exif metadata that is modified
 *
 */
void JpegMaker::processJpegSettings(const android::CameraMetadata* settings,
                                    bool shouldSwapWidthHeight, ExifMetaData* metaData)
{
    LOG2("@%s:", __FUNCTION__);

    //# METADATA_Control jpeg.quality done
    unsigned int tag = ANDROID_JPEG_QUALITY;
    camera_metadata_ro_entry entry = settings->find(tag);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        metaData->mJpegSetting.jpegQuality = value;
    }

    //# METADATA_Control jpeg.thumbnailQuality done
    tag = ANDROID_JPEG_THUMBNAIL_QUALITY;
    entry = settings->find(tag);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        metaData->mJpegSetting.jpegThumbnailQuality = value;
    }

    //# METADATA_Control jpeg.thumbnailSize done
    tag = ANDROID_JPEG_THUMBNAIL_SIZE;
    entry = settings->find(tag);
    if (entry.count == 2) {
        if (shouldSwapWidthHeight) {
            metaData->mJpegSetting.thumbWidth = entry.data.i32[1];
            metaData->mJpegSetting.thumbHeight = entry.data.i32[0];
        } else {
            metaData->mJpegSetting.thumbWidth = entry.data.i32[0];
            metaData->mJpegSetting.thumbHeight = entry.data.i32[1];
        }
    }

    //# METADATA_Control jpeg.orientation done
    tag = ANDROID_JPEG_ORIENTATION;
    entry = settings->find(tag);
    if (entry.count == 1) {
        metaData->mJpegSetting.orientation = entry.data.i32[0];
    }

    LOG1("jpegQuality=%d,thumbQuality=%d,thumbW=%d,thumbH=%d,orientation=%d",
        metaData->mJpegSetting.jpegQuality,
        metaData->mJpegSetting.jpegThumbnailQuality,
        metaData->mJpegSetting.thumbWidth, metaData->mJpegSetting.thumbHeight,
        metaData->mJpegSetting.orientation);
}

/**
 * This function will get GPS metadata from request setting
 *
 * \param[in] settings The Anroid metadata to process GPS settings from
 * \param[out] metadata The EXIF data where the GPS setting are written to
 */
void JpegMaker::processGpsSettings(const android::CameraMetadata* settings, ExifMetaData* metaData)
{
    LOG2("@%s:", __FUNCTION__);
    unsigned int len = 0;
    // get GPS
    //# METADATA_Control jpeg.gpsCoordinates done
    unsigned int tag = ANDROID_JPEG_GPS_COORDINATES;
    camera_metadata_ro_entry entry = settings->find(tag);
    if (entry.count == 3) {
        metaData->mGpsSetting.latitude = entry.data.d[0];
        metaData->mGpsSetting.longitude = entry.data.d[1];
        metaData->mGpsSetting.altitude = entry.data.d[2];
    }
    LOG2("GPS COORDINATES(%f, %f, %f)", metaData->mGpsSetting.latitude,
         metaData->mGpsSetting.longitude, metaData->mGpsSetting.altitude);

    //# METADATA_Control jpeg.gpsProcessingMethod done
    tag = ANDROID_JPEG_GPS_PROCESSING_METHOD;
    entry = settings->find(tag);
    if (entry.count > 0) {
        MEMCPY_S(metaData->mGpsSetting.gpsProcessingMethod,
                 sizeof(metaData->mGpsSetting.gpsProcessingMethod),
                 entry.data.u8,
                 entry.count);
        len = MIN(entry.count, MAX_NUM_GPS_PROCESSING_METHOD - 1);
        metaData->mGpsSetting.gpsProcessingMethod[len] = '\0';
    }

    //# METADATA_Control jpeg.gpsTimestamp done
    tag = ANDROID_JPEG_GPS_TIMESTAMP;
        entry = settings->find(tag);
    if (entry.count == 1) {
        metaData->mGpsSetting.gpsTimeStamp = entry.data.i64[0];
    }
}

void JpegMaker::processAwbSettings(const android::CameraMetadata* settings, ExifMetaData* metaData)
{
    LOG2("@%s:", __FUNCTION__);

    // get awb mode
    unsigned int tag = ANDROID_CONTROL_AWB_MODE;
    camera_metadata_ro_entry entry = settings->find(tag);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        AwbMode newValue = \
            (value == ANDROID_CONTROL_AWB_MODE_INCANDESCENT) ?
                    CAM_AWB_MODE_WARM_INCANDESCENT : \
            (value == ANDROID_CONTROL_AWB_MODE_FLUORESCENT) ?
                    CAM_AWB_MODE_FLUORESCENT : \
            (value == ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT) ?
                    CAM_AWB_MODE_WARM_FLUORESCENT : \
            (value == ANDROID_CONTROL_AWB_MODE_DAYLIGHT) ?
                    CAM_AWB_MODE_DAYLIGHT : \
            (value == ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT) ?
                    CAM_AWB_MODE_CLOUDY : \
            (value == ANDROID_CONTROL_AWB_MODE_TWILIGHT) ?
                    CAM_AWB_MODE_SUNSET : \
            (value == ANDROID_CONTROL_AWB_MODE_SHADE) ?
                    CAM_AWB_MODE_SHADOW : \
            CAM_AWB_MODE_AUTO;

        metaData->mAwbMode = newValue;
    }
    LOG2("awb mode=%d", metaData->mAwbMode);
}

void JpegMaker::processScalerCropSettings(const android::CameraMetadata* settings,
                                          ExifMetaData* metaData)
{
    LOG2("@%s:", __FUNCTION__);
    android::CameraMetadata staticMeta;
    const int32_t sensorActiveArrayCount = 4;
    const uint32_t scalerCropCount = 4;
    int count = 0;

    staticMeta = PlatformData::getStaticMetadata(mCameraId);
    const int32_t *rangePtr = (int32_t *)MetadataHelper::getMetadataValues(
        staticMeta, ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, TYPE_INT32, &count);

    camera_metadata_ro_entry entry = settings->find(ANDROID_SCALER_CROP_REGION);
    if (entry.count == scalerCropCount && count == sensorActiveArrayCount && rangePtr) {
        if (entry.data.i32[2] != 0 && entry.data.i32[3] != 0
            && rangePtr[2] != 0 && rangePtr[3] != 0) {
            metaData->mZoomRatio = (rangePtr[2] * 100)/ entry.data.i32[2];

            LOG2("scaler width %d height %d, sensor active array width %d height : %d",
                 entry.data.i32[2], entry.data.i32[3], rangePtr[2], rangePtr[3]);
        }
    }
}

void JpegMaker::processEvCompensationSettings(const android::CameraMetadata* settings,
                                              ExifMetaData* metaData)
{
    LOG2("@%s:", __FUNCTION__);
    float stepEV = 1 / 3.0f;
    int32_t evCompensation = 0;

    camera_metadata_ro_entry entry =
        settings->find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    if (entry.count != 1)
        return;

    evCompensation = entry.data.i32[0];
    stepEV = PlatformData::getStepEv(mCameraId);
    // Fill the evBias
    if (metaData->mAeConfig != nullptr)
        metaData->mAeConfig->evBias = evCompensation * stepEV;
}

} /* namespace intel */
} /* namespace cros */
