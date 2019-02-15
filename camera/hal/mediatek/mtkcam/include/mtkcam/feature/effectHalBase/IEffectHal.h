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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_IEFFECTHAL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_IEFFECTHAL_H_

#include <memory>
#include <string>
#include <vector>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/feature/effectHalBase/IEffectListener.h>
#include <mtkcam/feature/effectHalBase/EffectRequest.h>

namespace NSCam {

class IEffectHal;
class IEffectHalClient;
class IImageBuffer;
class IEffectListener;

typedef enum {
  HDR_MODE,
  FB_MODE,
  MFB_MODE,
} EFFECT_SDK_HAL_MODE;

class EffectHalVersion {
 public:  // LightFlattenable
  inline bool isFixedSize() const { return false; }
  size_t getFlattenedSize() const;
  status_t flatten(void* buffer, size_t size) const;
  status_t unflatten(void const* buffer, size_t size);

 private:
  static void flattenString8(void** buffer,
                             size_t* size,
                             const std::string& string);
  static bool unflattenString8(void const** buffer,
                               size_t* size,
                               std::string* outputString);

 public:  // @todo private
  std::string effectName;
  uint32_t mCallbackID;
  uint32_t major;
  uint32_t minor;
};

/**
 *  @brief                      The prototype of MediaTek camera features.
 *  @details                    A common case of call sequence will be
 *  <pre>
 *  getNameVersion() (optional)
 *  init()
 *    setEffectListener()
 *    setParameter() * N
 *    prepare()
 *      setParameter() * N
 *      getCaptureRequirement()
 *      start()
 *        addInputFrame() * N
 *        addOutputFrame() * N
 *        abort() (optional)
 *    release()
 *  uninit()
 *  </pre>
 */
class IEffectHal {
 public:
  virtual ~IEffectHal() {}

 public:  // may change state
  /**
   *  @brief                  The first function to initialize IEffectHal
   * object.
   *
   *  @par When to call:
   *                          At the start of IEffectHal instance has been
   * created.
   *
   *  @return                 status_t
   */
  virtual status_t init() = 0;

  /**
   *  @brief                  The latest function to de-initialize IEffectHal
   * object.
   *
   *  @par When to call:
   *  After calling init()
   */
  virtual status_t uninit() = 0;

  /**
   *  @brief                  A start call to inform IEffectHal, the client is
   * ready to initial a request.
   *
   *  @par When to call:
   *                          After calling prepare(), but before calling
   * release().
   *
   *  @return                 status_t
   */
  virtual status_t configure() = 0;

  /**
   *  @brief                  A start call to inform IEffectHal, the client is
   * be stopped.
   *
   *  @par When to call:
   *                          After calling release().
   *
   *  @return                 status_t
   */
  virtual status_t unconfigure() = 0;

  /**
   *  @brief                  A start call to inform IEffectHal, the client is
   * ready to add input/output buffer.
   *
   *  @par When to call:
   *                          After calling prepare(), but before calling
   * release().
   *
   *  @return                 session id - a unique id for all
   * IEffectHal::start()
   */
  virtual uint64_t start() = 0;

  /**
   *  @brief                  Abort current process.
   *  @details                client call this function to abort IEffectHal
   * current activity.
   *
   *  @par When to call:
   *                          After calling start(), but before
   * EffectListener::onAborted() or EffectListener::onCompleted() has been
   * triggered.
   *
   *  @param parameter        for client to config abort behavior.
   *                          EX: For MAV and Panorama
   *                          - parameter["save"] = true
   *
   */
  virtual status_t abort(EffectParameter const* parameter = NULL) = 0;

 public:  // would not change state
  /**
   *  @brief                  Get version of IEffectHal object
   *
   *  @par When to call:
   *                          At the start of IEffectHal instance has been
   * created.
   *
   *  @param[out] nameVersion A reference of returned name, major, minor version
   * number
   */
  virtual status_t getNameVersion(
      const EffectHalVersion& nameVersion) const = 0;

  /**
   *  @brief                  Client register listener object by this function.
   *
   *  @par When to call:
   *                          At the start of IEffectHal instance has been
   * created.
   *
   *  @param[in] listener     Point to client's listener object
   */
  virtual status_t setEffectListener(
      std::weak_ptr<IEffectListener> const& listener) = 0;

  /**
   *  @brief                  The usage is similar to Android CameraParameters.
   * The client use this api to set IEffectHal's parameter.
   *  @details                EX:setParameter(ZoomRatio, "320");
   * setParameter(Transform, "90");
   *
   *  @par When to call:
   *                          After calling init(), but before calling start().
   *
   *  @param[in] parameterKey the key name for the parameter
   *  @param[in] object       The address that point to the string value of the
   * parameter
   *  @return                 status_t
   */
  virtual status_t setParameter(const std::string& key,
                                const std::string& object) = 0;
  virtual status_t setParameters(
      std::shared_ptr<EffectParameter> const& parameter) = 0;

  /**
   *  @brief                  Get the requirement for following capture request.
   *
   *  @par When to call:
   *                          After calling init(), but before calling unint().
   *
   *  @param[out] requirement filled Capture requirement. EX: (HDR)Target
   * exp_time, gain for bright/dark frame
   *  @return                 status_t
   */
  virtual status_t getCaptureRequirement(
      EffectParameter* inputParam,
      const std::vector<EffectCaptureRequirement>& requirements) const = 0;

  /**
   *  @brief                  [non-blocking] The function to allocate necessary
   * resource, initialize default setting of IEffectHal object.
   *
   *  @par When to call:
   *                          After calling init(), but before calling uninit()
   *
   *  @return                 status_t
   */
  // non-blocking
  virtual status_t prepare() = 0;

  /**
   *  @brief                  Release the resource allocated by
   * IEffectHal::prepare().
   *
   *  @par When to call:
   *                          After calling prepare(), but before calling
   * uninit().
   */
  virtual status_t release() = 0;

  virtual status_t updateEffectRequest(
      std::shared_ptr<EffectRequest> const& request) = 0;
};

};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_IEFFECTHAL_H_
