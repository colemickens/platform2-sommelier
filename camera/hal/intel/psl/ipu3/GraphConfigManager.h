/*
 * Copyright (C) 2017 Intel Corporation
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

#ifndef _CAMERA3_GRAPHCONFIGMANAGER_H_
#define _CAMERA3_GRAPHCONFIGMANAGER_H_

#include <gcss.h>
#include <hardware/camera3.h>
#include <memory>
#include <utility>
#include <vector>
#include "MediaCtlPipeConfig.h"
#include "MediaController.h"


namespace GCSS {
class GraphConfigNode;
class GraphQueryManager;
class ItemUID;
}

namespace android {
namespace camera2 {

/**
 * \enum PlatformGraphConfigKey
 * List of keys that are Android Specific used in queries of settings by
 * the GraphConfigManager.
 *
 * The enum should not overlap with the enum of tags already predefined by the
 * parser, hence the initial offset.
 */
#define GCSS_KEY(key, str) GCSS_KEY_##key,
enum PlatformGraphConfigKey {
    GCSS_ANDROID_KEY_START = GCSS_KEY_START_CUSTOM_KEYS,
    #include "platform_gcss_keys.h"
    #include "ipu3_android_gcss_keys.h"
};
#undef GCSS_KEY

class Camera3Request;
class GraphConfig;

/**
 * Static data for graph settings for given sensor. Used to initialize
 * \class GraphConfigManager.
 */
class GraphConfigNodes
{
public:
    ~GraphConfigNodes();

private:
    // Private constructor, only initialized by GCM
    friend class GraphConfigManager;
    GraphConfigNodes();

    // Disable copy constructor and assignment operator
    GraphConfigNodes(const GraphConfigNodes &);
    GraphConfigNodes& operator=(const GraphConfigNodes &);

private:
    GCSS::GraphConfigNode *mDesc;
    GCSS::GraphConfigNode *mSettings;
};

/**
 * \interface IStreamConfigProvider
 *
 * This interface is used to expose the GraphConfig settings selected at stream
 * config time. At the moment only exposes the MediaController configuration.
 *
 * It is used by the 3 units (Ctrl, Capture and Processing).
 * At the moment it is implemented by the GraphConfigManager
 *
 *  TODO: Expose a full GraphConfig Object selected.
 */
class IStreamConfigProvider {
public:
    enum MediaType {
        CIO2 = 0,
        IMGU_VIDEO,
        IMGU_STILL,

        MEDIA_TYPE_MAX_COUNT
    };

    virtual ~IStreamConfigProvider() { };
    virtual const MediaCtlConfig *getMediaCtlConfig(MediaType type) const = 0;
    virtual std::shared_ptr<GraphConfig> getBaseGraphConfig(MediaType type) = 0;
};

/**
 * \class GraphConfigManager
 *
 * Class to wrap over parsing and executing queries on graph settings.
 * GraphConfigManager owns the interface towards GCSS and provides convenience
 * for HAL to execute queries and it generates GraphConfig objects as results.
 *
 * GraphConfigManager also provides static method for parsing graph descriptor
 * and graph settings from XML files and filtering that data based on sensor.
 * The \class GraphConfigmanager::Nodes object is stored in CameraCapInfo and
 * is used when instantiating GCM.
 *
 * At camera open, GraphConfigManager object is created.
 * At stream config time the state of GraphConfig manager changes with the
 * result of the first query. This is the possible subset of graph settings that
 * can fulfill the requirements of requested streams.
 * At this point, there may be more than one options, but
 * GCM can always return some default settings.
 *
 * Per each request, GraphConfigManager creates GraphConfig objects based
 * on request content. These objects are owned by GCM in a pool, and passed
 * around HAL via shared pointers.
 */
class GraphConfigManager: public IStreamConfigProvider
{
public:
    GraphConfigManager(int32_t camId, GraphConfigNodes *nodes = nullptr);
    virtual ~GraphConfigManager();
    /*
     * static methods for XML parsing
     */
    static void addAndroidMap();
    static GraphConfigNodes* parse(const char *descriptorXmlFile,
                                   const char *settingsXmlFile);

    void setMediaCtl(std::shared_ptr<MediaController> mediaCtl) { mMediaCtl = mediaCtl; }
    /*
     * First Query
     */
    status_t configStreams(const std::vector<camera3_stream_t*> &activeStreams,
                           uint32_t operationMode,
                           int32_t testPatternMode);
    /*
     * Implementation of IStreamConfigProvider
     */
    const MediaCtlConfig *getMediaCtlConfig(MediaType type) const;
    std::shared_ptr<GraphConfig> getBaseGraphConfig(MediaType type);

public:
    static const char *DEFAULT_DESCRIPTOR_FILE;
    static const char *DEFAULT_SETTINGS_FILE;
    static const int32_t MAX_REQ_IN_FLIGHT = 10;
    const int32_t mCameraId;

private: /* Types */
    /**
     * Pair of ItemUIDs to store the width and height of a stream
     * first item is for width, second for height
     */
    typedef std::pair<GCSS::ItemUID, GCSS::ItemUID> ResolutionItem;

private:
    // Disable copy constructor and assignment operator
    GraphConfigManager(const GraphConfigManager &);
    GraphConfigManager& operator=(const GraphConfigManager &);
    void initStreamConfigurations();

    // Debuging helpers
    void dumpStreamConfig(const std::vector<camera3_stream_t*> &streams);
    void dumpQuery(const std::map<GCSS::ItemUID, std::string> &query);
    status_t prepareGraphConfig();
    status_t prepareMediaCtlConfig(int32_t testPatternMode);
    bool needSwapVideoPreview(GCSS::GraphConfigNode* graphCfgNode, int32_t id);

    void handleVideoStream(ResolutionItem& res, PlatformGraphConfigKey& streamKey);
    void handleStillStream(ResolutionItem& res, PlatformGraphConfigKey& streamKey);
    void handleVideoMap(camera3_stream_t* stream, ResolutionItem& res, PlatformGraphConfigKey& streamKey);
    void handleStillMap(camera3_stream_t* stream, ResolutionItem& res, PlatformGraphConfigKey& streamKey);
    bool isRepeatedStream(camera3_stream_t* curStream, const std::vector<camera3_stream_t*> &streams);
    status_t mapStreamToKey(const std::vector<camera3_stream_t*> &streams,
                            int *hasVideoStream, int *hasStillStream);
    status_t queryVideoGraphSettings();
    status_t queryStillGraphSettings();

    status_t matchQueryResultByCsiSetting(int *videoResultIdx, int *stillResultIdx);

private:
    std::unique_ptr<GCSS::GraphQueryManager> mGraphQueryManager;

    std::map<GCSS::ItemUID, std::string> mQueryVideo;
    std::map<GCSS::ItemUID, std::string> mQueryStill;
    std::vector<GCSS::GraphConfigNode*> mVideoQueryResults;
    std::vector<GCSS::GraphConfigNode*> mStillQueryResults;

    bool mNeedSwapVideoPreview;
    bool mNeedSwapStillPreview;

    std::vector<PlatformGraphConfigKey> mVideoStreamKeys;
    std::vector<PlatformGraphConfigKey> mStillStreamKeys;
    std::vector<ResolutionItem> mVideoStreamResolutions;
    std::vector<ResolutionItem> mStillStreamResolutions;

    std::unique_ptr<GCSS::GraphConfigNode> mVideoGraphResult;
    std::unique_ptr<GCSS::GraphConfigNode> mStillGraphResult;

    std::map<MediaType, std::shared_ptr<GraphConfig>> mGraphConfigMap;

    /**
     * Map to get the virtual sink id from a client stream pointer.
     * The uid is one of the GCSS keys defined for the virtual sinks, like
     * GCSS_KEY_VIDEO or GCSS_KEY_STILL
     * From that we can derive the name using the id to string methods from
     * ItemUID class
     */
    std::map<camera3_stream_t*, uid_t> mVideoStreamToSinkIdMap;
    std::map<camera3_stream_t*, uid_t> mStillStreamToSinkIdMap;

    MediaCtlConfig mMediaCtlConfigs[MEDIA_TYPE_MAX_COUNT];

    std::shared_ptr<MediaController> mMediaCtl;
};

} // namespace camera2
} // namespace android
#endif
