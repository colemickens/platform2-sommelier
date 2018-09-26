/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#ifndef PSL_IPU3_MEDIACTLHELPER_H_
#define PSL_IPU3_MEDIACTLHELPER_H_

#include "GraphConfigManager.h"
#include "MediaController.h"
#include "cros-camera/v4l2_device.h"
#include <ia_aiq_types.h>
#include "NodeTypes.h"

namespace android {
namespace camera2 {

class MediaCtlHelper
{
public:
    class IOpenCallBack {
    public:
        virtual status_t opened(IPU3NodeNames isysNodeName,
                std::shared_ptr<cros::V4L2VideoNode> videoNode) = 0;
    };

    MediaCtlHelper(std::shared_ptr<MediaController> mediaCtl,
            IOpenCallBack *openCallBack);
    virtual ~MediaCtlHelper();

    status_t configure(IStreamConfigProvider &graphConfigMgr,
            IStreamConfigProvider::MediaType type);
    std::map<IPU3NodeNames, std::shared_ptr<cros::V4L2VideoNode>>
        getConfiguredNodesPerName(IStreamConfigProvider::MediaType mediaType);
    status_t closeVideoNodes();
    status_t resetLinks();

public:
    /**
     * \struct ConfigurationResults
     * Contains relevant information for the clients of this class after the
     * Input system has been configured.
     * Input system configuration also set the configuration of the sensor.
     */
    struct ConfigurationResults {
        int pixelFormat;  /**< V4L2 pixel format produced by the input system
                               pipe */
        ia_aiq_frame_params sensorFrameParams; /**< Sensor cropping and scaling
                                                    configuration */

        ConfigurationResults() :
            pixelFormat(0) {
            CLEAR(sensorFrameParams);
        }

    };

    ConfigurationResults& getConfigResults()
    {
        return mConfigResults;
    }

private:
    status_t openVideoNodesPerPipe();
    status_t openVideoNode(const char *entityName, IPU3NodeNames isysNodeName);

private:
    ConfigurationResults mConfigResults;

    IOpenCallBack* mOpenVideoNodeCallBack;
    std::shared_ptr<MediaController> mMediaCtl;

    const MediaCtlConfig *mMediaCtlConfig;
    std::vector<MediaCtlLinkParams> mPrevMediaCtlLinks;
    IStreamConfigProvider::MediaType mPipeType;
    IStreamConfigProvider *mGCM;

    std::vector<std::shared_ptr<cros::V4L2VideoNode>> mConfiguredNodes;
    std::map<IPU3NodeNames, std::shared_ptr<cros::V4L2VideoNode>> mConfiguredNodesPerName;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_MEDIACTLHELPER_H_ */
