/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#define LOG_TAG "CpfConf"

#include "CameraConf.h"
#include "LogHelper.h"
#include "PlatformData.h"
#include <ctype.h>         // tolower()
#include <dirent.h>        // DIR, dirent
#include <fnmatch.h>       // fnmatch()
#include <fcntl.h>         // open(), close()
#include <math.h>          // pow, log10
#include <algorithm>       // sort
#include <linux/media.h>   // media controller
#include <linux/kdev_t.h>  // MAJOR(), MINOR()
#include <utils/Errors.h>  // Error codes
#include <sys/stat.h>
#include <string>
#include <sstream>
#include "ia_exc.h"
#include "CameraMetadataHelper.h"
#include "Intel3aPlus.h"
#include "Intel3aExc.h"

NAMESPACE_DECLARATION {
using std::string;

static const int TRANSFORM_MATRIX_SIZE = 9;
static const float EPSILON = 0.00001;
static const int FORWARD_MATRIX_PRECISION = 65536;

/**
 * Location of CPF files (actually they are .aiqb)
 */
const char *cpfConfigPath = "/etc/camera/ipu3/";

// FIXME: The spec for following is "dr%02d[0-9][0-9]??????????????.aiqb"
const char *cpfConfigPatternSuffix = "*.aiqb";  // How CPF file name should look
const int FRAME_USE_MODE_NUMBER = 3;
const char* frameUseModeList[FRAME_USE_MODE_NUMBER] = {CPF_MODE_STILL,
                                                       CPF_MODE_VIDEO,
                                                       CPF_MODE_PREVIEW};

AiqConf::AiqConf(const int cameraId, const int size):
    mPtr(nullptr),
    mSize(size),
    mCmc(cameraId),
    mMetadata(nullptr),
    mCameraId(cameraId)
{
    UNUSED(mCameraId);

    if (size)
        mPtr = ::operator new(size);
}

AiqConf::~AiqConf()
{
    mCmc.deinit();

    if (mMetadata)
        mMetadata = nullptr;

    ::operator delete(mPtr);
    mPtr = nullptr;
}

status_t AiqConf::initCMC()
{
    if (mCmc.getCmc() != nullptr
        || ptr() == nullptr) {
        LOGE("@%s, Error Initializing CMC", __FUNCTION__);
        return NO_INIT;
    }

    ia_binary_data cpfData;
    CLEAR(cpfData);
    cpfData.data = ptr();
    cpfData.size = size();

    bool ret = mCmc.init((ia_binary_data*)&(cpfData));
    CheckError(ret == false, NO_INIT, "@%s, mCmc->init fails", __FUNCTION__);

    ia_cmc_t* cmc = mCmc.getCmc();
    cmc_lens_shading_t* cmc_lens_shading = cmc->cmc_parsed_lens_shading.cmc_lens_shading;
    if (cmc_lens_shading) {
        LOG1("@%s, grid_width:%d, grid_height:%d",
            __FUNCTION__, cmc_lens_shading->grid_width, cmc_lens_shading->grid_height);
    }

    return NO_ERROR;
}

status_t AiqConf::initMapping(LightSrcMap &lightSourceMap)
{
    // init light source mapping
    lightSourceMap.clear();
    lightSourceMap.insert(std::make_pair(cmc_light_source_d65,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT));
    lightSourceMap.insert(std::make_pair(cmc_light_source_f11,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_FLUORESCENT));
    lightSourceMap.insert(std::make_pair(cmc_light_source_a,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_TUNGSTEN));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d65,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_FLASH));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d65,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_FINE_WEATHER));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d75,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_CLOUDY_WEATHER));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d75,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_SHADE));
    lightSourceMap.insert(std::make_pair(cmc_light_source_f1,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT_FLUORESCENT));
    lightSourceMap.insert(std::make_pair(cmc_light_source_f3,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAY_WHITE_FLUORESCENT));
    lightSourceMap.insert(std::make_pair(cmc_light_source_f2,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_COOL_WHITE_FLUORESCENT));
    lightSourceMap.insert(std::make_pair(cmc_light_source_a,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_WHITE_FLUORESCENT));
    lightSourceMap.insert(std::make_pair(cmc_light_source_a,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A));
    lightSourceMap.insert(std::make_pair(cmc_light_source_b,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_B));
    lightSourceMap.insert(std::make_pair(cmc_light_source_c,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_C));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d55,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D55));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d65,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d75,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D75));
    lightSourceMap.insert(std::make_pair(cmc_light_source_d50,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D50));
    lightSourceMap.insert(std::make_pair(cmc_light_source_a,
                ANDROID_SENSOR_REFERENCE_ILLUMINANT1_ISO_STUDIO_TUNGSTEN));
    return NO_ERROR;
}

status_t AiqConf::fillStaticMetadataFromCMC(camera_metadata_t * metadata)
{
    status_t res = OK;
    if (mCmc.getCmc() == nullptr) {
        res = initCMC();
        if (res != NO_ERROR)
            return res;
    }

    // Check if the metadata has been filled
    if (mMetadata == metadata) {
        return res;
    } else {
        mMetadata = metadata;
    }

    // Lens
    res |= fillLensStaticMetadata(metadata);
    res |= fillSensorStaticMetadata(metadata);
    res |= fillLscSizeStaticMetadata(metadata);
    return res;
}

status_t AiqConf::fillLensStaticMetadata(camera_metadata_t * metadata)
{
    status_t res = OK;

    ia_cmc_t* cmc = mCmc.getCmc();
    CheckError(cmc == nullptr, UNKNOWN_ERROR, "@%s, mCmc.getCmc fails", __FUNCTION__);

    if (cmc->cmc_parsed_optics.cmc_optomechanics != nullptr) {
        uint16_t camera_features = cmc->cmc_parsed_optics.cmc_optomechanics->camera_actuator_features;
        // Lens: aperture
        float fn = 2.53;
        if (!(camera_features & cmc_camera_feature_variable_apertures)
            && cmc->cmc_parsed_optics.cmc_optomechanics->num_apertures == 1
            && cmc->cmc_parsed_optics.lut_apertures != nullptr) {
            // fixed aperture, the fn should be divided 100 because the value is multiplied 100 in cmc
            fn = (float)cmc->cmc_parsed_optics.lut_apertures[0] / 100;
            float av = log10(pow(fn, 2)) / log10(2.0);
            av = ((int)(av * 10 + 0.5)) / 10.0;
            res |= MetadataHelper::updateMetadata(metadata, ANDROID_LENS_INFO_AVAILABLE_APERTURES, &av, 1);
            LOG2("static ANDROID_LENS_INFO_AVAILABLE_APERTURES :%.2f", av);
        }
        // Lens: FilterDensities
        int32_t nd_gain = 0;
        if (camera_features & cmc_camera_feature_nd_filter)
            nd_gain = cmc->cmc_parsed_optics.cmc_optomechanics->nd_gain;
        res |= MetadataHelper::updateMetadata(metadata, ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES, (float*)&nd_gain, 1);
        LOG2("static ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES :%d", nd_gain);
        // Lens: availableFocalLengths, only support fixed focal length
        float effect_focal_length = 3.00;
        if (!(camera_features & cmc_camera_feature_optical_zoom)) {
            // focal length (mm * 100) from CMC
            effect_focal_length = (float)cmc->cmc_parsed_optics.cmc_optomechanics->effect_focal_length / 100;
            res |= MetadataHelper::updateMetadata(metadata, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &effect_focal_length, 1);
            LOG2("static ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS :%.2f", effect_focal_length);
        }
        /**
         * check the type of focus actuator
         * 0 means no actuator, in this case we signal that this is a fixed
         * focus sensor by setting min_focus_distance to 0.0
         * (see documentation for this tag)
         *
         */
        float min_focus_distance = 0.0;
        if (cmc->cmc_parsed_optics.cmc_optomechanics->actuator != 0) {
            // the unit from CMC is cm, convert to diopters (1/m)
            if (cmc->cmc_parsed_optics.cmc_optomechanics->min_focus_distance != 0)
                min_focus_distance = 100 / (float)cmc->cmc_parsed_optics.cmc_optomechanics->min_focus_distance;
        }
        res |= MetadataHelper::updateMetadata(metadata, ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &min_focus_distance, 1);
        LOG2("static ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE :%.2f", min_focus_distance);

        float hyperfocal_distance_diopter = 1000 / Intel3aPlus::calculateHyperfocalDistance(*cmc);
        res |= MetadataHelper::updateMetadata(metadata, ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE, &hyperfocal_distance_diopter, 1);
        LOG2("static ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE :%.2f", hyperfocal_distance_diopter);
        // TODO: lens.info.availableOpticalStabilization
    }
    return res;
}

status_t AiqConf::fillLightSourceStaticMetadata(camera_metadata_t * metadata)
{
    status_t res = UNKNOWN_ERROR;
    LightSrcMap lightSourceMap;
    initMapping(lightSourceMap);

    ia_cmc_t* cmc = mCmc.getCmc();
    CheckError(cmc == nullptr, UNKNOWN_ERROR, "@%s, mCmc.getCmc fails", __FUNCTION__);

    // color matrices
    if (cmc->cmc_parsed_color_matrices.cmc_color_matrix != nullptr && cmc->cmc_parsed_color_matrices.cmc_color_matrices != nullptr) {
        int num_matrices = cmc->cmc_parsed_color_matrices.cmc_color_matrices->num_matrices;
        // android metadata requests 2 light source
        if (num_matrices < 2)
            return res;

        res = OK;
        // calibrationTransform1
        // the below matrix should be sample specific transformation to golden module
        // in the future the matrix should be calculated by the data from NVM
        uint16_t calibrationTransform[TRANSFORM_MATRIX_SIZE] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        int sensor_calibration_transform_tag[2] = {ANDROID_SENSOR_CALIBRATION_TRANSFORM1, ANDROID_SENSOR_CALIBRATION_TRANSFORM2};
        int sensor_reference_illuminant_tag[2] = {ANDROID_SENSOR_REFERENCE_ILLUMINANT1, ANDROID_SENSOR_REFERENCE_ILLUMINANT2};
        int sensor_color_transform_tag[2] = {ANDROID_SENSOR_COLOR_TRANSFORM1, ANDROID_SENSOR_COLOR_TRANSFORM2};
        int sensor_forward_matrix_tag[2] = {ANDROID_SENSOR_FORWARD_MATRIX1, ANDROID_SENSOR_FORWARD_MATRIX2};
        int32_t matrix_accurate[TRANSFORM_MATRIX_SIZE];
        int16_t reference_illuminant[2];
        CLEAR(matrix_accurate);
        CLEAR(reference_illuminant);
        float XYZ_to_sRGB[TRANSFORM_MATRIX_SIZE] = {3.2404542, -1.5371385, -0.4985314,
                                                    -0.9692660, 1.8760108, 0.0415560,
                                                    0.0556434, -0.2040259, 1.0572252};
        float XYZ_to_sRGB_det = XYZ_to_sRGB[0] * XYZ_to_sRGB[4] * XYZ_to_sRGB[8] +
                                XYZ_to_sRGB[1] * XYZ_to_sRGB[5] * XYZ_to_sRGB[6] +
                                XYZ_to_sRGB[2] * XYZ_to_sRGB[3] * XYZ_to_sRGB[7] -
                                XYZ_to_sRGB[2] * XYZ_to_sRGB[4] * XYZ_to_sRGB[6] -
                                XYZ_to_sRGB[0] * XYZ_to_sRGB[5] * XYZ_to_sRGB[7] -
                                XYZ_to_sRGB[1] * XYZ_to_sRGB[3] * XYZ_to_sRGB[8];
        float XYZ_to_sRGB_adj[TRANSFORM_MATRIX_SIZE];
        //adjugate matrix
        XYZ_to_sRGB_adj[0] = XYZ_to_sRGB[4] * XYZ_to_sRGB[8] - XYZ_to_sRGB[5] * XYZ_to_sRGB[7];
        XYZ_to_sRGB_adj[1] = XYZ_to_sRGB[2] * XYZ_to_sRGB[7] - XYZ_to_sRGB[1] * XYZ_to_sRGB[8];
        XYZ_to_sRGB_adj[2] = XYZ_to_sRGB[1] * XYZ_to_sRGB[5] - XYZ_to_sRGB[2] * XYZ_to_sRGB[4];
        XYZ_to_sRGB_adj[3] = XYZ_to_sRGB[5] * XYZ_to_sRGB[6] - XYZ_to_sRGB[3] * XYZ_to_sRGB[8];
        XYZ_to_sRGB_adj[4] = XYZ_to_sRGB[0] * XYZ_to_sRGB[8] - XYZ_to_sRGB[2] * XYZ_to_sRGB[6];
        XYZ_to_sRGB_adj[5] = XYZ_to_sRGB[2] * XYZ_to_sRGB[3] - XYZ_to_sRGB[0] * XYZ_to_sRGB[5];
        XYZ_to_sRGB_adj[6] = XYZ_to_sRGB[3] * XYZ_to_sRGB[7] - XYZ_to_sRGB[4] * XYZ_to_sRGB[6];
        XYZ_to_sRGB_adj[7] = XYZ_to_sRGB[1] * XYZ_to_sRGB[6] - XYZ_to_sRGB[0] * XYZ_to_sRGB[7];
        XYZ_to_sRGB_adj[8] = XYZ_to_sRGB[0] * XYZ_to_sRGB[4] - XYZ_to_sRGB[1] * XYZ_to_sRGB[3];
        // inverse matrix
        if (!(XYZ_to_sRGB_det >= -EPSILON && XYZ_to_sRGB_det <= EPSILON)) {
            // need to iterate loop down, otherwise gtest produces a segfault
            // todo: figure out why iterating from 0 to 8 causes that segfault
            for (int i = TRANSFORM_MATRIX_SIZE - 1; i >= 0; --i)
                XYZ_to_sRGB_adj[i] = XYZ_to_sRGB_adj[i] / XYZ_to_sRGB_det;
        }
        camera_metadata_rational_t forward_matrix[TRANSFORM_MATRIX_SIZE];
        CLEAR(forward_matrix);

        for(int light_src_num = 0; light_src_num < 2; light_src_num++) {
            cmc_light_source light_src = (cmc_light_source)cmc->cmc_parsed_color_matrices.cmc_color_matrix[light_src_num].light_src_type;
            LightSrcMap::iterator it = lightSourceMap.find(light_src);
            if (it == lightSourceMap.end()) {
                LOG2("light source not found, use default!!");
                reference_illuminant[light_src_num] = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
            } else {
                reference_illuminant[light_src_num] = it->second;
            }
            // referenceIlluminant
            res |= MetadataHelper::updateMetadata(metadata, sensor_reference_illuminant_tag[light_src_num], &reference_illuminant[light_src_num], 1);
            // calibrationTransform
            res |= MetadataHelper::updateMetadata(metadata, sensor_calibration_transform_tag[light_src_num], &calibrationTransform, TRANSFORM_MATRIX_SIZE);
            // colorTransform
            MEMCPY_S(matrix_accurate,
                     sizeof(matrix_accurate),
                     cmc->cmc_parsed_color_matrices.cmc_color_matrix[light_src_num].matrix_accurate,
                     sizeof(int32_t) * TRANSFORM_MATRIX_SIZE);
            res |= MetadataHelper::updateMetadata(metadata, sensor_color_transform_tag[light_src_num], &matrix_accurate, TRANSFORM_MATRIX_SIZE);
            LOG2("matrix_accurate:%d,%d,%d,%d,%d,%d,%d,%d,%d",matrix_accurate[0],
                 matrix_accurate[1],matrix_accurate[2],matrix_accurate[3],
                 matrix_accurate[4],matrix_accurate[5],matrix_accurate[6],
                 matrix_accurate[7],matrix_accurate[8]);

            // forwardMatrix
            if (!(XYZ_to_sRGB_det >= -EPSILON && XYZ_to_sRGB_det <= EPSILON)) {
                int row = 0;
                int col = 0;
                for (int i = 0; i < TRANSFORM_MATRIX_SIZE; i++) {
                    row = i / 3;
                    col = i % 3;
                    forward_matrix[i].numerator = 0;
                    // CMC matrix is in Q16 format, so denominator is 65536
                    forward_matrix[i].denominator = FORWARD_MATRIX_PRECISION;
                    // matrix multiply
                    for (int j = 0; j < 3; j++)
                        forward_matrix[i].numerator = forward_matrix[i].numerator + XYZ_to_sRGB_adj[row * 3 + j] * matrix_accurate[j * 3 + col];
                    LOG2("forward matrix [%d] = {%d, %d}", i, forward_matrix[i].numerator, forward_matrix[i].denominator);
                }
                res |= MetadataHelper::updateMetadata(metadata, sensor_forward_matrix_tag[light_src_num], forward_matrix, TRANSFORM_MATRIX_SIZE);
            }
        }
    }
    return res;
}

status_t AiqConf::fillSensorStaticMetadata(camera_metadata_t * metadata)
{
    LOG1("%s", __FUNCTION__);
    status_t res = OK;

    ia_cmc_t* cmc = mCmc.getCmc();
    CheckError(cmc == nullptr, UNKNOWN_ERROR, "@%s, mCmc.getCmc fails", __FUNCTION__);

    // colorFilterArrangement
    if (cmc->cmc_general_data != nullptr) {
        uint16_t color_order = 0;
        switch (cmc->cmc_general_data->color_order) {
        case cmc_bayer_order_grbg:
            color_order = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
            break;
        case cmc_bayer_order_rggb:
            color_order = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
            break;
        case cmc_bayer_order_bggr:
            color_order = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR;
            break;
        case cmc_bayer_order_gbrg:
            color_order = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG;
            break;
        default:
            LOGE("Unsupported color_order inside cmc general data: %d", color_order);
            break;
        }

        res |= MetadataHelper::updateMetadata(metadata, ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &color_order, 1);
        LOG2("color order:%d", color_order);
    }
    // whiteLevel
    if (cmc->cmc_saturation_level != nullptr) {
        int32_t saturation_level = cmc->cmc_saturation_level->saturation_cc1;
        res |= MetadataHelper::updateMetadata(metadata, ANDROID_SENSOR_INFO_WHITE_LEVEL, &saturation_level, 1);
        LOG2("saturation_level:%d", saturation_level);
    }

    int16_t base_iso = 0;
    // baseGainFactor
    if (cmc->cmc_sensitivity != nullptr) {
        // baseGainFactor's definition is gain factor from electrons to raw units when ISO=100
        camera_metadata_rational_t baseGainFactor;
        baseGainFactor.numerator = 100;
        baseGainFactor.denominator = cmc->cmc_sensitivity->base_iso;
        base_iso = cmc->cmc_sensitivity->base_iso;
        res |= MetadataHelper::updateMetadata(metadata, ANDROID_SENSOR_BASE_GAIN_FACTOR, &baseGainFactor, 1);
        LOG2("base_iso:%d", base_iso);
    }
    // blackLevelPattern
    if (cmc->cmc_parsed_black_level.cmc_black_level_luts != nullptr) {
        uint16_t cc1 = cmc->cmc_parsed_black_level.cmc_black_level_luts->color_channels.cc1 / 256;
        uint16_t cc2 = cmc->cmc_parsed_black_level.cmc_black_level_luts->color_channels.cc2 / 256;
        uint16_t cc3 = cmc->cmc_parsed_black_level.cmc_black_level_luts->color_channels.cc3 / 256;
        uint16_t cc4 = cmc->cmc_parsed_black_level.cmc_black_level_luts->color_channels.cc4 / 256;
        int32_t black_level_pattern[4];
        black_level_pattern[0] = cc1;
        black_level_pattern[1] = cc2;
        black_level_pattern[2] = cc3;
        black_level_pattern[3] = cc4;
        res |= MetadataHelper::updateMetadata(metadata, ANDROID_SENSOR_BLACK_LEVEL_PATTERN, &black_level_pattern, 4);
        LOG2("blackLevelPattern, cc1:%d,cc2:%d,cc3:%d,cc4:%d", cc1, cc2, cc3, cc4);
    }
    // fill metadata: referenceIlluminant, sensor.colorTransform,
    //                sensor.forwardMatrix, sensor.calibrationTransform
    fillLightSourceStaticMetadata(metadata);

    // maxAnalogSensitivity
    if (cmc->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion != nullptr) {
        Intel3aExc exc;
        int32_t max_analog_sensitivity = 0;
        float max_analog_gain = 0.0;
        unsigned short analog_gain_code = 0;
        // we can give a large value (e.g. 1000) as input to ia_exc.
        // Output from ia_exc is a clipped sensor specific MAX.
        exc.AnalogGainToSensorUnits(&(cmc->cmc_parsed_analog_gain_conversion), 1000, &analog_gain_code);
        exc.SensorUnitsToAnalogGain(&(cmc->cmc_parsed_analog_gain_conversion),
                                           analog_gain_code,
                                           &max_analog_gain);
       // caclulate the iso base on max analog gain
       max_analog_sensitivity = max_analog_gain * base_iso;
       res |= MetadataHelper::updateMetadata(metadata, ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY, &max_analog_sensitivity, 1);
    }
    // noiseProfile, this should be got from CMC in the future, currently use default value first
    return res;
}

/*
Android requires lensShadingMapSize must be smaller than 64*64,
and it is static, however, in some case, such as video recording,
the width and height got from pa_results->weighted_lsc->cmc_lens_shading->grid_width
and grid_height are variable for different resolution.
here we are calculating the size of the downscaled LSC table we will calculate for them.
Preserve the same Aspect Ratio.
*/
status_t AiqConf::fillLscSizeStaticMetadata(camera_metadata_t * metadata)
{
    status_t res = OK;

    ia_cmc_t* cmc = mCmc.getCmc();
    CheckError(cmc == nullptr, UNKNOWN_ERROR, "@%s, mCmc.getCmc fails", __FUNCTION__);

    if (cmc->cmc_parsed_lens_shading.cmc_lens_shading != nullptr) {
        int32_t lensShadingMapSize[2];
        lensShadingMapSize[0] = cmc->cmc_parsed_lens_shading.cmc_lens_shading->grid_width;
        lensShadingMapSize[1] = cmc->cmc_parsed_lens_shading.cmc_lens_shading->grid_height;

        int i = 1;
        uint16_t destWidth = lensShadingMapSize[0];
        uint16_t destHeight = lensShadingMapSize[1];

        while (destWidth > MAX_LSC_GRID_WIDTH || destHeight > MAX_LSC_GRID_HEIGHT) {
               i++;
               destWidth = lensShadingMapSize[0] / i;
               destHeight = lensShadingMapSize[1] / i;
        }
        lensShadingMapSize[0] = destWidth;
        lensShadingMapSize[1] = destHeight;
        LOG2("grid_width:%d, grid_height:%d", lensShadingMapSize[0], lensShadingMapSize[1]);
        res = MetadataHelper::updateMetadata(metadata, ANDROID_LENS_INFO_SHADING_MAP_SIZE, &lensShadingMapSize, 2);
    }
    return res;
}

CpfStore::CpfStore(const int xmlCameraId, CameraHWInfo * cameraHWInfo)
    : mCameraId(xmlCameraId)
    , mHasMediaController(false)
    , mIsOldConfig(false)
{
    LOG1("@%s, mCameraId:%d", __FUNCTION__, mCameraId);

    mAiqConf.clear();

    // If anything goes wrong here, we simply return silently.
    // CPF should merely be seen as a way to do multiple configurations
    // at once; failing in that is not a reason to return with errors
    // and terminate the camera (some cameras may not have any CPF at all)

    /**
     * There is no CPF file for SoC sensors.
     * avoid getting any error messages when looking for it
     */
    const CameraCapInfo *capInfo = PlatformData::getCameraCapInfoForXmlCameraId(mCameraId);
    if (capInfo == nullptr) {
        LOGE("Cannot find xml camera id: %d", mCameraId);
        return;
    }
    if (capInfo->sensorType() == SENSOR_TYPE_SOC)
        return;

    mRegisteredDrivers = cameraHWInfo->mSensorInfo;
    mHasMediaController = cameraHWInfo->mHasMediaController;

    // Find out the related file names
    if (initFileNames(mCpfFileNames)) {
        // Error message given already
        return;
    }

    string mode;
    string fileName;
    mAiqConfig.clear();
    for (size_t i = 0; i < mCpfFileNames.size(); i++) {

        fileName = mCpfFileNames[i];
        //extract aiqb file name from the full file path
        std::size_t pos = fileName.rfind('/');
        if (pos != string::npos)
            fileName = fileName.substr(pos + 1);

        getCpfFileMode(fileName, mode);
        LOG1("%s: mode %s, file name: %s", __FUNCTION__,
                mode.c_str(), mCpfFileNames[i].c_str());

        // Obtain the configurations
        if (loadConf(mCpfFileNames[i])) {
            // Error message given already
            return;
        }

        // Provide configuration data for algorithms and image
        // quality purposes, and continue further even if errors did occur.
        // Pointer to that data is cleared later, whenever seen suitable,
        // so that the memory reserved for CPF data can then be freed
        // get the mode name and store in AIQ config vector
        mAiqConfig.insert(std::make_pair(mode, mAiqConf.at(i)));
    }
}

CpfStore::~CpfStore()
{
    mAiqConfig.clear();
    for (auto &it : mAiqConf) {
        delete it;
    }
    mAiqConf.clear();
    LOG1("@%s", __FUNCTION__);
}

status_t CpfStore::initFileNames(std::vector<string> &cpfFileNames)
{
    LOG1("@%s", __FUNCTION__);
    status_t ret = 0;

    string cpfPathName;
    struct stat statInfo;
    int PathExists = stat(cpfConfigPath, &statInfo);
    if (PathExists == 0) {
        cpfPathName = cpfConfigPath;
    } else {
        LOGE("Failed to find path for AIQB files!! - BUG");
        return UNKNOWN_ERROR;
    }

    // Secondly, we will find all the matching configuration files
    if ((ret = findConfigFile(cpfPathName, cpfFileNames))) {
        // Error message given already
        return ret;
    }

    LOG1("cpf config file name: %s", cpfPathName.c_str());

    return ret;
}

/**
 * Search the path were CPF files are stored
 * Find a CPF file that follows the following pattern:
 *      <cameraID><sensorname>[.<frame_use>][.<device_id>].aiqb
 * where
 * - cameraID is the android camera id, traditionally 00 for back camera
 * and 01 for front camera
 * - sensor name is the sensor name provided by the driver
 * - frame use is an optional part which presents the aiq frame use mode,
 * like preview, still, video.
 * The frame use is used in the event that different tunings are required
 * for video/still use cases for3A+ algorithms.
 * - device_id is an optional part that can be one of the following values
 * -- spid based string (what it has been used until now)
 * The device id is used in the event that the same sensor is used by multiple
 * product devices. The reason why there are several properties is that not all
 * platforms support SPID so we need to have a back up.
 *
 * In the event that multiple files in the CPF directory match the first 2
 * parts of the pattern then the logic in this method will select the first file
 * that matches any of the device_ids in the following order:
 *  - spid
 *   From the most detailed to the more generic
 *
 *   ex: back sensor 0v8858, used in multiple devices: moorefield (with SPID)
 *   and sofia3g product (MRD5)
 *   the files that we get after the first filter could be:
 *    00ov8858.aiqb
 *    00ov8858-0x10x20x3.aiqb
 *    00ov8858-sf3g_mrd5_p1.aiqb
 *
 *    The device_ids return from platform data will be
 *    in Moorefield:
 *          0x10x20x3
 *          mofd_v1
 *          moorefield
 *          moorefield
 *     in Sofia MRD5
 *         sf3g_mrd5_p1
 *         sofia3g
 *         sofia3g
 *  by selecting the file that matches the in order the device id we can
 *  select a very specific one or fallback to platform default.
 *  in case no match is done we can always select one and warn about it
 *
 * \param[in] path String with the path where the cpf files are stored
 * \param[out] cpfFileNames A vector of strings with the valid file names
 *
 * \return OK if the file name was found
 * \return NO_INIT if no valid configuration file was found
 */
status_t CpfStore::findConfigFile(const string &path,
                                  std::vector<string> &cpfFileNames)
{
    status_t ret = OK;

    // Let's filter first the files that look like CPF Files
    // the name should follow the pattern: "%02d*.aiqb"
    // ex: 00xxxxx.aiqb if mCameraId is 00
    std::vector<string> allCpfFileNames;
    ret = getCpfFileList(path, allCpfFileNames);
    if (ret != OK) {
        LOGE("ERROR Finding CPF Files!");
        return ret;
    }

    /**
     *  Now we filter the cpf files that match in the name the name of one of
     *  sensor drivers we have discovered in the first phase. Those names are
     *  stored  in mRegisteredDrivers
     */
    std::vector<string> registeredCpfFiles;
    filterKnownSensors(allCpfFileNames, registeredCpfFiles);

    if (registeredCpfFiles.empty()) {
        LOGW("No valid CPF file (is ok for SoC sensors)");
        return NO_INIT;
    }

    std::vector<string> deviceIds;
    ret = PlatformData::getDeviceIds(deviceIds);
    bool found = false;

    // Filter all the cpf files for a device id.
    if (registeredCpfFiles.size() == 1) {
        cpfFileNames.push_back(path + registeredCpfFiles[0]);
        found = true;
    } else {
        for (size_t i = 0; i < deviceIds.size(); i++) {
            // iterate through all the device ids in order try to find a match
            // in the cpf files
            for (size_t j = 0; j < registeredCpfFiles.size(); j++) {
                if (registeredCpfFiles[j].find(deviceIds[i]) != string::npos) {
                    cpfFileNames.push_back(path + registeredCpfFiles[j]);
                    found = true;
                }
            }
            if (found)
                break;
        }
    }

    if (!found) {
        LOGW("Could not find a good fit for CPF file, using default %s",
                registeredCpfFiles[0].c_str());
        cpfFileNames.push_back(path + registeredCpfFiles[0]);
    }
    return OK;
}

/**
 * compareString
 *
 * Take 2 string as input, and compare them, used to sort the cpf file,
 * called by getCpfFileList().
 * \param[in] 2 string lhs and rhs
 * \return true : if lhs is less than rhs
 * \return false : if lhs is greater than or equal to rhs
 */
static bool compareString(const string &lhs, const string &rhs)
{
    return lhs < rhs;
}

/**
 * getCpfFileList
 *
 * Return the list of CPF files found in the cpfConfigPath that matches the
 * pattern of a CPF file, this is:
 *                    "%02d*.aiqb"
 *
 * \param[in] path String with the path where the cpf files are stored
 * \param[out] cpfFileNames List of file names
 * \return OK: No error looking for files
 * \return FAILED_TRANSACTION: failed to read the contents of the directory
 * \return ENOTDIR if the path where the CPF files should be is not available
 */
status_t CpfStore::getCpfFileList(const string &path,
                                  std::vector<string> &cpfFileNames)
{
    status_t ret = 0;
    DIR *dir = opendir(path.c_str());
    if (!dir) {
       LOGE("ERROR in opening CPF folder \"%s\": %s!",
               cpfConfigPath, strerror(errno));
       return ENOTDIR;
    }
    do {
        dirent *entry;
        if (errno = 0, !(entry = readdir(dir))) {
            if (errno) {
                LOGE("ERROR in browsing CPF folder \"%s\": %s!",
                        cpfConfigPath, strerror(errno));
                ret = FAILED_TRANSACTION;
            }
            break;
        }
        if ((strcmp(entry->d_name, ".") == 0) ||
            (strcmp(entry->d_name, "..") == 0)) {
            continue;  // Skip self and parent
        }
        // Check that the file name follows the pattern
        std::ostringstream stringStream;
        stringStream << "0" << mCameraId << cpfConfigPatternSuffix;
        std::string pattern = stringStream.str();
        int r = fnmatch(pattern.c_str(), entry->d_name, 0);
        switch (r) {
        case 0:
            // The file name looks like a valid CPF file name
            cpfFileNames.push_back(string(entry->d_name));
            continue;
        case FNM_NOMATCH:
            // The file name did not look like a CPF file name
            continue;
        default:
            // Unknown error (the looping ends at 'while')
            LOGE("ERROR in pattern matching file name \"%s\"!", entry->d_name);
            ret = UNKNOWN_ERROR;
            continue;
        }
    } while (!ret);

    // sort filenames
    std::sort(cpfFileNames.begin(), cpfFileNames.end(), compareString);

    if (closedir(dir)) {
        LOGE("ERROR in closing CPF folder \"%s\": %s!",
                cpfConfigPath, strerror(errno));
        if (!ret) ret = EPERM;
    }
    return ret;
}

/**
 * filterKnownSensors
 *
 * Goes through the list of cpf files found in the directory and removes the
 * ones that do not match any of the names registered to the driver
 * The names registered to the driver are stored in mRegisteredDrivers
 *
 * \param[in] allCpfFileNames List of all CPF files found in file system
 * \param[out] registeredCpfFileNames List of CPF files to filter
 */
void CpfStore::filterKnownSensors(std::vector<string> &allCpfFileNames,
                                  std::vector<string> &registeredCpfFileNames)
{
    registeredCpfFileNames.clear();

    for (size_t i = 0; i < allCpfFileNames.size(); i++) {
        for (size_t j = 0; j < mRegisteredDrivers.size(); j++) {
            // return cpfFile according to sensorName in non-MediaController path
            const char* sensorName = mRegisteredDrivers[j].mSensorName.c_str();
            if (!mHasMediaController &&
                allCpfFileNames[i].find(sensorName) != string::npos) {
                registeredCpfFileNames.push_back(allCpfFileNames[i]);
            } else if (mHasMediaController &&
                allCpfFileNames[i].find(sensorName) != string::npos) {
                // TODO remove: This else-if-branch a hack to overcome the issue
                // caused by above mCameraId vs. mIspPort comparison in
                // MediaController-enabled platforms.
                registeredCpfFileNames.push_back(allCpfFileNames[i]);
            }
        }
    }
}

/**
 * CPF files for a given sensor will have a name with the following structure:
 * <cameraId><sensor_name>[.<frame_use>][.<device_id>].aiqb
 * or <cameraId><sensor_name>.<device_id>.aiqb
 * ex:
 * 00imx214.preview.bxt_rvp.aiqb
 * 00imx214.video.bxt_rvp.aiqb
 * 00imx214.still.bxt_rvp.aiqb
 * or
 * 00imx214.bxt_rvp.aiqb
 * or
 * 00imx214.still.aiqb
 * 00imx214.video.aiqb
 * or
 * 00imx214.aiqb
 *
 * Parse the frame_use section as the frame use mode. Frame_use section is
 * an optional part. If it is absent, the frame use mode will be considered
 * to be "default".
 *
 * \param[in] cpfFileName String with the cpf file name
 * \param[out] mode Frame use mode of the input cpf file.
 */
void CpfStore::getCpfFileMode(const string &cpfFileName, string &mode)
{
    mode = string(CPF_MODE_DEFAULT);
    for (int i = 0; i < FRAME_USE_MODE_NUMBER; i++) {
        if (cpfFileName.find(frameUseModeList[i]) != string::npos) {
            mode = string(frameUseModeList[i]);
            break;
        }
    }
    return;
}

status_t CpfStore::loadConf(const string &cpfFileName)
{
    LOG1("@%s, cameraId:%d", __FUNCTION__, mCameraId);
    FILE *file;
    struct stat statCurrent;
    status_t ret = 0;

    LOG1("Opening CPF file \"%s\"", cpfFileName.c_str());
    file = fopen(cpfFileName.c_str(), "rb");
    if (!file) {
        LOGE("ERROR in opening CPF file \"%s\": %s!",
                cpfFileName.c_str(), strerror(errno));
        return NAME_NOT_FOUND;
    }

    do {
        int fileSize;
        if ((fseek(file, 0, SEEK_END) < 0) ||
            ((fileSize = ftell(file)) < 0) ||
            (fseek(file, 0, SEEK_SET) < 0)) {
            LOGE("ERROR querying properties of CPF file \"%s\": %s!",
                    cpfFileName.c_str(), strerror(errno));
            ret = ESPIPE;
            break;
        }

        AiqConf *conf = new AiqConf(mCameraId, fileSize);

        if (!conf->ptr() || fread(conf->ptr(), fileSize, 1, file) < 1) {
            LOGE("ERROR reading CPF file \"%s\"!", cpfFileName.c_str());
            ret = EIO;
            delete conf;
            break;
        }
        mAiqConf.push_back(conf);

        // We use file statistics for file identification purposes.
        // The access time was just changed (because of us!),
        // so let's nullify the access time info
        if (stat(cpfFileName.c_str(), &statCurrent) < 0) {
            LOGE("ERROR querying filestat of CPF file \"%s\": %s!",
                    cpfFileName.c_str(), strerror(errno));
            ret = FAILED_TRANSACTION;
            break;
        }
        statCurrent.st_atime = 0;

    } while (0);

    if (fclose(file)) {
        LOGE("ERROR in closing CPF file \"%s\": %s!",
                cpfFileName.c_str(), strerror(errno));
        if (!ret) ret = EPERM;
    }

    return ret;
}
} NAMESPACE_DECLARATION_END

