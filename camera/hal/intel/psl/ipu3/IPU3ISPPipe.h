/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_IPU3_IPU3ISPPIPE_H_
#define PSL_IPU3_IPU3ISPPIPE_H_

#include <Pipe.h>
#include <IPU3AICCommon.h>

namespace android {
namespace camera2 {

#define NUM_ISP_PIPES 1

typedef enum AicMode {
    AIC_MODE_STILL = 0,
    AIC_MODE_VIDEO,
    AIC_MODE_MAX,
} AicMode;

class IPU3ISPPipe: public ISPPipe
{
public:
    IPU3ISPPipe();
    virtual ~IPU3ISPPipe();

public:

    // This function configures the HW/FW pipe via CSS interface
    virtual void SetPipeConfig(const aic_output_t pipe_config);

    virtual pipe_ver GetPipeVer() { return Czero; };

    virtual const ia_aiq_rgbs_grid* GetAWBStats();
    virtual const ia_aiq_af_grid* GetAFStats();
    virtual const ia_aiq_histogram* GetAEStats();
    virtual aic_config* GetAicConfig();

    virtual void dump();

private:
    aic_output_t mAicOutput;

    aic_config mAicConfig; /* Config to driver */

};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_IPU3ISPPIPE_H_ */
