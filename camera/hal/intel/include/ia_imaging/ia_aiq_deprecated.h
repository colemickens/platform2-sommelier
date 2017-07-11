/*
 * Copyright (C) 2015 - 2017 Intel Corporation.
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

/*!
 * \file ia_aiq_deprecated.h
 * \brief Definitions and declarations of Intel 3A library.
 */

#ifndef _IA_AIQ_DEPRECATED_H_
#define _IA_AIQ_DEPRECATED_H_

#include "ia_aiq_types.h"
#include "ia_face.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *  \brief Input parameter structure for setting the statistics.
 */
typedef struct
{
    unsigned long long frame_id;                                /*!< The frame identifier which identifies to which frame the given statistics correspond. Must be positive. */
    unsigned long long frame_timestamp;                         /*!< Mandatory although function will not return error, if not given.
                                                                     Start of frame timestamp in microseconds. This value is used in conjunction with timestamps
                                                                     provided in the AIQ algorithms function calls to calculate the convergence
                                                                     speed of AIQ algorithms.
                                                                     AEC, AWB and AF will not converge, if not given. */
    const ia_aiq_ae_results *frame_ae_parameters;               /*!< Mandatory although function will not return error, if not given.
                                                                     Exposure results from AEC which were used to capture this frame.
                                                                     AEC depends on this parameters. AEC will return cold start values if not given.*/
    const ia_aiq_af_results *frame_af_parameters;               /*!< Mandatory although function will not return error, if not given.
                                                                     Focus results from AF which were used to capture this frame.
                                                                     AEC with AF assist light and flash usage in macro functionalities depend on these parameters. */
    const ia_aiq_rgbs_grid **rgbs_grids;                        /*!< Mandatory. Almost all AIQ algorithms require RGBS grid statistics. */
    unsigned int num_rgbs_grids;                                /*!< The number of RGBS grids. */
    const ia_aiq_hdr_rgbs_grid* hdr_rgbs_grid;                  /*!< Optional. HDR statistics grid. */
    const ia_aiq_af_grid **af_grids;                            /*!< Mandatory although function will not return error, if not given.
                                                                     AF will return a NULL pointer, if not given.
                                                                     DSD will not return all scene modes, if not given. */
    unsigned int num_af_grids;                                  /*!< The number of AF grids. */
    const ia_aiq_histogram **external_histograms;               /*!< Optional. If ISP calculates histogram, if can be given. If external histogram is not given,
                                                                     AIQ internally calculates the histogram from the RGBS grid statistics and given AWB parameters. */
    unsigned int num_external_histograms;                       /*!< The number of external histograms. */
    const ia_aiq_pa_results *frame_pa_parameters;               /*!< Optional (Mandatory if external_histogram is not given).
                                                                     AWB results used in the frame from where the statistics are collected.
                                                                     GBCE will give default gamma table if external histogram or AWB results are not available. */
    const ia_face_state *faces;                                 /*!< Mandatory although function will not return error, if not given.
                                                                     Face coordinates from external face detector.
                                                                     DSD will not return all scene modes, if not given.
                                                                     AWB will not take face information into account, if not given. */
    ia_aiq_camera_orientation camera_orientation;               /*!< The orientation of the camera. Currently unused. */

    const ia_aiq_awb_results *awb_results;                      /*!< Optional. Estimated AWB results from the previous run of AWB */
    const ia_aiq_sa_results *frame_sa_parameters;               /*!< Optional. LSC results used in the frame for statistics collected. */
    const ia_aiq_depth_grid **depth_grids;                      /*!< Optional. Depth grid statistics */
    unsigned int num_depth_grids;                               /*!< Optional. Number of depth grid statistics */
} ia_aiq_statistics_input_params;

LIBEXPORT ia_err
ia_aiq_statistics_set(ia_aiq *ia_aiq,
    const ia_aiq_statistics_input_params *statistics_input_params);

#ifdef __cplusplus
}
#endif

#endif /* _IA_AIQ_DEPRECATED_H_ */
