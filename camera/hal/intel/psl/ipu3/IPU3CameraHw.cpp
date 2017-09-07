/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#define LOG_TAG "IPU3CameraHw"

#include "IPU3CameraHw.h"
#include "LogHelper.h"
#include "CameraMetadataHelper.h"
#include "RequestThread.h"
#include "HwStreamBase.h"
#include "PlatformData.h"
#include "ImguUnit.h"
#include "ControlUnit.h"
#include "CaptureUnit.h"
#include "PSLConfParser.h"

namespace android {
namespace camera2 {

#define CONFIGURE_LARGE_SIZE (1920*1080)

// Camera factory
ICameraHw *CreatePSLCamera(int cameraId) {
    return new IPU3CameraHw(cameraId);
}

IPU3CameraHw::IPU3CameraHw(int cameraId):
        mCameraId(cameraId),
        mStaticMeta(nullptr),
        mPipelineDepth(-1),
        mImguUnit(nullptr),
        mControlUnit(nullptr),
        mCaptureUnit(nullptr),
        mGCM(cameraId),
        mStreamsConfiguredWithLargeSize(false),
        mOperationMode(0)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

IPU3CameraHw::~IPU3CameraHw()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    deInit();
}

status_t
IPU3CameraHw::init()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    status_t status = NO_ERROR;

    std::string sensorMediaDevice = PSLConfParser::getSensorMediaDevice();
    mMediaCtl = std::make_shared<MediaController>(sensorMediaDevice.c_str());
    status = mMediaCtl->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing Media Controller");
        return status;
    }

    std::string imguMediaDevice = PSLConfParser::getImguMediaDevice();
    mImguMediaCtl = std::make_shared<MediaController>(imguMediaDevice.c_str());
    status = mImguMediaCtl->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing Media Controller");
        return status;
    }

    mGCM.setMediaCtl(mMediaCtl);

    mCaptureUnit = new CaptureUnit(mCameraId, mGCM, mMediaCtl);
    status = mCaptureUnit->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing CaptureUnit, ret code: %x", status);
        return status;
    }

    mImguUnit = new ImguUnit(mCameraId, mGCM, mImguMediaCtl);

    mControlUnit = new ControlUnit(mImguUnit,
                                   mCaptureUnit,
                                   mCameraId,
                                   mGCM);
    status = mControlUnit->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing ControlUnit. ret code:%x", status);
        return status;
    }

    // Register ControlUnit as a listener to capture events
    status = mImguUnit->attachListener(mControlUnit);
    status |= mCaptureUnit->attachListener(mControlUnit);
    if (status != NO_ERROR) {
        LOGE("Error registering CaptureUnit listener");
        return status;
    }

    status = initStaticMetadata();
    if (status != NO_ERROR) {
        LOGE("Error call initStaticMetadata, status:%d", status);
        return status;
    }

    return status;
}

void
IPU3CameraHw::deInit()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mImguUnit) {
       mImguUnit->cleanListener();
    }

    if (mCaptureUnit) {
        mCaptureUnit->cleanListeners();
        mCaptureUnit->flush();
    }

    if (mControlUnit) {
        delete mControlUnit;
        mControlUnit = nullptr;
    }

    if (mImguUnit) {
        delete mImguUnit;
        mImguUnit = nullptr;
    }

    if (mCaptureUnit) {
        delete mCaptureUnit;
        mCaptureUnit = nullptr;
    }

    if(mStaticMeta) {
        /**
         * The metadata buffer this object uses belongs to PlatformData
         * we release it before we destroy the object.
         */
        mStaticMeta->release();
        delete mStaticMeta;
        mStaticMeta = nullptr;
    }
}

const camera_metadata_t *
IPU3CameraHw::getDefaultRequestSettings(int type)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    return PlatformData::getDefaultMetadata(mCameraId, type);
}

status_t IPU3CameraHw::checkStreamSizes(std::vector<camera3_stream_t*> &activeStreams)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    int32_t count;

    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    if (!meta) {
        LOGE("Cannot get static metadata.");
        return BAD_VALUE;
    }

    int32_t *availStreamConfig = (int32_t*)MetadataHelper::getMetadataValues(meta,
                                 ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                                 TYPE_INT32,
                                 &count);

    if (availStreamConfig == nullptr) {
        LOGE("Cannot get stream configuration from static metadata.");
        return BAD_VALUE;
    }

    // check the stream sizes are reasonable. Note the array of integers in
    // availStreamConfig needs to be jumped by four, W & H are at +1 & +2
    for (uint32_t i = 0; i < activeStreams.size(); i++) {
        bool matchFound = false;
        for (uint32_t j = 0; j < (uint32_t)count; j += 4) {
            if ((activeStreams[i]->width  == (uint32_t)availStreamConfig[j + 1]) &&
                (activeStreams[i]->height == (uint32_t)availStreamConfig[j + 2])) {
                matchFound = true;
                break;
            }
        }
        if (!matchFound) {
            LOGE("Camera stream config had unsupported dimension %dx%d.",
                 activeStreams[i]->width,
                 activeStreams[i]->height);
            return BAD_VALUE;
        }
    }

    return OK;
}


status_t
IPU3CameraHw::configStreams(std::vector<camera3_stream_t*> &activeStreams,
                            uint32_t operation_mode)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    uint32_t maxBufs, usage;
    status_t status = NO_ERROR;
    bool configChanged = true;

    mOperationMode = operation_mode;
    mStreamsAll.clear();
    mStreamsWithSmallSize.clear();

    if (checkStreamSizes(activeStreams) != OK)
        return BAD_VALUE;

    maxBufs = mPipelineDepth;  /* value from XML static metadata */
    if (maxBufs > MAX_REQUEST_IN_PROCESS_NUM)
        maxBufs = MAX_REQUEST_IN_PROCESS_NUM;

    /**
     * Here we could give different gralloc flags depending on the stream
     * format, at the moment we give the same to all
     */
    usage = GRALLOC_USAGE_SW_READ_OFTEN |
            GRALLOC_USAGE_SW_WRITE_NEVER |
            GRALLOC_USAGE_HW_CAMERA_WRITE;

    for (unsigned int i = 0; i < activeStreams.size(); i++) {
        activeStreams[i]->max_buffers = maxBufs;
        activeStreams[i]->usage |= usage;
        mStreamsAll.push_back(activeStreams[i]);

        if (activeStreams[i]->width * activeStreams[i]->height <= CONFIGURE_LARGE_SIZE) {
            mStreamsWithSmallSize.push_back(activeStreams[i]);
        }
    }
    mStreamsConfiguredWithLargeSize = (mStreamsAll.size() > mStreamsWithSmallSize.size());


    /* Flush to make sure we return all graph config objects to the pool before
       next stream config. */
    mCaptureUnit->flush();
    mImguUnit->flush();
    mControlUnit->flush();

    status = mGCM.configStreams(mStreamsAll, operation_mode);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream: No matching graph config found! BUG");
        return status;
    }

    status = mCaptureUnit->configStreams(activeStreams, configChanged);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream");
        return status;
    }

    status = mImguUnit->configStreams(activeStreams);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream for ImguUnit");
        return status;
    }

    status = mControlUnit->configStreamsDone(configChanged);

    return status;
}

status_t
IPU3CameraHw::bindStreams(std::vector<CameraStreamNode *> activeStreams)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    mDummyHwStreamsVector.clear();
    // Need to have a producer stream as it is required by common code
    // bind all streams to dummy HW stream class.
    for (unsigned int i = 0; i < activeStreams.size(); i++) {
        CameraStreamNode *stream = activeStreams.at(i);
        HwStreamBase *hwStream = new HwStreamBase(*stream);
        CameraStream::bind(stream, hwStream);
        mDummyHwStreamsVector.push_back(hwStream);
    }

    return status;
}

status_t
IPU3CameraHw::processRequest(Camera3Request* request, int inFlightCount)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (inFlightCount > mPipelineDepth) {
        LOG2("@%s:blocking request %d", __FUNCTION__, request->getId());
        return RequestThread::REQBLK_WAIT_ONE_REQUEST_COMPLETED;
    }

    status_t status = NO_ERROR;
    // Check reconfiguration
    bool hasLargeSize = requireStreamWithLargeSize(request);
    if (hasLargeSize != mStreamsConfiguredWithLargeSize) {
        LOG1("%s: request %d need reconfigure, infilght %d", __FUNCTION__,
                request->getId(), inFlightCount);
        if (inFlightCount > 1) {
            return RequestThread::REQBLK_WAIT_ALL_PREVIOUS_COMPLETED;
        }

        status = reconfigureStreams(hasLargeSize, mOperationMode);
    }

    if (status == NO_ERROR) {
        mControlUnit->processRequest(request,
                                     mGCM.getGraphConfig(*request));
    }
    return status;
}

bool IPU3CameraHw::requireStreamWithLargeSize(Camera3Request* request) const
{
    const std::vector<camera3_stream_buffer>* buffers = request->getOutputBuffers();
    for (const auto & buf : *buffers) {
        if (buf.stream->width * buf.stream->height > CONFIGURE_LARGE_SIZE) {
            return true;
        }
    }

    return false;
}

status_t IPU3CameraHw::reconfigureStreams(bool configLargeSizeStream, uint32_t operation_mode)
{
    LOG1("%s: config streams with large size? %d", __FUNCTION__, configLargeSizeStream);
    mStreamsConfiguredWithLargeSize = configLargeSizeStream;
    std::vector<camera3_stream_t*>& streams = configLargeSizeStream ?
                                              mStreamsAll : mStreamsWithSmallSize;

    /* Flush to make sure we return all graph config objects to the pool before
       next stream config. */
    mCaptureUnit->flush();
    mImguUnit->flush();
    mControlUnit->flush();

    status_t status = mGCM.configStreams(streams, operation_mode);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream: No matching graph config found! BUG");
        return status;
    }

    status = mCaptureUnit->configStreams(streams, true);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream");
        return status;
    }

    status = mImguUnit->configStreams(streams);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream for ImguUnit");
        return status;
    }

    return mControlUnit->configStreamsDone(true);
}

status_t
IPU3CameraHw::flush()
{
    return NO_ERROR;
}

void
IPU3CameraHw::dump(int fd)
{
    UNUSED(fd);
}

/**
 * initStaticMetadata
 *
 * Create CameraMetadata object to retrieve the static tags used in this class
 * we cache them as members so that we do not need to query CameraMetadata class
 * every time we need them. This is more efficient since find() is not cheap
 */
status_t
IPU3CameraHw::initStaticMetadata(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    status_t status = NO_ERROR;
    /**
    * Initialize the CameraMetadata object with the static metadata tags
    */
    camera_metadata_t* staticMeta;
    staticMeta = (camera_metadata_t*)PlatformData::getStaticMetadata(mCameraId);
    mStaticMeta = new CameraMetadata(staticMeta);

    camera_metadata_entry entry;
    entry = mStaticMeta->find(ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    if(entry.count == 1) {
        mPipelineDepth = entry.data.u8[0];
    } else {
        mPipelineDepth = DEFAULT_PIPELINE_DEPTH;
    }

    /**
     * Check the consistency of the information we had in XML file.
     * partial result count is very tightly linked to the PSL
     * implementation
     */
    int32_t xmlPartialCount = PlatformData::getPartialMetadataCount(mCameraId);
    if (xmlPartialCount != PARTIAL_RESULT_COUNT) {
            LOGW("Partial result count does not match current implementation "
                 " got %d should be %d, fix the XML!", xmlPartialCount,
                 PARTIAL_RESULT_COUNT);
            status = NO_INIT;
    }
    return status;
}

}  // namespace camera2
}  // namespace android
