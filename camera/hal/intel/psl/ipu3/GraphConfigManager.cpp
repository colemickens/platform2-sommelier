/*
 * Copyright (C) 2015-2019 Intel Corporation
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

namespace cros {
namespace intel {

#define MIN_GRAPH_SETTING_STREAM 1
#define MAX_GRAPH_SETTING_STREAM 2
#define MAX_NUM_STREAMS    4
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
}

GraphConfigManager::GraphConfigManager(int32_t camId,
                                       GraphConfigNodes *testNodes) :
    mCameraId(camId),
    mGraphQueryManager(new GraphQueryManager()),
    mForceUseOneNodeInVideoPipe(false)
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

void GraphConfigManager::initVideoStreamConfigurations()
{
    mVideoStreamToSinkIdMap.clear();
    mVideoStreamResolutions.clear();
    mVideoQueryResults.clear();
    mQueryVideo.clear();

    // Will map streams in this order
    mVideoStreamKeys.clear();
    mVideoStreamKeys.push_back(GCSS_KEY_IMGU_VF);
    mVideoStreamKeys.push_back(GCSS_KEY_IMGU_MAIN);
    for (size_t i = 0; i < mVideoStreamKeys.size(); i++) {
        ItemUID w = {mVideoStreamKeys[i], GCSS_KEY_WIDTH};
        ItemUID h = {mVideoStreamKeys[i], GCSS_KEY_HEIGHT};
        mVideoStreamResolutions.push_back(std::make_pair(w,h));
    }
}

void GraphConfigManager::initStillStreamConfigurations()
{
    mStillStreamToSinkIdMap.clear();
    mStillStreamResolutions.clear();
    mStillQueryResults.clear();
    mQueryStill.clear();

    // Will map streams in this order
    mStillStreamKeys.clear();
    mStillStreamKeys.push_back(GCSS_KEY_IMGU_VF);
    mStillStreamKeys.push_back(GCSS_KEY_IMGU_MAIN);
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

status_t GraphConfigManager::sortStreamsByPipe(const std::vector<camera3_stream_t*> &streams,
                                               std::vector<camera3_stream_t*>* yuvStreams,
                                               std::vector<camera3_stream_t*>* blobStreams,
                                               int* repeatedStreamIndex)
{
    CheckError(yuvStreams == nullptr, BAD_VALUE, "yuvStreams is nullptr");
    CheckError(blobStreams == nullptr, BAD_VALUE, "blobStreams is nullptr");
    CheckError(repeatedStreamIndex == nullptr, BAD_VALUE, "repeatedStreamIndex is nullptr");

    *repeatedStreamIndex = -1;
    bool hasImpl = false;
    for (size_t i = 0; i < streams.size(); ++i) {
        if (streams[i]->stream_type == CAMERA3_STREAM_OUTPUT &&
            streams[i]->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            hasImpl = true;
            break;
        }
    }

    for (size_t i = 0; i < streams.size(); i++) {
        if ( streams[i]->stream_type != CAMERA3_STREAM_OUTPUT) {
            LOGW("@%s, stream[%lu] is not output, %d", __FUNCTION__, i, streams[i]->stream_type);
            continue;
        }

        switch (streams[i]->format) {
        case HAL_PIXEL_FORMAT_BLOB:
            blobStreams->push_back(streams[i]);
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            if (hasImpl &&
                streams[i]->width > RESOLUTION_1080P_WIDTH &&
                streams[i]->height > RESOLUTION_1080P_HEIGHT) {
                blobStreams->push_back(streams[i]);
            } else {
                if (isRepeatedStream(streams[i], *yuvStreams)) {
                    *repeatedStreamIndex = i;
                } else {
                    yuvStreams->push_back(streams[i]);
                }
            }
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            yuvStreams->push_back(streams[i]);
            break;
        default:
            LOGE("Unsupported stream format %d", streams[i]->format);
            return BAD_VALUE;
        }
    }

    return OK;
}

status_t GraphConfigManager::
mapVideoStreamToKey(const std::vector<camera3_stream_t*> &videoStreams, bool *hasVideoStream)
{
    CheckError(!hasVideoStream, UNKNOWN_ERROR, "hasVideoStream is nullptr");

    int yuvNum = videoStreams.size();
    LOG2("yuvNum:%d", yuvNum);
    if (videoStreams.size() == 0) {
        *hasVideoStream = false;
        return OK;
    }

    // store active output number for video pipe
    // if YUV stream number > 2. we only use the biggest two streams for video pipe
    if (yuvNum > MAX_GRAPH_SETTING_STREAM) {
        yuvNum = MAX_GRAPH_SETTING_STREAM;
    }

    if (mForceUseOneNodeInVideoPipe) {
        yuvNum = 1;
    }
    CheckError(yuvNum > MAX_GRAPH_SETTING_STREAM, UNKNOWN_ERROR,
               "yuvNum:%d > maxNum:%d", yuvNum, MAX_GRAPH_SETTING_STREAM);

    // store active output number for video pipe
    ItemUID streamCount = {GCSS_KEY_ACTIVE_OUTPUTS};
    mQueryVideo[streamCount] = std::to_string(yuvNum);

    // The main output port must >= vf output port.
    int mainOutputIndex = (yuvNum >= MAX_OUTPUT_NUM_IN_PIPE) ? 0 : -1;
    int vfOutputIndex = (yuvNum >= MAX_OUTPUT_NUM_IN_PIPE) ? 1 : 0;

    // map video stream settings
    ResolutionItem res;
    PlatformGraphConfigKey streamKey;
    handleVideoStream(res, streamKey);
    handleVideoMap(videoStreams[vfOutputIndex], res, streamKey);
    if (mainOutputIndex >= 0) {
        handleVideoStream(res, streamKey);
        handleVideoMap(videoStreams[mainOutputIndex], res, streamKey);
    }
    LOG2("video pipe: mainOutputIndex %d, vfOutputIndex %d", mainOutputIndex, vfOutputIndex);

    *hasVideoStream = true;

    return OK;
}

status_t GraphConfigManager::
mapStillStreamToKey(const std::vector<camera3_stream_t*> &stillStreams, bool *hasStillStream)
{
    CheckError(!hasStillStream, UNKNOWN_ERROR, "hasStillStream is nullptr");

    int blobNum = stillStreams.size();
    LOG2("blobNum:%d", blobNum);
    if (blobNum == 0) {
        return OK;
    }

    // store active output number for still pipe
    ItemUID streamCount = {GCSS_KEY_ACTIVE_OUTPUTS};
    mQueryStill[streamCount] = std::to_string(MIN_GRAPH_SETTING_STREAM);

    // map still stream settings
    ResolutionItem res;
    PlatformGraphConfigKey streamKey;
    handleStillStream(res, streamKey);
    handleStillMap(stillStreams[0], res, streamKey);
    LOG2("still pipe: %p", stillStreams[0]);

    *hasStillStream = true;

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

status_t GraphConfigManager::getCsiBeOutput(GCSS::GraphConfigNode &queryResult,
                                            std::map<camera3_stream_t*, uid_t> &streamToSinkIdMap,
                                            bool enableStill, CsiBeOutput *output)
{
    GraphConfigNode result = {};
    MediaType type = enableStill ? IMGU_STILL : IMGU_VIDEO;
    std::shared_ptr<GraphConfig> graph = mGraphConfigMap[type];
    CheckError(graph == nullptr, UNKNOWN_ERROR, "Graph config is nullptr");

    css_err_t ret = mGraphQueryManager->getGraph(&queryResult, &result);
    if (ret != css_err_none) {
        LOGE("failed to get the graph config");
        graph.reset();
        return UNKNOWN_ERROR;
    }
    status_t status = graph->prepare(&result, streamToSinkIdMap);
    CheckError(status != OK, UNKNOWN_ERROR, "failed to prepare graph config");

    status = graph->graphGetDimensionsByName(CSI_BE_OUTPUT, output->width, output->height);
    CheckError(status != OK, UNKNOWN_ERROR, "Cannot find <%s> node", CSI_BE_OUTPUT);

    return status;
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
    std::vector<CsiBeOutput> videoCsiOutput, stillCsiOutput;
    int32_t id = 0;

    LOG2("Find csi be output setting of video pipe, query result: %ld", mVideoQueryResults.size());
    for (size_t i = 0; i < mVideoQueryResults.size(); i++) {
        string streamType;
        CsiBeOutput output = {};

        CheckError(mVideoQueryResults[i] == nullptr, UNKNOWN_ERROR, "the video query result %zu is nullptr", i);
        status = getCsiBeOutput(*mVideoQueryResults[i], mVideoStreamToSinkIdMap, false, &output);
        CheckError(status != OK, UNKNOWN_ERROR, "Couldn't get csi BE output for video pipe");

        mVideoQueryResults[i]->getValue(GCSS_KEY_STREAM_TYPE, streamType);
        if (streamType.compare("video") && streamType.compare("both")) {
            continue;
        }

        output.index = i;
        mVideoQueryResults[i]->getValue(GCSS_KEY_KEY, id);
        LOG2("setting id: %d, video pipe csi be output width: %d, height: %d", id, output.width, output.height);
        videoCsiOutput.push_back(output);
    }

    LOG2("Find csi be output setting of still pipe, query result: %ld", mStillQueryResults.size());
    for (size_t i = 0; i < mStillQueryResults.size(); i++) {
        string streamType;
        CsiBeOutput output = {};

        CheckError(mStillQueryResults[i] == nullptr, UNKNOWN_ERROR, "the still query result %zu is nullptr", i);
        status = getCsiBeOutput(*mStillQueryResults[i], mStillStreamToSinkIdMap, true, &output);
        CheckError(status != OK, UNKNOWN_ERROR, "Couldn't get csi BE output for still pipe");

        mStillQueryResults[i]->getValue(GCSS_KEY_STREAM_TYPE, streamType);
        if (streamType.compare("still") && streamType.compare("both")) {
            continue;
        }

        output.index = i;
        mStillQueryResults[i]->getValue(GCSS_KEY_KEY, id);
        LOG2("setting id: %d, still pipe csi be output width: %d, height: %d", id, output.width, output.height);
        stillCsiOutput.push_back(output);
    }

    if (videoCsiOutput.empty() || stillCsiOutput.empty()) {
        *videoResultIdx = videoCsiOutput.empty() ? *videoResultIdx : videoCsiOutput[0].index;
        *stillResultIdx = stillCsiOutput.empty() ? *stillResultIdx : stillCsiOutput[0].index;
    } else {
        bool matchFound = false;
        for (size_t i = 0; i < videoCsiOutput.size(); i++) {
            for (size_t j = 0; j < stillCsiOutput.size(); j++) {
                if (videoCsiOutput[i].width == stillCsiOutput[j].width &&
                    videoCsiOutput[i].height == stillCsiOutput[j].height) {
                    LOG2("Find match csi be resolution, width: %d height: %d",
                         videoCsiOutput[i].width, videoCsiOutput[i].height);
                    *videoResultIdx = videoCsiOutput[i].index;
                    *stillResultIdx = stillCsiOutput[j].index;
                    matchFound = true;
                    break;
                }
            }
            if (matchFound)
                break;
        }

        CheckError(*videoResultIdx < 0 && *stillResultIdx < 0,
                   UNKNOWN_ERROR, "Failed to find match csi be resolution!");
    }

    if (*videoResultIdx >= 0) {
        mVideoQueryResults[*videoResultIdx]->getValue(GCSS_KEY_KEY, id);
        LOG1("Video graph config settings id %d", id);
    }

    if (*stillResultIdx >= 0) {
        mStillQueryResults[*stillResultIdx]->getValue(GCSS_KEY_KEY, id);
        LOG1("Still graph config settings id %d", id);
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

    if (streams.size() > MAX_NUM_STREAMS) {
        LOGE("Maximum number of streams %u exceeded: %zu", MAX_NUM_STREAMS, streams.size());
        return BAD_VALUE;
    }

    int repeatedStreamIndex = -1;
    std::vector<camera3_stream_t *> videoStreams;
    std::vector<camera3_stream_t *> stillStreams;
    status_t ret = sortStreamsByPipe(streams, &videoStreams, &stillStreams, &repeatedStreamIndex);
    CheckError(ret != OK, ret, "Sort streams failed %d", ret);

    mGraphConfigMap.clear();

    // for video pipe
    // If the graph cfg fails, let's try to cfg the 2nd time with just using only one node.
    mForceUseOneNodeInVideoPipe = false;
    bool hasVideoStream = false;
    do {
        initVideoStreamConfigurations();
        mapVideoStreamToKey(videoStreams, &hasVideoStream);
        if (!hasVideoStream) {
            break;
        }

        ret = queryVideoGraphSettings();
        if (ret == OK) {
            break;
        }

        // case 1: first time fails, and it just has one stream.
        // case 2: second time fails.
        CheckError(videoStreams.size() == 1 || mForceUseOneNodeInVideoPipe,
                   ret, "ret:%d, queryVideoGraphSettings fails", ret);

        LOGW("queryVideoGraphSettings fails, try again with one node enabled");
        mForceUseOneNodeInVideoPipe = true;
    } while (1);
    if (hasVideoStream) {
        mGraphConfigMap[IMGU_VIDEO] = std::make_shared<GraphConfig>();
        mVideoGraphResult = std::unique_ptr<GraphConfigNode>(new GraphConfigNode);
    }

    // for still pipe
    initStillStreamConfigurations();
    bool hasStillStream = false;
    mapStillStreamToKey(stillStreams, &hasStillStream);
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
        LOG2("get media control config for %s pipe", isVideoPipe ? "video" : "still");
        std::shared_ptr<GraphConfig> gc = it.second;

        gc->setMediaCtlConfig(mMediaCtl, !isVideoPipe);

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
        /*
         * select graph config for CIO2 in mGraphConfigMap
         * graph config from either video/still pipe is workable
         * as they have the same CIO2 graph config
         */
        gc = mGraphConfigMap.begin()->second;
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
}  // namespace intel
}  // namespace cros
