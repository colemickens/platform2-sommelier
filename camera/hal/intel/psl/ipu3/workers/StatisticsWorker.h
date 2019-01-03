/*
 * Copyright (C) 2017-2018 Intel Corporation.
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

#ifndef PSL_IPU3_WORKERS_STATISTICSWORKER_H_
#define PSL_IPU3_WORKERS_STATISTICSWORKER_H_

#include <skycam_statistics.h>
#include "FrameWorker.h"
#include "CaptureUnit.h"
#include "tasks/ICaptureEventSource.h"

#define GRID_FILTER_NUM 2

namespace cros {
namespace intel {

class StatisticsWorker: public FrameWorker, public ICaptureEventSource
{
public:
    StatisticsWorker(std::shared_ptr<cros::V4L2VideoNode> node, int cameraId,
                     GraphConfig::PipeType pipeType,
                     std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> &afFilterBuffPool,
                     std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> &rgbsGridBuffPool);
    virtual ~StatisticsWorker();

    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    typedef struct {
        uint8_t bfType[2];
    } BMPFILETYPE;

    typedef struct {
        uint32_t bfSize;
        uint16_t bfReserved1;
        uint16_t bfReserved2;
        uint32_t bfOffBits;
    } BMPFILEHEADER;

    typedef struct {
        uint32_t biSize;
        int32_t biWidth;
        int32_t biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        int32_t biXPelsPerMeter;
        int32_t biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    } BMPINFOHEADER;

    enum {
        RGBS_GRID_TO_BMP = 1,
        AF_GRID_TO_BMP = 2,
    };

    status_t gridToBmp(const stats_4a_public_raw_buffer *rgbs_grid_ptr, int width, int height,
                     unsigned char *output_ptr);
    void writeBmp(const std::string &filename, const ia_css_4a_statistics &input_params,
                  unsigned int grid_width, unsigned int grid_height, unsigned int cur_grid_filter_num, unsigned int flag);
    void createBmpHeader(unsigned int width, unsigned int height, unsigned char **output_ptr);
    /* rgbs grid bmp
     * rgbs dump file is used by AWB and AE algorithm. Every pixel in this bmp is response a average pixel value
     * of this grid. For example, a high red value stand for the sum of red pixel in this grid is very high.
     * AWB/AE algorithm will adjust the white blance and exposure according those value in this bmp.
     */
    void writeRgbsGridToBmp(const std::string &rgbsFilename, const ia_css_4a_statistics &input_params,
                            unsigned int frame_counter);
    /* Af grid bmp
     * There are two bmp files for af grid stats. frame 1 stand for low pass filter, which is composed by low
     * requency pixel. frame 2 stand for high pass filter, which is composed by high requency pixel.
     * Every image frame can generate these two stats frames, AF algorithm will judge if the image is clear
     * according to these bmp files. if clear, the lens will stop at current position, if not, the lens will move to
     * far or near which is decided by the algorithm
     */
    void writeAfGridToBmp(const std::string &afFilename, const ia_css_4a_statistics &input_params,
                          unsigned int frame_counter);
    status_t afGridFilterResponseToBmp(const stats_4a_public_raw_buffer *raw_buffer,
                                     unsigned char *output_ptr, unsigned int grid_width,
                                     unsigned int grid_height, int filter_num);

private:
    GraphConfig::PipeType mPipeType;
    std::shared_ptr<SharedItemPool<ia_aiq_af_grid>>   mAfFilterBuffPool;
    std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> mRgbsGridBuffPool;
};

} /* namespace intel */
} /* namespace cros */

#endif /* PSL_IPU3_WORKERS_STATISTICSWORKER_H_ */
