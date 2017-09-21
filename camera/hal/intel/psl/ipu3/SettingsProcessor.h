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

#include "CaptureUnit.h"
#include "ControlUnit.h"
#include "ProcUnitSettings.h"
#include "GraphConfigManager.h"

#ifndef PSL_IPU3_SETTINGSPROCESSOR_H_
#define PSL_IPU3_SETTINGSPROCESSOR_H_

class IntelAFStateMachine;

namespace android {
namespace camera2 {

class SettingsProcessor
{
public:
    SettingsProcessor(int cameraId,
            Intel3aPlus *a3aWrapper,
            IStreamConfigProvider &aStreamCfgProv);
    virtual ~SettingsProcessor();
    status_t init();

    /* Parameter processing methods */
    status_t processRequestSettings(const CameraMetadata &settings,
                                    RequestCtrlState &aiqCfg);

    status_t handleNewSensorDescriptor(ControlUnit::Message &msg);

    ia_aiq_frame_params *getCurrentFrameParams() { return &mCurrentFrameParams; }
    int getLSCMapWidth() { return mStaticMetadataCache.lensShadingMapSize.data.i32[0]; }
    int getLSCMapHeight() { return mStaticMetadataCache.lensShadingMapSize.data.i32[1]; }

private:
    struct StaticMetadataCache {
        camera_metadata_ro_entry availableEffectModes;
        camera_metadata_ro_entry availableEdgeModes;
        camera_metadata_ro_entry availableNoiseReductionModes;
        camera_metadata_ro_entry availableTonemapModes;
        camera_metadata_ro_entry availableHotPixelMapModes;
        camera_metadata_ro_entry availableHotPixelModes;
        camera_metadata_ro_entry availableVideoStabilization;
        camera_metadata_ro_entry availableOpticalStabilization;
        camera_metadata_ro_entry currentAperture;
        camera_metadata_ro_entry currentFocalLength;
        camera_metadata_ro_entry flashInfoAvailable;
        camera_metadata_ro_entry lensShadingMapSize;
        camera_metadata_ro_entry maxAnalogSensitivity;
        camera_metadata_ro_entry pipelineDepth;
        camera_metadata_ro_entry lensSupported;
        camera_metadata_ro_entry availableTestPatternModes;
        StaticMetadataCache() { CLEAR(*this); }

        status_t getFlashInfoAvailable(bool &available) const
        {
            if (flashInfoAvailable.count == 1) {
                available = flashInfoAvailable.data.u8[0];
                return OK;
            }
            return NAME_NOT_FOUND;
        }

        status_t getPipelineDepth(uint8_t &depth) const
        {
            if (pipelineDepth.count == 1) {
                depth = pipelineDepth.data.u8[0];
                return OK;
            }
            return NAME_NOT_FOUND;
        }
    };

public:
    const StaticMetadataCache& getStaticMetadataCache() const
    {
        return mStaticMetadataCache;
    }


private:
    void cacheStaticMetadata();

    status_t processAwbSettings(const CameraMetadata &settings,
                                RequestCtrlState &reqAiqCfg);
    status_t processAfSettings(const CameraMetadata &settings,
                               RequestCtrlState &reqAiqCfg);
    status_t processAeSettings(const CameraMetadata &settings,
                               RequestCtrlState &reqAiqCfg);
    status_t processIspSettings(const CameraMetadata &settings,
                                RequestCtrlState &reqAiqCfg);
    status_t processImageEnhancementSettings(const CameraMetadata &settings,
                                             RequestCtrlState &reqAiqCfg);
    status_t processStabilizationSettings(const CameraMetadata &settings,
                                          RequestCtrlState &reqAiqCfg);
    status_t processHotPixelSettings(const CameraMetadata &settings,
                                     RequestCtrlState &reqAiqCfg);
    status_t processTonemapSettings(const CameraMetadata &settings,
                                    RequestCtrlState &reqAiqCfg);
    void processCroppingRegion(const CameraMetadata &settings,
                                   RequestCtrlState &reqAiqCfg);

    status_t processTestPatternMode(const CameraMetadata &settings,
                                    RequestCtrlState &reqAiqCfg);
    char mapImageEnhancementSettings(const CameraMetadata &settings,
            const int enhancementName,
            RequestCtrlState &reqAiqCfg);

    status_t getTonemapCurve(const CameraMetadata settings, unsigned int tag,
                             unsigned int *gammaLutSize, float **gammaLut) const;

private:
    /**
     * Static values cached at init to avoid multiple find operations
     * on the static metadata
     */
    CameraWindow    mAPA;   /*!< Active Pixel Array */
    StaticMetadataCache mStaticMetadataCache; /*!< metadata fetched at init */
    int mCameraId;
    Intel3aPlus    *m3aWrapper; /* SettingsProcessor doesn't own m3aWrapper */

    /**
     * Sensor mode information
     * These structs hold information used when running 3A algorithms that
     * describe the currently selected sensor mode.
     * Control unit learns about them when it receives the event
     * NEW_SENSOR_DESCRIPTOR from capture unit
     */
    ia_aiq_exposure_sensor_descriptor mSensorDescriptor;

    ia_aiq_frame_params mCurrentFrameParams;

    int32_t mMinSensorModeFrameTime; /*!< min frame time in useconds dictated
                                          by the sensor mode */
    /*
     * Provider of details of the stream configuration
     */
    IStreamConfigProvider &mStreamCfgProv;

    /**
     * To be handled by the AF state machine
     */
    bool mFixedFocus;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_SETTINGSPROCESSOR_H_ */
