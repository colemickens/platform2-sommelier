/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#define LOG_TAG "GraphConfigManager"

#include "GraphConfigManager.h"
#include "GraphConfig.h"
#include "PlatformData.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "Camera3Request.h"
#include <GCSSParser.h>

using namespace GCSS;
using std::vector;
using std::map;

// Settings to use in fallback cases
#define DEFAULT_SETTING_1_VIDEO_1_STILL "8302" // 7002, 1 video, 1 still stream
#define DEFAULT_SETTING_2_VIDEO_2_STILL "7004" // 7004, 2 video, 2 stills streams
#define DEFAULT_SETTING_2_STILL "7005" // 7005, 2 still streams
#define DEFAULT_SETTING_1_STILL "7006" // 7006, 1 still stream

namespace android {
namespace camera2 {

const char *GraphConfigManager::DEFAULT_DESCRIPTOR_FILE = "/etc/camera/graph_descriptor.xml";
const char *GraphConfigManager::DEFAULT_SETTINGS_FILE = "/etc/camera/graph_settings.xml";

GraphConfigNodes::GraphConfigNodes() :
        mDesc(nullptr),
        mSettings(nullptr)
{
}

GraphConfigNodes::~GraphConfigNodes()
{
    delete mDesc;
    delete mSettings;
}

GraphConfigManager::GraphConfigManager(int32_t camId,
                                       GraphConfigNodes *testNodes) :
    mCameraId(camId),
    mFallback(false),
    mGraphQueryManager(new GraphQueryManager())
{
    const CameraCapInfo *info = PlatformData::getCameraCapInfo(mCameraId);
    if (CC_UNLIKELY(!info || !info->getGraphConfigNodes())) {
        LOGE("Failed to get camera %d info - BUG", mCameraId);
        return;
    }
    // TODO: need casting because GraphQueryManager interface is clumsy.
    GraphConfigNodes *nodes = testNodes;
    if (nodes == nullptr) {
        nodes = const_cast<GraphConfigNodes*>(info->getGraphConfigNodes());
    }

    if (mGraphQueryManager.get() != nullptr && nodes != nullptr) {
        mGraphQueryManager->setGraphDescriptor(nodes->mDesc);
        mGraphQueryManager->setGraphSettings(nodes->mSettings);
    } else {
        LOGE("Failed to allocate Graph Query Manager -- FATAL");
        return;
    }

    status_t status = mGraphConfigPool.init(MAX_REQ_IN_FLIGHT,
                                            GraphConfig::reset);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to initialize the pool of GraphConfigs");
    }
}

/**
 * Generate the helper vectors mVideoStreamResolutions and
 *  mStillStreamResolutions used during stream configuration.
 *
 * This is a helper member to store the ItemUID's for the width and height of
 * each stream. Each ItemUID points to items like :
 *  video0.width
 *  video0.height
 * This vector needs to be regenerated after each stream configuration.
 */
void GraphConfigManager::initStreamResolutionIds()
{
    mVideoStreamResolutions.clear();
    mStillStreamResolutions.clear();
    mVideoStreamKeys.clear();
    mStillStreamKeys.clear();

    mVideoStreamKeys.push_back(GCSS_KEY_IMGU_PREVIEW);
    mStillStreamKeys.push_back(GCSS_KEY_IMGU_VIDEO);
    mStillStreamKeys.push_back(GCSS_KEY_IMGU_RAW);

    for (size_t i = 0; i < mVideoStreamKeys.size(); i++) {
        ItemUID w = {mVideoStreamKeys[i], GCSS_KEY_WIDTH};
        ItemUID h = {mVideoStreamKeys[i], GCSS_KEY_HEIGHT};
        mVideoStreamResolutions.push_back(std::make_pair(w,h));
    }
    for (size_t i = 0; i < mStillStreamKeys.size(); i++) {
        ItemUID w = {mStillStreamKeys[i], GCSS_KEY_WIDTH};
        ItemUID h = {mStillStreamKeys[i], GCSS_KEY_HEIGHT};
        mStillStreamResolutions.push_back(std::make_pair(w,h));
    }
}

GraphConfigManager::~GraphConfigManager()
{
    // Check that all graph config objects are returned to the pool
    if(!mGraphConfigPool.isFull()) {
        LOGE("GraphConfig pool is missing objects at destruction!");
    }
}

/**
 * Add predefined keys used in android to the map used by the graph config
 * parser.
 *
 * This method is static and should only be called once.
 *
 * We do this so that the keys we will use in the queries are already defined
 * and we can create the query objects in a more compact way, by using the
 * ItemUID initializers.
 */
void GraphConfigManager::addAndroidMap()
{
    /**
     * Initialize the map with android specific tags found in the
     * Graph Config XML's
     */
    #define GCSS_KEY(key, str) std::make_pair(#str, GCSS_KEY_##key),
    map<std::string, uint32_t> ANDROID_GRAPH_KEYS = {
        #include "platform_gcss_keys.h"
        #include "ipu3_android_gcss_keys.h"
    };
    #undef GCSS_KEY

    LOG1("Adding %d android specific keys to graph config parser",
            ANDROID_GRAPH_KEYS.size());

    /*
     * add Android specific tags so parser can use them
     */
    ItemUID::addCustomKeyMap(ANDROID_GRAPH_KEYS);
}
/**
 *
 * Static method to parse the XML graph configurations and settings
 *
 * This method is currently called once per camera.
 *
 * \param[in] descriptorXmlFile: name of the file where the graphs are described
 * \param[in] settingsXmlFile: name of the file where the settings are listed
 *
 * \return nullptr if parsing failed.
 * \return pointer to a valid GraphConfigNode object. Ownership passes to
 *         caller.
 */
GraphConfigNodes* GraphConfigManager::parse(const char *descriptorXmlFile,
                                            const char *settingsXmlFile)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    GCSSParser parser;

    GraphConfigNodes *nodes = new GraphConfigNodes;

    parser.parseGCSSXmlFile(descriptorXmlFile, &nodes->mDesc);
    if (!nodes->mDesc) {
        LOGE("Failed to parse graph descriptor from %s", descriptorXmlFile);
        delete nodes;
        return nullptr;
    }

    parser.parseGCSSXmlFile(settingsXmlFile, &nodes->mSettings);
    if (!nodes->mSettings) {
        LOGE("Failed to parse graph settings from %s", settingsXmlFile);
        delete nodes;
        return nullptr;
    }

    return nodes;
}

/**
 * Perform a reverse lookup on the map that associates client streams to
 * virtual sinks.
 *
 * This method is used during pipeline configuration to find a stream associated
 * with the id (GCSS key) of the virtual sink
 *
 * \param[in] vPortId GCSS key representing one of the virtual sinks in the
 *                    graph, like GCSS_KEY_VIDEO1
 * \return nullptr if not found
 * \return pointer to the client stream associated with that virtual sink.
 */
camera3_stream_t* GraphConfigManager::getStreamByVirtualId(uid_t vPortId)
{
    std::map<camera3_stream_t*, uid_t>::iterator it;
    it = mStreamToSinkIdMap.begin();

    for (; it != mStreamToSinkIdMap.end(); ++it) {
        if (it->second == vPortId) {
            return it->first;
        }
    }
    return nullptr;
}

bool GraphConfigManager::needSwapVideoPreview(GCSS::GraphConfigNode* graphCfgNode, int32_t id)
{
    bool swapVideoPreview = false;
    int previewWidth = 0;
    int previewHeight = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    css_err_t ret1 = ia_err_none;
    css_err_t ret2 = ia_err_none;

    GraphConfigNode* node = nullptr;
    std::string nodeName = GC_PREVIEW;
    graphCfgNode->getDescendantByString(nodeName, &node);
    if (node) {
        ret1 = node->getValue(GCSS_KEY_WIDTH, previewWidth);
        ret2 = node->getValue(GCSS_KEY_HEIGHT, previewHeight);
        if (ret1 != ia_err_none || ret2 != ia_err_none) {
            LOGE("@%s, fail to get width or height for node %s, ret1:%d, ret2:%d",
                __FUNCTION__, nodeName.c_str(), ret1, ret2);
            return swapVideoPreview;
        }
    }
    LOG2("@%s, settings id:%d, for %s, width:%d, height:%d",
        __FUNCTION__, id, nodeName.c_str(), previewWidth, previewHeight);

    node = nullptr;
    nodeName = GC_VIDEO;
    graphCfgNode->getDescendantByString(nodeName, &node);
    if (node) {
        ret1 = node->getValue(GCSS_KEY_WIDTH, videoWidth);
        ret2 = node->getValue(GCSS_KEY_HEIGHT, videoHeight);
        if (ret1 != ia_err_none || ret2 != ia_err_none) {
            LOGE("@%s, fail to get width or height for node %s, ret1:%d, ret2:%d",
                __FUNCTION__, nodeName.c_str(), ret1, ret2);
            return swapVideoPreview;
        }
    }
    LOG2("@%s, settings id:%d, for %s, width:%d, height:%d",
        __FUNCTION__, id, nodeName.c_str(), videoWidth, videoHeight);

    if (previewWidth != 0 && previewHeight != 0
        && videoWidth != 0 && videoHeight != 0) {
        if (previewWidth > videoWidth
            && previewHeight > videoHeight)
            swapVideoPreview = true;
    }
    LOG2("@%s, swapVideoPreview:%d", __FUNCTION__, swapVideoPreview);

    return swapVideoPreview;
}

void GraphConfigManager::handleVideoStream(ResolutionItem& res, PlatformGraphConfigKey& streamKey)
{
    res = mVideoStreamResolutions[0];
    mVideoStreamResolutions.erase(mVideoStreamResolutions.begin());
    streamKey = mVideoStreamKeys[0];
    mVideoStreamKeys.erase(mVideoStreamKeys.begin());
}

void GraphConfigManager::handleStillStream(ResolutionItem& res, PlatformGraphConfigKey& streamKey)
{
    res = mStillStreamResolutions[0];
    mStillStreamResolutions.erase(mStillStreamResolutions.begin());
    streamKey = mStillStreamKeys[0];
    mStillStreamKeys.erase(mStillStreamKeys.begin());
}

void GraphConfigManager::handleMap(camera3_stream_t* stream, ResolutionItem& res, PlatformGraphConfigKey& streamKey)
{
    LOG1("Adding stream %p to map %s", stream, ItemUID::key2str(streamKey));
    mStreamToSinkIdMap[stream] = streamKey;

    ItemUID w = res.first;
    ItemUID h = res.second;

    mQuery[w] = std::to_string(stream->width);
    mQuery[h] = std::to_string(stream->height);
}

status_t GraphConfigManager::mapStreamToKey(const std::vector<camera3_stream_t*> &streams,
                                                    int& videoStreamCnt, int& stillStreamCnt,
                                                    int& needEnableStill)
{
    for (int i = 0; i < streams.size(); i++) {
        if ( streams[i]->stream_type != CAMERA3_STREAM_OUTPUT) {
            LOGE("@%s, the streamd[%d] is not CAMERA3_STREAM_OUTPUT, it's:%d",
                __FUNCTION__, i, streams[i]->stream_type);
            return UNKNOWN_ERROR;
        }
    }

    std::vector<camera3_stream_t *> yuvStreams;
    std::vector<camera3_stream_t *> blobStreams;
    yuvStreams.clear();
    blobStreams.clear();
    for (int i = 0; i < streams.size(); i++) {
        switch (streams[i]->format) {
            case HAL_PIXEL_FORMAT_BLOB:
                 blobStreams.push_back(streams[i]);
                 break;
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
            case HAL_PIXEL_FORMAT_YCbCr_422_I:
            case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
                 yuvStreams.push_back(streams[i]);
                 break;
            default:
                LOGW("Unsupported stream format %d", streams.at(i)->format);
                break;
        }
    }

    int yuvNum = yuvStreams.size();
    int blobNum = blobStreams.size();
    LOG2("@%s, yuvNum:%d, blobNum:%d", __FUNCTION__, yuvNum, blobNum);
    PlatformGraphConfigKey streamKey;
    ResolutionItem res;

    if (blobNum) {
        // still pipe doesn't have statistic information, use video pipe
        // TODO: ensure still pipe use correct statistic data
        needEnableStill = false;
        LOGD("@%s, it has BLOB, needEnableStill:%d", __FUNCTION__, needEnableStill);
    }

    if (blobNum == 1) {
        if (yuvNum == 0) { // 1 JPEG stream only
            stillStreamCnt++;
            handleStillStream(res, streamKey);
            handleMap(blobStreams[0], res, streamKey);
        } else if (yuvNum == 1) { // 1 JPEG stream and 1 YUV stream
            camera3_stream_t* sYuv = yuvStreams[0];
            camera3_stream_t* sBlob = blobStreams[0];
            if (sBlob->width >= sYuv->width) {
                stillStreamCnt++;
                handleStillStream(res, streamKey);
                handleMap(blobStreams[0], res, streamKey);

                videoStreamCnt++;
                handleVideoStream(res, streamKey);
                handleMap(yuvStreams[0], res, streamKey);
            } else {
                stillStreamCnt++;
                handleStillStream(res, streamKey);
                handleMap(yuvStreams[0], res, streamKey);

                videoStreamCnt++;
                handleVideoStream(res, streamKey);
                handleMap(blobStreams[0], res, streamKey);
            }
        } else {
            LOGE("@%s, line:%d, ERROR, blobNum:%d, yuvNum:%d", __FUNCTION__, __LINE__, blobNum, yuvNum);
            return UNKNOWN_ERROR;
        }

        return OK;
    } else if (blobNum > 1) {
        LOGE("@%s, line:%d, ERROR, blobNum:%d, yuvNum:%d", __FUNCTION__, __LINE__, blobNum, yuvNum);
        return UNKNOWN_ERROR;
    } else {
        LOG2("@%s, no blob", __FUNCTION__);
    }

    if (yuvNum == 1) { // 1 YUV stream only
        stillStreamCnt++;
        handleStillStream(res, streamKey);
        handleMap(yuvStreams[0], res, streamKey);
    } else if (yuvNum == 2) { // 2 YUV streams
        int maxWidth = 0;
        int idx = 0;
        for (int i = 0; i < yuvNum; i++) {
            camera3_stream_t* s = yuvStreams[i];
            if (s->width > maxWidth) {
                maxWidth = s->width;
                idx = i;
            }
        }

        stillStreamCnt++;
        handleStillStream(res, streamKey);
        handleMap(yuvStreams[idx], res, streamKey);

        videoStreamCnt++;
        handleVideoStream(res, streamKey);
        handleMap(yuvStreams[1 - idx], res, streamKey);
    } else {
        LOGE("@%s, ERROR, the current yuv stream number is:%d", __FUNCTION__, yuvNum);
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Initialize the state of the GraphConfigManager after parsing the stream
 * configuration.
 * Perform the first level query to find a subset of settings that fulfill the
 * constrains from the stream configuration.
 *
 * TODO: Pass the new stream config modifier
 *
 * \param[in] streams List of streams required by the client.
 */
status_t GraphConfigManager::configStreams(const vector<camera3_stream_t*> &streams,
                                           uint32_t operationMode)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    HAL_KPI_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, 1000000); /* 1 ms*/
    PlatformGraphConfigKey streamKey;
    mFirstQueryResults.clear();
    mQuery.clear();
    ResolutionItem res;
    mFallback = false;
    UNUSED(operationMode);
    int needEnableStill = false;
    status_t ret = OK;

    /*
     * Add to the query the number of active outputs
     */
    ItemUID streamCount = {GCSS_KEY_ACTIVE_OUTPUTS};
    mQuery[streamCount] = std::to_string(streams.size());
    /*
     * regenerate the stream resolutions vector if needed
     * We do this because we consume this vector for each stream configuration.
     * This allows us to have sequential stream numbers even when an input
     * stream is present.
     */
    initStreamResolutionIds();
    mStreamToSinkIdMap.clear();

    int videoStreamCount = 0, stillStreamCount = 0;
    ret = mapStreamToKey(streams, videoStreamCount, stillStreamCount, needEnableStill);
    if (ret != OK) {
        LOGE("@%s, call mapStreamToKey fail, ret:%d", __FUNCTION__, ret);
        return ret;
    }

    /**
     * Look for settings. If query results are empty, get default settings
     */
    int32_t id = 0;
    string settingsId = "0";
    mGraphQueryManager->queryGraphs(mQuery, mFirstQueryResults);
    if (mFirstQueryResults.empty()) {

        dumpQuery(mQuery);
        mFallback = true;
        mQuery.clear();
        status_t status = OK;
        status = selectDefaultSetting(videoStreamCount, stillStreamCount, settingsId);
        if (status != OK) {
            return UNKNOWN_ERROR;
        }

        ItemUID content1({GCSS_KEY_KEY});
        mQuery.insert(std::make_pair(content1, settingsId));
        mGraphQueryManager->queryGraphs(mQuery, mFirstQueryResults);

        if (!mFirstQueryResults.empty()) {
            mFirstQueryResults[0]->getValue(GCSS_KEY_KEY, id);
            LOGD("CAM[%d]Default settings in use for this stream configuration. Settings id %d", mCameraId, id);
        } else {
            LOGE("Failed to retrieve default settings(%s)", settingsId.c_str());
            return UNKNOWN_ERROR;
        }

    } else {
        mFirstQueryResults[0]->getValue(GCSS_KEY_KEY, id);
        LOGD("CAM[%d]Graph config in use for this stream configuration - SUCCESS, settings id %d", mCameraId, id);
    }
    dumpStreamConfig(streams); // TODO: remove this when GC integration is done

    /*
     * Currently it is enough to refresh information in graph config objects
     * per stream config. Here we populate all gc objects in the pool.
     */
    std::shared_ptr<GraphConfig> gc = nullptr;
    int poolSize = mGraphConfigPool.availableItems();
    for (int i = 0; i < poolSize; i++) {
        mGraphConfigPool.acquireItem(gc);
        ret = prepareGraphConfig(gc);
        if (ret != OK) {
            LOGE("Failed to prepare graph config");
            dumpQuery(mQuery);
            return UNKNOWN_ERROR;
        }
    }

    if (gc.get() == nullptr) {
        LOGE("Graph config is NULL, BUG!");
        return UNKNOWN_ERROR;
    }

    // Get media control config
    mMediaCtlConfigsPrev = mMediaCtlConfigs;
    gc->setMediaCtl(mMediaCtl);
    ret = gc->getMediaCtlData(mMediaCtlConfigs);
    if (ret != OK) {
        LOGE("Couldn't get mediaCtl data");
    }

    bool swapVideoPreview = needSwapVideoPreview(mFirstQueryResults[0], id);

    mImguMediaCtlConfigsPrev = mImguMediaCtlConfigs;
    ret = gc->getImguMediaCtlData(mImguMediaCtlConfigs, swapVideoPreview, needEnableStill);
    if (ret != OK) {
        LOGE("Couldn't get Imgu mediaCtl data");
    }

    return OK;
}

/**
 * Prepare graph config object
 *
 * Use graph query results as a parameter to getGraph. The result will be given
 * to graph config object.
 *
 * \param[in/out] gc     Graph Config object.
 */
status_t GraphConfigManager::prepareGraphConfig(std::shared_ptr<GraphConfig> gc)
{
    css_err_t ret;
    status_t status = OK;
    GraphConfigNode *result = new GraphConfigNode;
    ret  = mGraphQueryManager->getGraph(mFirstQueryResults[0], result);
    if (CC_UNLIKELY(ret != ia_err_none)) {
        gc.reset();
        delete result;
        return UNKNOWN_ERROR;
    }

    status = gc->prepare(this, result, mStreamToSinkIdMap, mFallback);
    LOG1("Graph config object prepared");

    return status;
}

/**
 * Find suitable default setting based on stream config.
 *
 * \param[in] videoStreamCount
 * \param[in] stillStreamCount
 * \param[out] settingsId
 * \return OK when success, UNKNOWN_ERROR on failure
 */
status_t GraphConfigManager::selectDefaultSetting(int videoStreamCount,
                                                  int stillStreamCount,
                                                  string &settingsId)
{
    // Determine which default setting to use
    switch (videoStreamCount) {
    case 0:
        if (stillStreamCount == 1) {
            settingsId = DEFAULT_SETTING_1_STILL; // 0 video, 1 still
        } else if (stillStreamCount == 2) {
            settingsId = DEFAULT_SETTING_2_STILL; // 0 video, 2 still
        } else {
            LOGE("Default settings cannot support 0 video, >2 still streams");
            return UNKNOWN_ERROR;
        }
        break;
    case 1:
        if ((stillStreamCount == 0) || (stillStreamCount == 1)) {
            settingsId = DEFAULT_SETTING_1_VIDEO_1_STILL; // 1 video, 1 still
        } else if (stillStreamCount == 2) {
            settingsId = DEFAULT_SETTING_2_VIDEO_2_STILL; // 2 video, 2 still
        } else {
            LOGE("Default settings cannot support 1 video, >2 still streams");
            return UNKNOWN_ERROR;
        }
        break;
    case 2:
        // Works for 2 video 2 still, and 2 video 1 still.
        settingsId = DEFAULT_SETTING_2_VIDEO_2_STILL; // 2 video, 2 still
        if (stillStreamCount > 2) {
            LOGE("Default settings cannot support 2 video, >2 still streams");
            return UNKNOWN_ERROR;
        }
        break;
    default:
        LOGE("Default settings cannot support > 2 video streams");
        return UNKNOWN_ERROR;
    }
    return OK;
}

/**
 * Retrieve the current active media controller configuration for Sensor + ISA
 *
 * This method will be removed as we clean up the CaptureUnit
 *
 */
const MediaCtlConfig* GraphConfigManager::getMediaCtlConfig(IStreamConfigProvider::MediaType type) const
{
    vector<int> foundConfigId;
    int id = 0;

    if (mFirstQueryResults.empty()) {
        LOGE("Invalid operation, first level query no done yet");
        return nullptr;
    }

    for (size_t i = 0; i < mFirstQueryResults.size(); i++) {
        foundConfigId.push_back(id);
    }
    LOG1("Number of available Sensor+ISA configs for this stream config: %d",
            foundConfigId.size());
    if (foundConfigId.empty()) {
        LOGE("Could not find any sensor config id - BUG");
        return nullptr;
    }
    /*
     * The size of this vector should be ideally 1, but in the future we will
     * have different sensor modes for the high-speed video. We should
     * know this at stream config time to filter them.
     * If there is more than one it means that there could be a potential change
     * of sensor mode for a new request.
     */
    if (type == CIO2) {
        if (mMediaCtlConfigs.mControlParams.size() < 1)
            return nullptr;
        return &mMediaCtlConfigs;
    } else {
        if (mImguMediaCtlConfigs.mVideoNodes.size() < 1)
            return nullptr;
        return &mImguMediaCtlConfigs;
    }
}

/**
 * Retrieve the previous media control configuration.
 */
const MediaCtlConfig* GraphConfigManager::getMediaCtlConfigPrev(IStreamConfigProvider::MediaType type) const
{
    if (type == CIO2) {
        if (mMediaCtlConfigsPrev.mControlParams.size() < 1)
            return nullptr;
        return &mMediaCtlConfigsPrev;
    } else {
        if (mImguMediaCtlConfigsPrev.mLinkParams.size() < 1)
            return nullptr;
        return &mImguMediaCtlConfigsPrev;
    }
}

std::shared_ptr<GraphConfig>
GraphConfigManager::getGraphConfig(Camera3Request &request)
{
    std::shared_ptr<GraphConfig> gc;
    status_t status = OK;

    status = mGraphConfigPool.acquireItem(gc);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to acquire GraphConfig from pool!!- BUG");
        return gc;
    }

    /*
     * Do second level query.
     *
     * TODO: Do it based on number of output buffers
     *
     * TODO 2: add intent and other constrains
     * Currently we just take the first result from the stream config query
     *
     *mGraphQueryManager->queryGraphs(mQuery,
                                    mFirstQueryResults,
                                    mSecondQueryResults);*/

    // Init graph config with the current request id
    gc->init(request.getId());

    detectActiveSinks(request, gc);
    return gc;
}

/**
 * Used at stream configuration time to get the base graph that covers all
 * the possible request outputs that we have. This is used for pipeline
 * initialization.
 */
std::shared_ptr<GraphConfig> GraphConfigManager::getBaseGraphConfig()
{
    std::shared_ptr<GraphConfig> gc;
    status_t status = OK;

    status = mGraphConfigPool.acquireItem(gc);
    if (CC_UNLIKELY(status != OK || gc.get() == nullptr)) {
        LOGE("Failed to acquire GraphConfig from pool!!- BUG");
        return gc;
    }
    gc->init(0);
    return gc;
}

/**
 * Analyze the request to get the active streams (the ones with buffer in this
 * request) and find the corresponding virtual sink id's (stored in the map
 * we create at stream config time).
 * Pass this list of active virtual sinks to the GraphConfig object so it can
 * determine with links are active.
 *
 * This is an intermediate step because we are re-using the same GC settings for
 * all request. Once this changes this step will be done inside the GC object
 * itself during init.
 *
 * \param[in] request
 * \param[out] gc GraphConfig object that we inform of the active sinks in the
 *                request
 */
void GraphConfigManager::detectActiveSinks(Camera3Request &request,
                                           std::shared_ptr<GraphConfig> gc)
{
    vector<uid_t> activeSinks;
    camera3_stream *stream = nullptr;

    const std::vector<camera3_stream_buffer>* outBufs = request.getOutputBuffers();
    if (CC_UNLIKELY(outBufs == nullptr)) {
        // This is impossible, but just to cover all cases
        LOGE("No output bufs in a request -- BUG");
        return;
    }

    for (size_t i = 0; i < outBufs->size(); i++) {
        stream = outBufs->at(i).stream;
        activeSinks.push_back(mStreamToSinkIdMap[stream]);
    }

    gc->setActiveSinks(activeSinks);
    gc->setActiveStreamId(activeSinks);
}
/******************************************************************************
 *  HELPER METHODS
 ******************************************************************************/
/**
 * Check the gralloc hint flags and decide whether this stream should be served
 * by Video Pipe or Still Pipe
 */
bool GraphConfigManager::isVideoStream(camera3_stream_t *stream)
{
    bool display = false;
    bool videoEnc = false;
    display = CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_COMPOSER);
    display |= CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_TEXTURE);
    display |= CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_RENDER);

    videoEnc = CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_VIDEO_ENCODER);

    return (display || videoEnc);
}

void GraphConfigManager::dumpStreamConfig(const vector<camera3_stream_t*> &streams)
{
    bool display = false;
    bool videoEnc = false;
    bool zsl = false;

    for (size_t i = 0; i < streams.size(); i++) {
        display = CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_COMPOSER);
        display |= CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_TEXTURE);
        display |= CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_RENDER);

        videoEnc = CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_VIDEO_ENCODER);
        zsl = CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_CAMERA_ZSL);

        LOGW("stream[%d] (%s): %dx%d, fmt %s, max buffers:%d, gralloc hints (0x%x) display:%s, video:%s, zsl:%s",
                i,
                METAID2STR(android_scaler_availableStreamConfigurations_values, streams[i]->stream_type),
                streams[i]->width, streams[i]->height,
                METAID2STR(android_scaler_availableFormats_values, streams[i]->format),
                streams[i]->max_buffers,
                streams[i]->usage,
                display? "YES":"NO",
                videoEnc? "YES":"NO",
                zsl? "YES":"NO");
    }
}

void GraphConfigManager::dumpQuery(const map<GCSS::ItemUID, std::string> &query)
{
    map<GCSS::ItemUID, std::string>::const_iterator it;
    it = query.begin();
    LOGW("Query Dump ------- Start");
    for(; it != query.end(); ++it) {
        LOGW("item: %s value %s", it->first.toString().c_str(),
                                  it->second.c_str());
    }
    LOGW("Query Dump ------- End");
}
}  // namespace camera2
}  // namespace android
