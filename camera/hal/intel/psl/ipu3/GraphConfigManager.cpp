/*
 * Copyright (C) 2015-2018 Intel Corporation
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

namespace android {
namespace camera2 {

#define MIN_GRAPH_SETTING_STREAM 1
#define MAX_GRAPH_SETTING_STREAM 2
#define MAX_NUM_STREAMS    4
const char *GraphConfigManager::DEFAULT_DESCRIPTOR_FILE = "/etc/camera/graph_descriptor.xml";
const char *GraphConfigManager::DEFAULT_SETTINGS_FILE = "/etc/camera/graph_settings.xml";

// save csi be output resolution settings
struct csi_be_output_res {
    int width;
    int height;
};

GraphConfigNodes::GraphConfigNodes() :
        mDesc(nullptr),
        mSettings(nullptr)
{
}

GraphConfigNodes::~GraphConfigNodes()
{
    delete mDesc;
}

GraphConfigManager::GraphConfigManager(int32_t camId,
                                       GraphConfigNodes *testNodes) :
    mCameraId(camId),
    mGraphQueryManager(new GraphQueryManager()),
    mNeedSwapVideoPreview(false),
    mNeedSwapStillPreview(false)
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
}

/**
 * This is a helper member to store the ItemUID's for the width and height of
 * each stream. It also clear all the vector/map that saves GraphConfig settings.
 * This function needs to be called when reconfiguration is needed.
 */
void GraphConfigManager::initStreamConfigurations()
{
    mVideoStreamToSinkIdMap.clear();
    mStillStreamToSinkIdMap.clear();
    mVideoStreamResolutions.clear();
    mStillStreamResolutions.clear();
    mVideoQueryResults.clear();
    mStillQueryResults.clear();
    mVideoStreamKeys.clear();
    mStillStreamKeys.clear();
    mQueryVideo.clear();
    mQueryStill.clear();

    mGraphConfigMap.clear();

    // Will map streams in this order
    mVideoStreamKeys.push_back(GCSS_KEY_IMGU_VIDEO);
    mVideoStreamKeys.push_back(GCSS_KEY_IMGU_PREVIEW);
    mStillStreamKeys.push_back(GCSS_KEY_IMGU_STILL);
    mStillStreamKeys.push_back(GCSS_KEY_IMGU_PREVIEW);

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

    LOG1("Adding %zu android specific keys to graph config parser",
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
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
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

bool GraphConfigManager::needSwapVideoPreview(GCSS::GraphConfigNode* graphCfgNode, int32_t id)
{
    bool swapVideoPreview = false;
    int previewWidth = 0;
    int previewHeight = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    status_t ret1 = OK;
    status_t ret2 = OK;

    GraphConfigNode* node = nullptr;
    std::string nodeName = GC_PREVIEW;
    graphCfgNode->getDescendantByString(nodeName, &node);
    if (node) {
        ret1 = node->getValue(GCSS_KEY_WIDTH, previewWidth);
        ret2 = node->getValue(GCSS_KEY_HEIGHT, previewHeight);
        if (ret1 != OK || ret2 != OK) {
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
        if (ret1 != OK || ret2 != OK) {
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
    LOG2("@%s :%d", __FUNCTION__, swapVideoPreview);

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

void GraphConfigManager::handleVideoMap(camera3_stream_t* stream, ResolutionItem& res, PlatformGraphConfigKey& streamKey)
{
    LOG1("Adding video stream %p to map %s", stream, ItemUID::key2str(streamKey));
    mVideoStreamToSinkIdMap[stream] = streamKey;

    ItemUID w = res.first;
    ItemUID h = res.second;
    bool rotate = stream->stream_type == CAMERA3_STREAM_OUTPUT &&
                  (stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_90
                   || stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_270);
    mQueryVideo[w] = std::to_string(rotate ? stream->height : stream->width);
    mQueryVideo[h] = std::to_string(rotate ? stream->width : stream->height);
}

void GraphConfigManager::handleStillMap(camera3_stream_t* stream, ResolutionItem& res, PlatformGraphConfigKey& streamKey)
{
    LOG1("Adding still stream %p to map %s", stream, ItemUID::key2str(streamKey));
    mStillStreamToSinkIdMap[stream] = streamKey;

    ItemUID w = res.first;
    ItemUID h = res.second;
    bool rotate = stream->stream_type == CAMERA3_STREAM_OUTPUT &&
                  (stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_90
                   || stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_270);
    mQueryStill[w] = std::to_string(rotate ? stream->height : stream->width);
    mQueryStill[h] = std::to_string(rotate ? stream->width : stream->height);
}

bool GraphConfigManager::isRepeatedStream(camera3_stream_t* curStream,
                                            const std::vector<camera3_stream_t*> &streams)
{
    //The streams are already sorted by dimensions in IPU3CameraHw.
    if (!streams.empty() &&
        curStream->width == streams.back()->width &&
        curStream->height == streams.back()->height &&
        curStream->format == streams.back()->format &&
        curStream->usage == streams.back()->usage) {
        LOG1("%dx%d(fmt:%s) is a repeating stream.",curStream->width,curStream->height,
        METAID2STR(android_scaler_availableFormats_values, curStream->format));
        return true;
    }
    return false;
}

status_t GraphConfigManager::mapStreamToKey(const std::vector<camera3_stream_t*> &streams,
                                            int *hasVideoStream, int *hasStillStream)
{
    std::vector<camera3_stream_t *> videoStreams, stillStream;
    ItemUID streamCount = {GCSS_KEY_ACTIVE_OUTPUTS};
    int yuvNum = 0;
    int blobNum = 0;

    bool hasIMPL = false;
    for (size_t i = 0; i < streams.size(); ++i) {
        if (streams[i]->stream_type == CAMERA3_STREAM_OUTPUT &&
            streams[i]->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            hasIMPL = true;
            break;
        }
    }

    for (size_t i = 0; i < streams.size(); i++) {
        if ( streams[i]->stream_type != CAMERA3_STREAM_OUTPUT) {
            LOGE("@%s, stream[%lu] is not output, %d", __FUNCTION__, i, streams[i]->stream_type);
            return UNKNOWN_ERROR;
        }

        if (streams[i]->format == HAL_PIXEL_FORMAT_BLOB) {
            stillStream.push_back(streams[i]);
            *hasStillStream = true;
            blobNum++;
        } else if (streams[i]->format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
            if (hasIMPL &&
                streams[i]->width > RESOLUTION_1080P_WIDTH &&
                streams[i]->height > RESOLUTION_1080P_HEIGHT) {
                stillStream.push_back(streams[i]);
                *hasStillStream = true;
                blobNum++;
            } else if (!isRepeatedStream(streams[i], videoStreams)) {
                videoStreams.push_back(streams[i]);
                *hasVideoStream = true;
                yuvNum++;
            }
        } else if (streams[i]->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            videoStreams.push_back(streams[i]);
            *hasVideoStream = true;
            yuvNum++;
        } else {
            LOGE("Unsupported stream format %d", streams.at(i)->format);
            return BAD_VALUE;
        }
    }

    LOG2("@%s, blobNum:%d, yuvNum:%d", __FUNCTION__, blobNum, yuvNum);

    PlatformGraphConfigKey streamKey;
    ResolutionItem res;

    // map video stream settings
    if (*hasVideoStream) {
        // store active output number for video pipe
        CheckError(yuvNum > MAX_GRAPH_SETTING_STREAM,
            UNKNOWN_ERROR, "yuv stream number out of range: %d", yuvNum);
        mQueryVideo[streamCount] = std::to_string(yuvNum);

        // Main output produces full size frames
        // Secondary output produces small size frames
        int mainOutputIndex = 0;
        int secondaryOutputIndex = (videoStreams.size() == 2) ? 1 : -1;

        // map video stream settings
        handleVideoStream(res, streamKey);
        handleVideoMap(videoStreams[mainOutputIndex], res, streamKey);
        if (secondaryOutputIndex >= 0) {
            handleVideoStream(res, streamKey);
            handleVideoMap(videoStreams[secondaryOutputIndex], res, streamKey);
        }
        LOG2("@%s, video pipe: mainOutput %p, secondaryOutput %p", __func__,
            videoStreams[mainOutputIndex], videoStreams[secondaryOutputIndex]);
    }

    // map still stream settings
    if (*hasStillStream) {
        // store active output number for still pipe
        mQueryStill[streamCount] = std::to_string(MIN_GRAPH_SETTING_STREAM);

        CLEAR(streamKey);
        CLEAR(res);
        handleStillStream(res, streamKey);
        handleStillMap(stillStream[0], res, streamKey);
        LOG2("@%s, still pipe: %p", __func__, stillStream[0]);
    }

    return OK;
}

status_t GraphConfigManager::queryVideoGraphSettings()
{
    mGraphQueryManager->queryGraphs(mQueryVideo, mVideoQueryResults);
    if (mVideoQueryResults.empty()) {
        LOGE("Can't find fitting graph settings");
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t GraphConfigManager::queryStillGraphSettings()
{
    mGraphQueryManager->queryGraphs(mQueryStill, mStillQueryResults);
    if (mStillQueryResults.empty()) {
        LOGE("Failed to retrieve default settings");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Graph settings of both video pipe and still pipe must have the same cio2 settings,
 * and there might be serveral sets of graph settings for both pipes, need to find the match
 * settings with common cio2 configure
 */
status_t GraphConfigManager::matchQueryResultByCsiSetting(int *videoResultIdx,
                                                           int *stillResultIdx)
{
    status_t status = NO_ERROR;
    std::string csiName = "csi_be:output";
    std::vector<csi_be_output_res> videoCsiOutRes, stillCsiOutRes;
    int32_t id = 0;

    LOG2("@%s, find csi be output setting of video pipe, query result: %ld",
        __func__, mVideoQueryResults.size());
    for (auto& videoResultIter : mVideoQueryResults) {
        csi_be_output_res res = {};
        GraphConfigNode result = {};
        std::shared_ptr<GraphConfig> videoGc = mGraphConfigMap[IMGU_VIDEO];
        CheckError(videoGc.get() == nullptr, UNKNOWN_ERROR, "Video graph config is nullptr");

        css_err_t ret = mGraphQueryManager->getGraph(videoResultIter, &result);
        if (ret != css_err_none) {
            videoGc.reset();
            return UNKNOWN_ERROR;
        }

        status = videoGc->prepare(&result, mVideoStreamToSinkIdMap);
        CheckError(status != OK, UNKNOWN_ERROR, "failed to compare graph config for video pipe");

        status = videoGc->graphGetDimensionsByName(csiName, res.width, res.height);
        CheckError(status != OK, UNKNOWN_ERROR, "Cannot find <%s> node", csiName.c_str());

        videoResultIter->getValue(GCSS_KEY_KEY, id);
        LOG2("@%s, setting id: %d, video pipe csi be output width: %d, height: %d",
            __func__, id, res.width, res.height);
        videoCsiOutRes.push_back(res);
    }

    LOG2("@%s, find csi be output setting of still pipe, query result: %ld",
        __func__, mStillQueryResults.size());
    for (auto& stillResultIter : mStillQueryResults) {
        csi_be_output_res res = {};
        GraphConfigNode result = {};
        std::shared_ptr<GraphConfig> stillGc = mGraphConfigMap[IMGU_STILL];
        CheckError(stillGc.get() == nullptr, UNKNOWN_ERROR, "Still graph config is nullptr");

        css_err_t ret = mGraphQueryManager->getGraph(stillResultIter, &result);
        if (ret != css_err_none) {
            stillGc.reset();
            return UNKNOWN_ERROR;
        }

        status = stillGc->prepare(&result, mStillStreamToSinkIdMap);
        CheckError(status != OK, UNKNOWN_ERROR, "failed to compare graph config for still pipe");

        status = stillGc->graphGetDimensionsByName(csiName, res.width, res.height);
        CheckError(status != OK, UNKNOWN_ERROR, "Cannot find <%s> node", csiName.c_str());

        stillResultIter->getValue(GCSS_KEY_KEY, id);
        LOG2("@%s, setting id: %d, still pipe csi be output width: %d, height: %d",
            __func__, id, res.width, res.height);
        stillCsiOutRes.push_back(res);
    }

    if (videoCsiOutRes.empty() || stillCsiOutRes.empty()) {
        *videoResultIdx = videoCsiOutRes.empty() ? *videoResultIdx : 0;
        *stillResultIdx = stillCsiOutRes.empty() ? *stillResultIdx : 0;
    } else {
        bool matchFound = false;
        for (size_t i = 0; i < videoCsiOutRes.size(); i++) {
            for (size_t j = 0; j < stillCsiOutRes.size(); j++) {
                if (videoCsiOutRes[i].width == stillCsiOutRes[j].width &&
                    videoCsiOutRes[i].height == stillCsiOutRes[j].height) {
                    LOG2("@%s, find match csi be resolution, width: %d height: %d",
                        __func__, videoCsiOutRes[i].width, videoCsiOutRes[i].height);
                    *videoResultIdx = i;
                    *stillResultIdx = j;
                    matchFound = true;
                    break;
                }
            }
            if (matchFound)
                break;
        }

        if (*videoResultIdx < 0 && *stillResultIdx < 0) {
            LOGE("@%s, failed to find match csi be resolution!", __func__);
            return UNKNOWN_ERROR;
        }
    }

    if (*videoResultIdx >= 0) {
        mVideoQueryResults[*videoResultIdx]->getValue(GCSS_KEY_KEY, id);
        LOG1("@%s, Video graph config settings id %d", __func__, id);
        mNeedSwapVideoPreview = needSwapVideoPreview(mVideoQueryResults[*videoResultIdx], id);
    }

    if (*stillResultIdx >= 0) {
        mStillQueryResults[*stillResultIdx]->getValue(GCSS_KEY_KEY, id);
        LOG1("@%s, Still graph config settings id %d", __func__, id);
    }

    return status;
}

/**
 * Initialize the state of the GraphConfigManager after parsing the stream
 * configuration.
 * Perform the first level query to find a subset of settings that fulfill the
 * constrains from the stream configuration.
 */
status_t GraphConfigManager::configStreams(const vector<camera3_stream_t*> &streams,
                                           uint32_t operationMode,
                                           int32_t testPatternMode)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    HAL_KPI_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, 1000000); /* 1 ms*/
    UNUSED(operationMode);
    ResolutionItem res;
    int hasVideoStream = false, hasStillStream = false;
    status_t ret = OK;

    if (streams.size() > MAX_NUM_STREAMS) {
        LOGE("Maximum number of streams %u exceeded: %zu",
            MAX_NUM_STREAMS, streams.size());
        return BAD_VALUE;
    }

    initStreamConfigurations();

    ret = mapStreamToKey(streams, &hasVideoStream, &hasStillStream);
    CheckError(ret != OK, ret, "@%s, call mapStreamToKey fail, ret:%d", __func__, ret);

    if (hasVideoStream) {
        ret = queryVideoGraphSettings();
        CheckError(ret != OK, ret, "@%s, Failed to query graph settings for video pipe", __func__);
        mGraphConfigMap[IMGU_VIDEO] = std::make_shared<GraphConfig>();
        mVideoGraphResult = std::unique_ptr<GraphConfigNode>(new GraphConfigNode);
    }
    if (hasStillStream) {
        ret = queryStillGraphSettings();
        CheckError(ret != OK, ret, "@%s, Failed to query graph settings for still pipe", __func__);
        mGraphConfigMap[IMGU_STILL] = std::make_shared<GraphConfig>();
        mStillGraphResult = std::unique_ptr<GraphConfigNode>(new GraphConfigNode);
    }
    dumpStreamConfig(streams); // TODO: remove this when GC integration is done

    ret = prepareGraphConfig();
    CheckError(ret != OK, UNKNOWN_ERROR, "Failed to prepare graph config");

    ret = prepareMediaCtlConfig(testPatternMode);
    CheckError(ret != OK, UNKNOWN_ERROR, "failed to prepare media control config");

    return OK;
}

/**
 * Prepare graph config object
 *
 * Use graph query results as a parameter to getGraph. The result will be given
 * to graph config object.
 */
status_t GraphConfigManager::prepareGraphConfig()
{
    status_t status = OK;
    LOG2("@%s, graph config size: %ld", __func__, mGraphConfigMap.size());

    int videoResultIndex = -1, stillResultIndex = -1;
    status = matchQueryResultByCsiSetting(&videoResultIndex, &stillResultIndex);
    CheckError(status != NO_ERROR, status, "failed to find match query result by csi be settings");

    for (auto& it : mGraphConfigMap) {
        bool isVideoPipe = (it.first == IMGU_VIDEO) ? true : false;
        std::shared_ptr<GraphConfig> gc = it.second;
        CheckError(gc.get() == nullptr, UNKNOWN_ERROR, "Graph config is nullptr");

        GCSS::GraphConfigNode *queryResult = isVideoPipe ?
                     mVideoQueryResults[videoResultIndex] : mStillQueryResults[stillResultIndex];
        std::map<camera3_stream_t*, uid_t> streamToSinkIdMap = isVideoPipe ?
                                     mVideoStreamToSinkIdMap : mStillStreamToSinkIdMap;
        std::map<GCSS::ItemUID, std::string> query = isVideoPipe ?
                                       mQueryVideo : mQueryStill;
        GraphConfigNode *result = isVideoPipe ?
            mVideoGraphResult.get() : mStillGraphResult.get();

        css_err_t ret = mGraphQueryManager->getGraph(queryResult, result);
        if (CC_UNLIKELY(ret != css_err_none)) {
           LOGE("Failed to get graph from graph query manager for %s pipe", isVideoPipe ? "video" : "still");
           gc.reset();
           return UNKNOWN_ERROR;
        }

        status = gc->prepare(result, streamToSinkIdMap);
        if (status != OK) {
            LOGE("Failed to compare graph config for %s pipe", isVideoPipe ? "video" : "still");
            dumpQuery(query);
            return UNKNOWN_ERROR;
        }
    }

    LOG1("Graph config object prepared");

    return status;
}

status_t GraphConfigManager::prepareMediaCtlConfig(int32_t testPatternMode)
{
    status_t status = OK;
    // both imgu should share the same cio2 pixel array format
    int cio2Format = 0;
    // CIO2 media control data only need to save once
    bool isCIO2MediaCtlConfiged = false;
    LOG2("@%s, graph config size: %ld", __func__, mGraphConfigMap.size());

    // clear media control configs
    for (size_t i = 0; i < MEDIA_TYPE_MAX_COUNT; i++) {
        // Reset old values
        mMediaCtlConfigs[i].mLinkParams.clear();
        mMediaCtlConfigs[i].mFormatParams.clear();
        mMediaCtlConfigs[i].mSelectionParams.clear();
        mMediaCtlConfigs[i].mSelectionVideoParams.clear();
        mMediaCtlConfigs[i].mControlParams.clear();
        mMediaCtlConfigs[i].mVideoNodes.clear();
    }

    // save media control data into mMediaCtlConfigs
    for (auto& it : mGraphConfigMap) {
        MediaType type = it.first;
        bool isVideoPipe = (type == IMGU_VIDEO) ? true : false;
        bool swapOutput = isVideoPipe ? mNeedSwapVideoPreview : mNeedSwapStillPreview;
        LOG2("get media control config for %s pipe", isVideoPipe ? "video" : "still");
        std::shared_ptr<GraphConfig> gc = it.second;

        gc->setMediaCtlConfig(mMediaCtl, swapOutput, !isVideoPipe);

        if (!isCIO2MediaCtlConfiged) {
            status = gc->getCio2MediaCtlData(&cio2Format, &mMediaCtlConfigs[CIO2]);
            CheckError(status != OK, status, "Couldn't get mediaCtl data");
            isCIO2MediaCtlConfiged = true;
        }

        status = gc->getImguMediaCtlData(mCameraId,
                                         cio2Format,
                                         testPatternMode,
                                         !isVideoPipe,
                                         &mMediaCtlConfigs[type]);
        CheckError(status != OK, status, "Couldn't get Imgu mediaCtl data for %s pipe",
                   isVideoPipe ? "video" : "still");
    }

    return status;
}


/**
 * Retrieve the current active media controller configuration for Sensor + ISA by Mediatype
 *
 */
const MediaCtlConfig* GraphConfigManager::getMediaCtlConfig(IStreamConfigProvider::MediaType type) const
{
    if (type >= MEDIA_TYPE_MAX_COUNT) {
        return nullptr;
    }

    if (type == CIO2) {
        if (mMediaCtlConfigs[type].mControlParams.size() < 1) {
            return nullptr;
        }
    } else if (mMediaCtlConfigs[type].mLinkParams.size() < 1) {
        return nullptr;
    }
    return &mMediaCtlConfigs[type];
}

/**
 * Used at stream configuration time to get the base graph that covers all
 * the possible request outputs that we have. This is used for pipeline
 * initialization.
 */
std::shared_ptr<GraphConfig> GraphConfigManager::getBaseGraphConfig(MediaType type)
{
    CheckError(mGraphConfigMap.empty(), nullptr, "@%s, no valid graph config found", __func__);
    std::shared_ptr<GraphConfig> gc = nullptr;

    if (type == CIO2) {
        // select graph config for CIO2 in mGraphConfigMap
        for (auto& it : mGraphConfigMap) {
            /* graph config from either video/still pipe is workable
             * as they have the same CIO2 graph config */
            gc = it.second;
            break;
        }
    } else if (type == IMGU_VIDEO || type == IMGU_STILL) {
        gc = mGraphConfigMap[type];
    } else {
        LOGE("@%s, not a valid media type: %d", __func__, type);
        return nullptr;
    }

    CheckError(gc.get() == nullptr, nullptr, "Failed to acquire GraphConfig!!- BUG");

    gc->init(0);
    return gc;
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

        LOGW("stream[%zu] (%s): %dx%d, fmt %s, max buffers:%d, gralloc hints (0x%x) display:%s, video:%s, zsl:%s",
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
