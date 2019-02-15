/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_EFFECTHALBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_EFFECTHALBASE_H_
#include <mtkcam/feature/effectHalBase/IEffectHal.h>

#include <memory>
#include <string>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/

namespace NSCam {
// class EffectHalBase : public IEffectHal
/**
 *  @brief EffectHalBase implement the IEffectHal inteface and declare pure
 * virtual functions let feature owner implemenetation.
 *
 */
class EffectHalBase : public IEffectHal {
 public:
  EffectHalBase();
  virtual ~EffectHalBase();

 public:  // may change state
  /**
   *  @brief Change state to STATE_INIT and call initImpl() that overriding by
   derived class.
   *  @details
          - This function could not be overridden anymore.
          - This function should be called on STATE_UNINIT.
          - This function may change status to STATE_INIT.


   */
  status_t init() final;
  /**
   *  @brief Change state to STATE_UNINIT and call uninitImpl() that overriding
   by derived class.
   *  @details
          - This function couldn?™t be overridden anymore.
          - This function should be called on STATE_INIT.
          - This function may change status to STATE_UNINIT.
   */
  status_t uninit() final;

  /**
   *  @brief This function will check all capture parameters are setting done
   via allParameterConfigured() and change statue to STATE_CONFIGURED.
   *  @details
          - This function could not be overridden anymore.
          - This function should be called on STATE_INIT.
          - This function may change status to STATE_CONFIGURED.
  */
  status_t configure() final;
  /**
   *  @brief Release resource and change status to STATE_INIT.
   *  @details
          - This function could not be overridden anymore.
          - This function should be called on STATE_CONFIGURED.
          - This function may change status to STATE_INIT.
  */
  status_t unconfigure() final;
  /**
   *  @brief Start this session. Change state to STATE_RUNNING and call
   startImpl() that overriding by derived class.
   *  @details
          - This function could not be overridden anymore.
          - This function should be called on STATE_CONFIGURED.
          - This function may change status to STATE_RUNNING.
  */
  uint64_t start() final;
  /**
   *  @brief Abort this session. This function will change state to
   STATE_CONFIGURED and call abortImpl() that overriding by derived class.
   *  @details
          - This function could not be overridden anymore.
          - This function should be called on STATE_RUNNING.
          - This function may change status to STATE_CONFIGURED.
  */
  status_t abort(EffectParameter const* parameter = NULL) final;

 public:  // would not change state
  status_t getNameVersion(const EffectHalVersion& nameVersion) const final;
  status_t setEffectListener(std::weak_ptr<IEffectListener> const&) final;
  status_t setParameter(const std::string& key,
                        const std::string& object) final;
  status_t setParameters(
      std::shared_ptr<EffectParameter> const& parameter) final;
  status_t getCaptureRequirement(
      EffectParameter* inputParam,
      const std::vector<EffectCaptureRequirement>& requirements) const final;
  // non-blocking
  status_t prepare() final;
  status_t release() final;
  // non-blocking
  status_t updateEffectRequest(
      std::shared_ptr<EffectRequest> const& request) final;

 private:
  std::weak_ptr<IEffectListener> mpListener;

  //---------------------------------------------------------------------------
  //---------------------------------------------------------------------------
 private:
  enum State {
    STATE_UNINIT = 0x01,
    STATE_INIT = 0x02,
    STATE_CONFIGURED = 0x04,
    STATE_RUNNING = 0x08
  };
  State mState;
  bool mPrepared;
  uint64_t mUid;

 protected:  // call those in sub-class
  status_t prepareDone(const EffectResult& result, status_t state);

 protected:  // should be implement in sub-class
  virtual bool allParameterConfigured() = 0;

  virtual status_t initImpl() = 0;
  virtual status_t uninitImpl() = 0;
  // non-blocking
  virtual status_t prepareImpl() = 0;
  virtual status_t releaseImpl() = 0;

  virtual status_t getNameVersionImpl(
      const EffectHalVersion& nameVersion) const = 0;
  virtual status_t getCaptureRequirementImpl(
      EffectParameter* inputParam,
      const std::vector<EffectCaptureRequirement>& requirements) const = 0;
  virtual status_t setParameterImpl(const std::string& key,
                                    const std::string& object) = 0;
  virtual status_t setParametersImpl(
      std::shared_ptr<EffectParameter> parameter) = 0;
  virtual status_t startImpl(uint64_t* uid = NULL) = 0;
  virtual status_t abortImpl(EffectResult* result,
                             EffectParameter const* parameter = NULL) = 0;
  // non-blocking
  virtual status_t updateEffectRequestImpl(
      const std::shared_ptr<EffectRequest> request) = 0;
};

}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_EFFECTHALBASE_H_
