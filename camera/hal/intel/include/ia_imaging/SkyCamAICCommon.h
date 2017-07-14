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

 #pragma once

#include "IPU3AICCommon.h"

//#include "ia_aiq_types.h"
//#include "ia_isp_types.h"
//#include "InterpMachine.h"
//#include "Pipe.h"
//#include  "cpffData.h"
//#include <vector>
//#include  "ia_dpc.h"
//#include <memory>
//#include "dpc_public.h"
//#include "bds_public.h"
//#include "obgrid_public.h"
//#include "ia_css_lin_types.h"
//#include "shd_public.h"
//#include "bnr_public.h"
//#include "anr_public.h"
//#include "dm_public.h"
//#include "ia_css_tnr3_types.h"
//#include "yuvp1_b0_public.h"
//#include "yuvp1_c0_public.h"
//#include "yuvp2_public.h"
//#include "rgbpp_public.h"
//#include "ia_css_xnr3_types.h"
//#include "ia_abstraction.h"
//
//#define BypassPowerSaveMode 1
//
//#define OBG_TILE_SIZE 16
//
//typedef struct ia_aiq_output_frame_parameters{
//	//output_resolution resolution;
//	unsigned short width;
//	unsigned short height;
//	bool           fix_flip_x;  //  fix the x flipping if was done in sensor
//	bool           fix_flip_y ; //  fix the y flipping if was done in sensor
//} ia_aiq_output_frame_parameters_t;
//
//typedef struct aic_input_frame_parameters{
//	ia_aiq_frame_params sensor_frame_params; /*!< Mandatory. Sensor frame parameters. Describe frame scaling/cropping done in sensor. */
//	bool fix_flip_x;  //  fix the x flipping if was done in sensor
//	bool fix_flip_y ; //  fix the y flipping if was done in sensor
//} aic_input_frame_parameters_t;
//
//typedef struct aic_resolution_config_parameters{
//	uint16_t horizontal_IF_crop;
//	uint16_t vertical_IF_crop;
//	uint16_t BDSin_img_width;
//	uint16_t BDSin_img_height;
//	uint16_t BDSout_img_width; //without padding
//	uint16_t BDSout_img_height; //without padding
//	//float    horizontal_BDS_scale;=BDSout_img_width/BDSin_img_width
//	//float    vertical_BDS_scale;=BDSout_img_height/BDSin_img_height
//	uint16_t BDS_horizontal_padding;
//} aic_resolution_config_parameters_t;
////Note that effective_img_width+horizontal_padding= image_output_resolution_width
////Note that effective_img_height+vertical_padding= image_output_resolution_height
//
//class Array2DInterpMachine {
//	size_t m_size;
//    int m_frameNum, m_modeNum;
//	std::vector<InterpMachine *> m_data;
//  public:
//    Array2DInterpMachine():
//		m_size(0), m_frameNum(2), m_modeNum(0), m_data(1)
//	{}
//	Array2DInterpMachine(int frames, int modes):
//		m_size(0), m_frameNum(frames), m_modeNum(modes), m_data((frames + 1)*(modes + 1)) {
//	}
//	void Set(InterpMachine* intMach, int frame_index, int mode_index) {
//		if ((mode_index*m_frameNum+frame_index) >= (int)m_data.size())
//			m_data.resize(mode_index*m_frameNum+m_frameNum);
//		m_data.at(mode_index*m_frameNum+frame_index) = intMach;
//		m_size++;
//		//std::vector<InterpMachine *>::pointer ptr = &m_data[0];
//	}
//	void Push(InterpMachine* intMach) {
//		m_data.push_back(intMach);
//		m_size++;
//	}
//	size_t GetSize() {
//		return m_size;
//	}
//    InterpMachine* operator()(int frame_index, int mode_index) {
//		if ((0 != m_data.size()) && ((int)m_data.size() > (mode_index*m_frameNum+frame_index)))
//			return m_data.at(mode_index*m_frameNum+frame_index);
//		return 0;
//    }
//    const InterpMachine* const operator()(const int frame_index, const int mode_index) const {
//		if ((0 != m_data.size()) && ((int)m_data.size() > (mode_index*m_frameNum+frame_index)))
//			return m_data.at(mode_index*m_frameNum+frame_index);
//		return 0;
//    }
//};
//
//class Array2DInt {
//	size_t m_size;
//    size_t m_width, m_height;
//    std::vector<int> m_data;
//  public:
//    Array2DInt():
//      m_size(0), m_width(2), m_height(0), m_data(10)
//	{}
//    Array2DInt(size_t x, size_t y, int init = 0):
//      m_size(0), m_width(x), m_height(y), m_data(x*y, init)
//    {}
//	void Set(int domNum, int x, int y) {
//		if ((int)m_data.size() <= y*m_width+x)
//			m_data.resize(y*m_width+m_width);
//		m_data.at(y*m_width+x) = domNum;
//		m_size++;
//	}
//	void Push(int domNum) {
//		m_data.push_back(domNum);
//		m_size++;
//	}
//	size_t GetSize() {
//		return m_size;
//	}
//    int& operator()(size_t x, size_t y) {
//		int i = 0;
//		int& iRet = i;
//		if ((0 != m_data.size()) && ((int)m_data.size() > (y*m_width+x)))
//			return m_data.at(y*m_width+x);
//		return iRet;
//    }
//    const int& operator()(size_t x, size_t y) const {
//		int i = 0;
//		const int& iRet = i;
//		if ((0 != m_data.size()) && ((int)m_data.size() > (y*m_width+x)))
//			return m_data.at(y*m_width+x);
//		return iRet;
//    }
//};
//
struct SkyCamAICRuntimeParams
{
	long long time_stamp;
	ia_aiq_frame_use frame_use;
	int mode_index;
    const aic_input_frame_parameters_t *input_frame_params;  /*!< Mandatory. Inputr frame parameters. Describe frame scaling/cropping done in sensor. */
	const aic_resolution_config_parameters_t *frame_resolution_parameters;
	const ia_aiq_output_frame_parameters_t *output_frame_params; /*!< Mandatory. Output frame parameters.  */
    const ia_aiq_exposure_parameters *exposure_results;    /*!< Mandatory. Exposure parameters which are to be used to calculate next ISP parameters. */
    const ia_aiq_hist_weight_grid *weight_grid;
	const ia_aiq_awb_results *awb_results;                 /*!< Mandatory. WB results which are to be used to calculate next ISP parameters (WB gains, color matrix,etc). */
    const ia_aiq_gbce_results *gbce_results;               /*!< Mandatory. GBCE Gamma tables which are to be used to calculate next ISP parameters.
                                                                If NULL pointer is passed, AIC will use static gamma table from the CPF.  */
    const ia_aiq_pa_results *pa_results;                   /*!< Mandatory. Parameter adaptor results from AIQ. */
	const ia_aiq_sa_results *sa_results;                   /*!< Mandatory. Shading adaptor results from AIQ. */
    uint32_t isp_vamem_type;                               /*!< Mandatory. ISP vamem type. */

    char manual_brightness;                                /*!< Optional. Manual brightness value range [-128,127]. */
    char manual_contrast;                                  /*!< Optional. Manual contrast value range [-128,127]. */
    char manual_hue;                                       /*!< Optional. Manual hue value range [-128,127]. */
    char manual_saturation;                                /*!< Optional. Manual saturation value range [-128,127]. */
    char manual_sharpness;                                 /*!< Optional. Manual setting for sharpness [-128,127]. */
    ia_isp_effect effects;                                 /*!< Optional. Manual setting for special effects. Combination of ia_aiq_effect enums.*/
	ia_rectangle *focus_rect;
	sd_dpc_output *scdpc_data;
};
