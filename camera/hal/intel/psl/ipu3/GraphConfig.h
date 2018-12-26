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

#ifndef _CAMERA3_GRAPHCONFIG_H_
#define _CAMERA3_GRAPHCONFIG_H_

#include <string>
#include <memory>
#include <vector>
#include <set>
#include <utils/Errors.h>
#include <hardware/camera3.h>
#include <gcss.h>
#include <ia_aiq.h>
#include <linux/media.h>
#include "MediaCtlPipeConfig.h"
#include "LogHelper.h"
#include "MediaController.h"
#include "IPU3CameraCapInfo.h"

namespace GCSS {
class GraphConfigNode;
}

#define NODE_NAME(x) (getNodeName(x).c_str())

namespace cros {
namespace intel {

class GraphConfigManager;

const int32_t ACTIVE_ISA_OUTPUT_BUFFER = 2;
const int32_t MAX_STREAMS = 4; // max number of streams
const uint32_t MAX_KERNEL_COUNT = 30; // max number of kernels in the kernel list
// Declare string consts
const std::string CSI_BE = "ipu3-cio2 ";
const std::string GC_INPUT = "input";
const std::string GC_PREVIEW = "preview";
const std::string GC_VIDEO = "video";
const std::string GC_STILL = "still";
const std::string GC_RAW = "raw";

/**
 * Stream id associated with the ISA PG that runs on Psys.
 */
static const int32_t PSYS_ISA_STREAM_ID = 60002;
/**
 * Stream id associated with the ISA PG that runs on Isys.
 */
static const int32_t ISYS_ISA_STREAM_ID = 0;

/**
 * \struct SinkDependency
 *
 * This structure stores dependency information for each virtual sink.
 * This information is useful to determine the connections that preceded the
 * virtual sink.
 * We do not go all the way up to the sensor (we could), we just store the
 * terminal id of the input port of the pipeline that serves a particular sink
 * (i.e. the input port of the video pipe or still pipe)
 */
struct SinkDependency {
    uid_t sinkGCKey;       /**< GCSS_KEY that represents a sink, like GCSS_KEY_VIDEO1 */
    int32_t streamId;   /**< (a.k.a pipeline id) linked to this sink (ex 60000) */
    uid_t streamInputPortId;    /**< 4CC code of that terminal */

    SinkDependency():
        sinkGCKey(0),
        streamId(-1),
        streamInputPortId(0) {};
};
/**
 * \class GraphConfig
 *
 * Reference and accessor to pipe configuration for specific request.
 *
 * In general case, at sream-config time there are multiple possible graphs.
 * Per each request there is additional intent that can narrow down the
 * possibilities to single graph settings: the GraphConfig object.
 *
 * This class is instantiated by \class GraphConfigManager for each request,
 * and passed around HAL (control unit, capture unit, processing unit) via
 * shared pointers. The objects are read-only and owned by GCM.
 */
class GraphConfig {
public:
    typedef GCSS::GraphConfigNode Node;
    typedef std::vector<Node*> NodesPtrVector;
    typedef std::vector<int32_t> StreamsVector;
    typedef std::map<camera3_stream_t*, uid_t> StreamToSinkMap;
    static const int32_t PORT_DIRECTION_INPUT = 0;
    static const int32_t PORT_DIRECTION_OUTPUT = 1;

public:
     GraphConfig();
    ~GraphConfig();

    /*
     * Convert Node to GraphConfig interface
     */
    const GCSS::IGraphConfig* getInterface(Node *node) const;
    const GCSS::IGraphConfig* getInterface() const;
    /*
     * Graph Interrogation methods
     */
    status_t graphGetSinksByName(const std::string &name, NodesPtrVector &sinks);
    status_t graphGetDimensionsByName(const std::string &name,
                                              int &widht, int &height);
    status_t graphGetDimensionsByName(const std::string &name,
                                              unsigned short &widht, unsigned short &height);
   /*
    * Find distinct stream ids from the graph
    */
    status_t graphGetStreamIds(std::vector<int32_t> &streamIds);
    /*
     * Sink Interrogation methods
     */
    int32_t sinkGetStreamId(Node *sink);
    /*
     * Stream Interrogation methods
     */
    status_t streamGetInputPort(int32_t streamId, Node **port);
    /*
     * Port Interrogation methods
     */
    status_t portGetFullName(Node *port, std::string &fullName);
    status_t portGetPeer(Node *port, Node **peer);
    int32_t portGetDirection(Node *port);
    bool portIsVirtual(Node *port);
    status_t portGetPeerIdByName(std::string name,
                                 uid_t &terminalId);
    status_t getDimensions(const Node *node, int &w, int &h) const;
    status_t getDimensions(const Node *node, int &w, int &h, int &l,int &t) const;

    /*
     * re-cycler static method
     */
    static void reset(GraphConfig *me);
    void fullReset();
    /*
     * Debugging support
     */
    std::string getNodeName(Node *node);
    status_t getValue(string &nodeName, uint32_t id, int &value);
    bool doesNodeExist(string nodeName);

    enum PipeType {
        PIPE_STILL = 0,
        PIPE_VIDEO,
        PIPE_MAX
    };
    PipeType getPipeType() const { return mPipeType; }
    void setPipeType(PipeType type) { mPipeType = type; }
    bool isStillPipe() { return mPipeType == PIPE_STILL; }

public:
    void setMediaCtlConfig(std::shared_ptr<MediaController> mediaCtl,
                           bool swapVideoPreview,
                           bool enableStill);

private:
    /* Helper structures to access Sensor Node information easily */
    class Rectangle {
    public:
        Rectangle();
        int32_t w;  /*<! width */
        int32_t h;  /*<! height */
        int32_t t;  /*<! top */
        int32_t l;  /*<! left */
    };

    struct MediaCtlLut {
        string uidStr;
        uint32_t uid;
        int pad;
        string nodeName;
        int ipuNodeName;
    };

    class SubdevPad: public Rectangle {
    public:
        SubdevPad();
        int32_t mbusFormat;
    };
    struct BinFactor {
        int32_t h;
        int32_t v;
    };
    struct ScaleFactor {
        int32_t num;
        int32_t denom;
    };
    union RcFactor {  // Resolution Changing factor
        BinFactor bin;
        ScaleFactor scale;
    };
    struct SubdevInfo {
        string name;
        SubdevPad in;
        SubdevPad out;
        RcFactor factor;
    };
    class SourceNodeInfo {
    public:
        SourceNodeInfo();
        string name;
        string i2cAddress;
        string modeId;
        bool metadataEnabled;
        string csiPort;
        string nativeBayer;
        SubdevInfo tpg;
        SubdevInfo pa;
        SubdevPad output;
        int32_t interlaced;
        string verticalFlip;
        string horizontalFlip;
        string link_freq;
    };
    friend class GraphConfigManager;
    // Private initializer: only used by our friend GraphConfigManager.
    void init(int32_t reqId);
    status_t prepare(Node *settings,
                     StreamToSinkMap &streamToSinkIdMap);
    status_t analyzeSourceType();
    void calculateSinkDependencies();
    void storeTuningModes();

    /*
     * Helpers for constructing mediaCtlConfigs from graph config
     */
    status_t parseSensorNodeInfo(Node* sensorNode, SourceNodeInfo &info);
    status_t getCio2MediaCtlData(int *cio2Format, MediaCtlConfig* mediaCtlConfig);
    status_t getImguMediaCtlData(int32_t cameraId,
                                 int cio2Format,
                                 int32_t testPatternMode,
                                 bool enableStill,
                                 MediaCtlConfig* mediaCtlConfig);
    status_t addControls(const Node *sensorNode,
                         const SourceNodeInfo &sensorInfo,
                         MediaCtlConfig* config);

    void addVideoNodes(MediaCtlConfig *config);
    void addImguVideoNode(int ipuNodeName, const string& nodeName, MediaCtlConfig* config);
    status_t getBinningFactor(const Node *node,
                              int32_t &hBin, int32_t &vBin) const;
    status_t getScalingFactor(const Node *node,
                              int32_t &scalingNum,
                              int32_t &scalingDenom) const;
    void addCtlParams(const string &entityName,
                      uint32_t controlName,
                      int controlId,
                      const string &strValue,
                      MediaCtlConfig* config);
    void addFormatParams(const string &entityName,
                         int width,
                         int height,
                         int pad,
                         int formatCode,
                         int field,
                         MediaCtlConfig* config);
    void addLinkParams(const string &srcName,
                       int srcPad,
                       const string &sinkName,
                       int sinkPad,
                       int enable,
                       int flags,
                       MediaCtlConfig* config);
    void addSelectionParams(const string &entityName,
                            int width,
                            int height,
                            int left,
                            int top,
                            int target,
                            int pad,
                            MediaCtlConfig* config);
    void addSelectionVideoParams(const string &entityName,
                                 const struct v4l2_subdev_selection &select,
                                 MediaCtlConfig* config);
    status_t getNodeInfo(const ia_uid uid, const Node &parent, int *width, int *height);
    void dumpMediaCtlConfig(const MediaCtlConfig &config) const;

    // Private helpers for port nodes
    status_t portGetFourCCInfo(Node &portNode,
                               uint32_t &stageId, uint32_t &terminalId);
    // Format options methods
    status_t getActiveOutputPorts(
            const StreamToSinkMap &streamToSinkIdMap);
    Node *getOutputPortForSink(const std::string &sinkName);

public:
    // Imgu used from ParameterWorker
    status_t getSensorFrameParams(ia_aiq_frame_params &sensorFrameParams);

private:
    // Disable copy constructor and assignment operator
    GraphConfig(const GraphConfig &);
    GraphConfig& operator=(const GraphConfig &);

private:
    GCSS::GraphConfigNode *mSettings;
    int32_t mReqId;
    std::map<int32_t, size_t> mKernelCountsMap; // key is stream id

    PipeType mPipeType;
    enum SourceType {
        SRC_NONE = 0,
        SRC_SENSOR,
        SRC_TPG,
    };
    SourceType mSourceType;

    /**
     * pre-computed state done *per request*.
     * This map holds the terminal id's of the ISA's peer ports (this is
     * the terminal id's of the input port of the video or still pipe)
     * that are required to fulfill a request.
     * Ideally this gets initialized during init() call.
     * But for now the GcManager will set it via a private method.
     * we use a map so that we can handle the case when a request has 2 buffers
     * that are generated from the same pipe.
     */
    std::map<uid_t, uid_t> mIsaActiveDestinations;
    /**
     * vector holding the peers to the sink nodes. Map contains pairs of
     * {sink, peer}.
     * This map is filled at stream config time.
     */
    std::map<Node*, Node*> mSinkPeerPort;
    /**
     *copy of the map provided from GraphConfigManager to be used internally.
     */
    StreamToSinkMap mStreamToSinkIdMap;
    /**
     * Map of tuning modes per stream id
     * Key: stream id
     * Value: tuning mode
     */
    std::map<int32_t, int32_t> mStream2TuningMap;

    std::string mCSIBE;
    std::shared_ptr<MediaController> mMediaCtl;

    std::vector<MediaCtlLut> mLut;
};

} // namespace intel
} // namespace cros
#endif
