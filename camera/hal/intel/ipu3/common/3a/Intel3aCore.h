/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#ifndef AAA_INTEL3ACORE_H_
#define AAA_INTEL3ACORE_H_

#include <ia_aiq.h>
#include <ia_coordinate.h>
#include <string>
#include <utils/Errors.h>

#include "CameraWindow.h"
#include "UtilityMacros.h"
#include "Intel3aControls.h"
#include "Intel3aHelper.h"

#include "Intel3aCoordinate.h"
#include "Intel3aAiq.h"
#include "Intel3aCmc.h"
#include "Intel3aExc.h"
#include "Intel3aMkn.h"

namespace cros {
namespace intel {
static const unsigned int NUM_EXPOSURES = 1;   /*!> Number of frames AIQ algorithm
                                                    provides output for */
static const unsigned int NUM_FLASH_LEDS = 1;   /*!> Number of leds AEC algorithm
                                                    provides output for */
#define MAX_STATISTICS_WIDTH 150
#define MAX_STATISTICS_HEIGHT 150

static const int UI_IMAGE_ENHANCEMENT_MAX = 10;
static const float UI_IMAGE_ENHANCEMENT_STEPS = 20.0f;

/**
 * The result structures for 3A algorithm are full of pointers to other structs,
 * some of those depends on the RGBS grid size or LSC grid size
 * We should query those at init time and initialize the struct with the correct
 * amount of memory. This is a TODO as an optimization
 * for now we just allocate statically big values.
 */
static const unsigned int MAX_AE_GRID_SIZE = 2048; /*!> Top limit for  the RGBS grid size
                                                        This is an upper limit to avoid dynamic allocation*/
static const unsigned int DEFAULT_LSC_SIZE = 2048;
static const unsigned int MAX_GAMMA_LUT_SIZE = 1024;
static const unsigned int COLOR_CHANNEL_COUNT = 4;  // Bayer quad: R Gb Gr B
static const unsigned int COLOR_MATRIX_ELEMENT_COUNT = 9;  // 3 x 3 matrix

/**
 *  Maker note maximum sizes
 *  Section 1 is used for Normal capture
 *  Section 2 is used for RAW captures
 */
static const unsigned int MAKERNOTE_SECTION1_SIZE = 56000;
static const unsigned int MAKERNOTE_SECTION2_SIZE = 110592;

/**
 * \struct AiqInputParams
 * The private structs are part of AE, AF and AWB input parameters.
 * They need to separately be introduced to store the contents for
 * the corresponding pointers.
 */
struct AiqInputParams {
    void init();
    void reset();
    AiqInputParams &operator=(const AiqInputParams &other);

    ia_aiq_ae_input_params  aeInputParams;
    ia_aiq_af_input_params  afParams;
    ia_aiq_awb_input_params awbParams;
    ia_aiq_gbce_input_params gbceParams;
    ia_aiq_pa_input_params paParams;
    ia_aiq_sa_input_params saParams;
    ia_aiq_dsd_input_params dsdParams;

    /**
     * We do not directly parse the AF region in the settings to the
     * afParams focus_rectangle.
     * The fillAfInputParams will output the AF region in this member.
     * The reason is that not all HW platforms will implement touch AF
     * by passing the focus rectangle to the AF algo. The current implementation
     * assume that AF will get AF statistics covering the whole image.
     * This is not always true.
     * Some platforms modify the statistic collection parameters instead. So
     * by modifying from where we get the statistics we can also achieve the
     * effect of touch focus.
     * It will be up to the PSL implementation to make use of the afRegion.
     */
    CameraWindow    afRegion;   /*!> AF region in IA_COORDINATE space parsed
                                     from capture request settings */
    bool aeLock;
    bool awbLock;
    bool blackLevelLock;
    /*
     * Manual color correction.
     * This will be used to overwrite the results of PA
     */
    ia_aiq_color_channels manualColorGains;
    float manualColorTransform[9];

private:
    /*!< ia_aiq_ae_input_params pointer contents */
    ia_aiq_exposure_sensor_descriptor sensorDescriptor;
    ia_rectangle exposureWindow;
    ia_coordinate exposureCoordinate;
    ia_aiq_ae_features aeFeatures;
    ia_aiq_ae_manual_limits aeManualLimits;
    long manual_exposure_time_us[NUM_EXPOSURES];
    float manual_analog_gain[NUM_EXPOSURES];
    short manual_iso[NUM_EXPOSURES];

    /*!< ia_aiq_af_input_params pointer contents */
    ia_aiq_manual_focus_parameters manualFocusParams;
    ia_rectangle focusRect;

    /*!< ia_aiq_awb_input_params pointer contents */
    ia_aiq_awb_manual_cct_range manualCctRange;
    ia_coordinate manualWhiteCoordinate;

    /*!< ia_aiq_pa_input_params pointer contents */
    ia_aiq_awb_results awbResults;
    ia_aiq_color_channels colorGains;
    ia_aiq_exposure_parameters exposureParams;

    /*!< ia_aiq_sa_input_params pointer contents*/
    ia_aiq_frame_params sensorFrameParams;
};

/**
 * \class AiqResults
 * The private structs are part of AE, AF, AWB, PA and SA results.
 * They need to be separately introduced to store the contents of the results
 * that the AIQ algorithms return as pointers.
 * Then we can do deep copy of the results
 */
class AiqResults {
public:
    void init();
    AiqResults();
    ~AiqResults();
    status_t allocateLsc(size_t lscSize);
    AiqResults& operator=(const AiqResults& other);

// Member variables
public:
    int requestId;
    ia_aiq_ae_results aeResults;
    ia_aiq_awb_results awbResults;
    ia_aiq_af_results afResults;
    ia_aiq_gbce_results gbceResults;
    ia_aiq_scene_mode detectedSceneMode;
    ia_aiq_pa_results paResults;
    ia_aiq_sa_results saResults;

private:
    // prevent copy constructor and assignment operator
    AiqResults(const AiqResults& other);

private:
    /*!< ia_aiq_ae_results pointer contents */
    ia_aiq_ae_exposure_result mExposureResults;
    ia_aiq_hist_weight_grid  mWeightGrid;
    unsigned char mGrid[MAX_AE_GRID_SIZE];
    ia_aiq_flash_parameters mFlashes[NUM_FLASH_LEDS];

    /*!< ia_aiq_ae_exposure_result pointer contents */
    ia_aiq_exposure_parameters        mGenericExposure;
    ia_aiq_exposure_sensor_parameters mSensorExposure;

    /*!< ia_aiq_gbce results */
    /* The actual size of this table can be calculated by running cold
     * GBCE, it will provide those tables. TODO!!
     */
    float mRGammaLut[MAX_GAMMA_LUT_SIZE];
    float mGGgammaLut[MAX_GAMMA_LUT_SIZE];
    float mBGammaLut[MAX_GAMMA_LUT_SIZE];

    /*!< ia_aiq_sa_results pointer content */
    float *mChannelR;
    float *mChannelGR;
    float *mChannelGB;
    float *mChannelB;
};

struct SceneOverride {
    uint8_t ae;
    uint8_t awb;
    uint8_t af;
};

struct AeInputParams {
    ia_aiq_exposure_sensor_descriptor   *sensorDescriptor;
    AiqInputParams                      *aiqInputParams;
    AAAControls                         *aaaControls;
    CameraWindow                        *croppingRegion;
    CameraWindow                        *aeRegion;
    int                                 extraEvShift;
    int                                 maxSupportedFps;
    AeInputParams() : sensorDescriptor(nullptr), aiqInputParams(nullptr),
      aaaControls(nullptr), croppingRegion(nullptr), aeRegion(nullptr)
    {
      extraEvShift = 0;
      maxSupportedFps = 0;
    }
};

struct AwbInputParams {
    AiqInputParams                      *aiqInputParams;
    AAAControls                         *aaaControls;
    AwbInputParams() : aiqInputParams(nullptr), aaaControls(nullptr) { }
};

struct PAInputParams {
    AiqInputParams *aiqInputParams;
    // non Aiq related fields can be put here if needed
    PAInputParams() : aiqInputParams(nullptr) { }
};

struct SAInputParams {
    AiqInputParams *aiqInputParams;
    // non Aiq related fields can be put here if needed
    uint8_t saMode; /**< android sa mode */
    uint8_t shadingMapMode; /**< android shading map mode */
    SAInputParams() : aiqInputParams(nullptr) { saMode = 0; shadingMapMode = 0; }
};

struct DsdInputParams {
    AiqInputParams *aiqInputParams;
    // non Aiq related fields can be put here if needed
    ia_aiq_dsd_input_params aiqDsdInputParams;
    DsdInputParams() : aiqInputParams(nullptr) { CLEAR(aiqDsdInputParams); }
};

struct AfInputParams {
    AiqInputParams *aiqInputParams;
    AfControls  *afControls;
    AfInputParams() : aiqInputParams(nullptr), afControls(nullptr) { }
};

/**
 * \class Intel3aCore
 *
 * Intel3aCore is wrapper of Intel 3A Library (known as libia_aiq).
 * The purpose of this class is to
 * - call libia_aiq functions such as run 3A functions
 * - dump/deep copy 3a input params and 3a results
 */
class Intel3aCore {
public:
    Intel3aCore(int camId);
    void enableAiqdDataSave(const bool enableAiqdDataSave = true);
    status_t init(int maxGridW = MAX_STATISTICS_WIDTH,
                     int maxGridH = MAX_STATISTICS_HEIGHT,
                     ia_binary_data nvmData = {nullptr,0},
                     const char* sensorName = nullptr);
    void deinit();

    status_t setStatistics(ia_aiq_statistics_input_params *ispStatistics);

    status_t runAe(ia_aiq_statistics_input_params *ispStatistics,
                   ia_aiq_ae_input_params *aeInputParams,
                   ia_aiq_ae_results *aeResults);

    status_t runAf(ia_aiq_statistics_input_params *ispStatistics,
                   ia_aiq_af_input_params *afInputParams,
                   ia_aiq_af_results* afResults);

    status_t runAwb(ia_aiq_statistics_input_params *ispStatistics,
                    ia_aiq_awb_input_params *awbInputParams,
                    ia_aiq_awb_results *awbResults);

    status_t runGbce(ia_aiq_statistics_input_params *ispStatistics,
                     ia_aiq_gbce_input_params *gbceInputParams,
                     ia_aiq_gbce_results *gbceResults);

    status_t runPa(ia_aiq_statistics_input_params *ispStatistics,
                   ia_aiq_pa_input_params *paInputParams,
                   ia_aiq_pa_results *paResults);

    status_t runSa(ia_aiq_statistics_input_params *ispStatistics,
                   ia_aiq_sa_input_params *saInputParams,
                   ia_aiq_sa_results *saResults,
                   bool forceUpdated = false);

    status_t getMakerNote(ia_mkn_trg aTarget, ia_binary_data &aBlob);

    void convertFromAndroidToIaCoordinates(const CameraWindow &srcWindow,
                                           CameraWindow &toWindow);
    void convertFromIaToAndroidCoordinates(const CameraWindow &srcWindow,
                                           CameraWindow &toWindow);

    status_t calculateDepthOfField(const ia_aiq_af_results &afResults,
                                   float &dofNear, float &dofFar) const;

    class LSCGrid {
    public: /* this was a struct: class just to satisfy a static code scanner */
        uint16_t width;
        uint16_t height;
        float *gridR;
        float *gridGr;
        float *gridGb;
        float *gridB;

        bool isBad() const {
            return (gridB == nullptr || gridGb == nullptr || gridR == nullptr ||
                    gridGr == nullptr || width == 0 || height == 0);
        }
        LSCGrid(): width(0), height(0), gridR(nullptr), gridGr(nullptr),
            gridGb(nullptr), gridB(nullptr) {}
    };

    /*
     * static common operation
     */
    static float calculateHyperfocalDistance(ia_cmc_t &cmc);
    static status_t reFormatLensShadingMap(const LSCGrid &inputLscGrid,
                                           float *dstLscGridRGGB);
    static status_t storeLensShadingMap(const LSCGrid &inputLscGrid,
                                        LSCGrid &resizeLscGrid,
                                        float *dstLscGridRGGB);

    static status_t deepCopyAiqResults(AiqResults &dst,
                                       const AiqResults &src,
                                       bool onlyCopyUpdatedSAResults = false);
    static status_t deepCopyAEResults(ia_aiq_ae_results *dst, const ia_aiq_ae_results *src);
    static status_t deepCopyGBCEResults(ia_aiq_gbce_results *dst,
                                 const ia_aiq_gbce_results *src);
    static status_t deepCopyPAResults(ia_aiq_pa_results *dst, const ia_aiq_pa_results *src);
    static status_t deepCopySAResults(ia_aiq_sa_results *dst,
                                      const ia_aiq_sa_results *src,
                                      bool forceUpdated = false);

    const ia_cmc_t* getCmc() const { return mCmc ? mCmc->getCmc() : nullptr; }
    char mapUiImageEnhancement2Aiq(int uiValue);

private:
    // prevent copy constructor and assignment operator
    Intel3aCore(const Intel3aCore& other);
    Intel3aCore& operator=(const Intel3aCore& other);
    bool saveAiqdData();

    status_t convertError(ia_err iaErr);

protected:
    const Intel3aCmc* mCmc; /* CameraConf has ownership */

// private members
private:
    int mCameraId;
    CameraWindow mActivePixelArray;
    float mHyperFocalDistance; /**< in millimeters */
    bool mEnableAiqdDataSave;

private:
    Intel3aAiq mAiq;
    Intel3aMkn mMkn;
    Intel3aCoordinate mCoordinate;
}; //  class Intel3aCore

} /* namespace intel */
} /* namespace cros */
#endif  //  AAA_INTEL3ACORE_H_
