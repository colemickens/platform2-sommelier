/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#ifndef AAA_INTEL3APLUS_H_
#define AAA_INTEL3APLUS_H_

#include <camera/camera_metadata.h>
#include <vector>
#include <map>

#include "CameraWindow.h"
#include "UtilityMacros.h"
#include "Intel3aCore.h"

NAMESPACE_DECLARATION {
/**
 * \class Intel3aPlus
 *
 * Intel3aPlus is an interface to Intel 3A Library.
 * The purpose of this class is to
 * - convert Google specific metadata to ia_aiq input parameters
 * - convert ia_aiq output parameters to Google specific metadata
 * - call 3a functions through Intel3aCore
 *
 */
class Intel3aPlus : public Intel3aCore {
public:
    explicit Intel3aPlus(int camId);
    status_t initAIQ(int maxGridW = MAX_STATISTICS_WIDTH,
                     int maxGridH = MAX_STATISTICS_HEIGHT,
                     ia_binary_data nvmData = {nullptr,0},
                     const char* sensorName = nullptr);

    ia_aiq_frame_use getFrameUseFromIntent(const CameraMetadata * settings);

    status_t fillAeInputParams(const CameraMetadata *settings,
                               AeInputParams &aeInputParams);

    status_t fillAwbInputParams(const CameraMetadata *settings,
                               AwbInputParams &awbInputParams);

    status_t fillAfInputParams(const CameraMetadata *settings,
                               AfInputParams &afInputParams);

    status_t fillPAInputParams(const CameraMetadata &settings,
                               PAInputParams &paInputParams) const;

    status_t fillSAInputParams(const CameraMetadata &settings,
                               SAInputParams &saInputParams) const;

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
    static status_t deepCopySAResults(ia_aiq_sa_results *dst, const ia_aiq_sa_results *src);

    float getMinFocusDistance() const { return mMinFocusDistance; }
    /* map ISO */
    void setSupportIsoMap(bool support);
    void initIsoMappingRatio(void);
    int32_t mapUiIso2RealIso(int32_t iso);
    int32_t mapRealIso2UiIso(int32_t iso);
    char mapUiImageEnhancement2Aiq(int uiValue);

private:
    // prevent copy constructor and assignment operator
    Intel3aPlus(const Intel3aPlus& other);
    Intel3aPlus& operator=(const Intel3aPlus& other);

    void updateMinAEWindowSize(CameraWindow &dst);
    void parseMeteringRegion(const CameraMetadata *settings,
                             int tagId, CameraWindow &meteringWindow);

    void parseAfTrigger(const CameraMetadata &settings,
                            ia_aiq_af_input_params &afInputParams,
                            uint8_t &trigger) const;
    status_t parseAFMode(const CameraMetadata *settings,
                            ia_aiq_af_input_params &afInputParams,
                            uint8_t &afMode) const;
    void setAfMode(ia_aiq_af_input_params &afInputParams,
                   uint8_t afMode) const;
    bool afModeIsAvailable(uint8_t afMode ) const;

    status_t parseFocusDistance(const CameraMetadata &settings,
                                ia_aiq_af_input_params &afCfg) const;

// private members
private:
    int mCameraId;

    Intel3aExc mExc;

    /**
     * Cached values from static metadata
     */
    std::vector<uint8_t> mAvailableAFModes;
    float mMinFocusDistance;
    int32_t mMinAeCompensation;
    int32_t mMaxAeCompensation;

    int mMinSensitivity;
    int mMaxSensitivity;
    int64_t mMinExposureTime;
    int64_t mMaxExposureTime;
    int64_t mMaxFrameDuration;
    double mPseudoIsoRatio;
    bool mSupportIsoMap;
}; //  class Intel3aPlus
} NAMESPACE_DECLARATION_END

#endif  //  AAA_INTEL3APLUS_H_
