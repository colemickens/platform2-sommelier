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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PIPELINEPLUGIN_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PIPELINEPLUGIN_H_

#include <mtkcam/3rdparty/plugin/PluginInterceptor.h>
#include <mtkcam/3rdparty/plugin/Reflection.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/metadata/IMetadata.h>
//
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

/******************************************************************************
 *
 ******************************************************************************/
template <class T>
class VISIBILITY_PUBLIC PipelinePlugin {
 public:
  typedef typename std::shared_ptr<PipelinePlugin<T>> Ptr;
  typedef typename std::weak_ptr<PipelinePlugin<T>> WeakPtr;

  typedef typename T::Property Property;
  struct Selection : T::Selection {
    typedef typename std::shared_ptr<Selection> Ptr;
  };
  struct Request : T::Request {
    typedef typename std::shared_ptr<Request> Ptr;
  };

  /*
   * Every plugin must define a interface to offer the plug point's
   * capabilities. To expose the support buffer formats and sizes, for provider
   * to select what it want to meet its requirement.
   *
   */
  class IInterface {
   public:
    typedef typename std::shared_ptr<IInterface> Ptr;

    /*
     * Provide the list of buffer size and format, which plug point could do
     *
     * @param[out] sel: the offering selection
     * @return 0 indicates success; otherwise failure.
     */
    virtual MERROR offer(Selection* sel) = 0;

    virtual ~IInterface() {}
  };

  /*
   * The request using asynchronous call must be with a callback pointer.
   * The provider must send a callback if the call to process() is successful.
   *
   */
  class RequestCallback {
   public:
    typedef typename std::shared_ptr<RequestCallback> Ptr;

    /*
     * Cancel a request which have sent to plugin successfully
     *
     * @param[in] req: the request pointer
     */
    virtual void onAborted(typename Request::Ptr) = 0;

    /*
     * Notify a completed result and request result
     *
     * @param[in] req: the request pointer
     * @param[in] err: result status
     */
    virtual void onCompleted(typename Request::Ptr, MERROR) = 0;

    virtual ~RequestCallback() {}
  };

  /*
   * A plugin could have multiple implementations for different features.
   * The provider will follow the specification to exchange data or processing
   * buffer.
   *
   */
  class IProvider {
   public:
    typedef typename std::shared_ptr<IProvider> Ptr;

    /*
     * Set the open id to provider. It will be called after construction
     */
    virtual void set(MINT32 iOpenId);

    /*
     * get the property to expose the plugin's characteristic
     *
     * @return the required information to plug point
     */
    virtual const Property& property() = 0;

    /*
     * Negotiate buffer format and size between plug point and provider.
     * The provider should update the accept format and size.
     *
     * The provider should return -EINVAL if the offered selection do NOT meet
     * the requirement.
     *
     * @param[in] sel: the offering selection
     * @return 0 indicates success; otherwise failure.
     *
     */
    virtual MERROR negotiate(Selection* sel) = 0;

    /*
     * Initialized procedure of plugin.  That may have multiple users to share a
     * plugin instance. Only the first call to init() will be invoked.
     *
     */
    virtual void init() = 0;

    /*
     * Send a request to plugin provider.
     * Synchronous call to process() If callback pointer is null
     * Asynchronous call to process() If callback poiner is not null.
     *
     * @param[in] req: the request pointer
     * @param[in] cb: a callback for a asynchronous request
     * @return 0 indicates success; otherwise failure.
     */
    virtual MERROR process(typename Request::Ptr,
                           typename RequestCallback::Ptr = nullptr) = 0;

    /*
     * abort the specific requests and will block until the requests have been
     * aborted.
     *
     * @param[in] req: list of aborting requests
     */
    virtual void abort(const std::vector<typename Request::Ptr>&) = 0;

    /*
     * Uninitialized procedure of plugin.  That may have multiple users to share
     * a plugin instance. Only the last call to uninit() will be invoked, which
     * represent will users had leaved.
     *
     */
    virtual void uninit() = 0;
    virtual ~IProvider() {}
  };

  /*
   * Get a plugin by a specific sensor id.
   * Create a instance if not existed. Will be released while no one hold this
   * pointer.
   *
   * @param[in] iOpenId
   * @param[in] iOpenId2
   * @return a singleton pointer to plugin
   *     .
   */
  static Ptr getInstance(MINT32 iOpenId, MINT32 iOpenId2 = -1);

  /*
   * Create a shared pointer to empty request.
   */
  typename Request::Ptr createRequest();

  /*
   * Get the instance of IInterface
   */
  typename IInterface::Ptr getInterface();

  /*
   * Get the instances of IProvider.
   */
  const std::vector<typename IProvider::Ptr>& getProviders();

  /*
   * Get the result of negotiation
   * @param[in] prov: the plugin provider
   * @return selection
   *
   */
  const Selection& getSelection(typename IProvider::Ptr prov);
  /*
   * Create a shared pointer to empty selection.
   */
  typename Selection::Ptr createSelection();

  /*
   * Push a selection into the provider's container
   * @param[in] prov: the plugin provider
   * @param[in] sel: the negotiated selection
   * @return selection
   *
   */
  MVOID pushSelection(typename IProvider::Ptr prov,
                      typename Selection::Ptr sel);
  /*
   * Pop a selection from the provider's container
   * @param[in] prov: the plugin provider
   * @return selection
   *
   */
  typename Selection::Ptr popSelection(typename IProvider::Ptr prov);

  /*
   * Dump the all plugin's peropties and selections
   *
   * @param[in] os outputstream
   */
  MVOID dump(std::ostream& os);

  /*
   * Consturctor
   * @param[in] openId: Sensor Id
   */
  PipelinePlugin(MINT iOpenId, MINT iOpenId2)
      : mOpenId(iOpenId), mpInterface(nullptr) {}

  virtual ~PipelinePlugin();

 private:
  MINT32 mOpenId;
  typename IInterface::Ptr mpInterface;
  std::mutex mMutex;
  std::vector<typename IProvider::Ptr> mpProviders;
  std::map<typename IProvider::Ptr, std::queue<typename Selection::Ptr>>
      mpSelections;
  static std::map<MINT, WeakPtr> mInstances;
};

/******************************************************************************
 * Common
 ******************************************************************************/
typedef std::shared_ptr<NSCam::IMetadata> MetadataPtr;

/******************************************************************************
 * Buffer & Metadata Handle
 ******************************************************************************/
class MetadataHandle {
 public:
  typedef typename std::shared_ptr<MetadataHandle> Ptr;

  /*
   * Acquire the pointer of metadata
   *
   * @return the pointer of metadata
   */
  virtual NSCam::IMetadata* acquire() = 0;

  /*
   * Release the metadata to the caller
   */
  virtual MVOID release() = 0;

  /*
   * Dump the handle info
   *
   * @param[in] os outputstream
   */
  virtual MVOID dump(std::ostream& os) const = 0;

  virtual ~MetadataHandle() {}
};

class BufferHandle {
 public:
  typedef typename std::shared_ptr<BufferHandle> Ptr;

  /*
   * Acquire the pointer of locked image buffer
   *
   * @param[in] usage: the buffer usage
   * @return the pointer of image buffer
   */
  virtual NSCam::IImageBuffer* acquire(
      MINT usage = eBUFFER_USAGE_HW_CAMERA_READWRITE |
                   eBUFFER_USAGE_SW_READ_OFTEN) = 0;

  /*
   * Release the image buffer to the caller
   */
  virtual MVOID release() = 0;

  /*
   * Dump the handle info
   *
   * @param[in] os outputstream
   */
  virtual MVOID dump(std::ostream& os) const = 0;

  virtual ~BufferHandle() {}
};

std::ostream& operator<<(std::ostream&, const BufferHandle::Ptr);
std::ostream& operator<<(std::ostream&, const MetadataHandle::Ptr);

/******************************************************************************
 * Buffer & Metadata Selection
 ******************************************************************************/
class VISIBILITY_PUBLIC MetadataSelection {
 public:
  MetadataSelection();
  virtual ~MetadataSelection();

  /*
   * [Provider] set the metadata whether required or not
   * It will get a null metadata in enque phrase if not required
   *
   * @param[in] required
   */
  MetadataSelection& setRequired(MBOOL required);

  MBOOL getRequired() const;

  /*
   * [User] add the control metadata, which is the original frame metadata.
   * Only set metadata in input port
   *
   * @param[in] control: the shared pointer of metadata
   */
  MetadataSelection& setControl(MetadataPtr control);

  /*
   * [Provider] add the addtional metadata, which will be applied into pipeline
   * frame. Only set metadata in input port ex: HDR has to control the exposure
   * time in every frame.
   *
   * @param[in] addtional: the shared pointer of metadata
   */
  MetadataSelection& setAddtional(MetadataPtr additional);

  /*
   * [Provider] add the dummy metadata. It's used in creating extra pipeline
   * frames before capture starting. ex: HDR could use rear dummy frame to
   * stablize AE.
   *
   * @param[in] addtional: the shared pointer of metadata
   */
  MetadataSelection& setDummy(MetadataPtr additional);
  /*
   * Get the control metadata
   *
   * @return the shared pointer of metadata
   */
  MetadataPtr getControl() const;

  /*
   * Get the addtional metadata
   *
   * @return the shared pointer of metadata
   */
  MetadataPtr getAddtional() const;

  /*
   * Get the dummy metadata.
   *
   * @return the shared pointer of metadata
   */
  MetadataPtr getDummy() const;

  /*
   * Dump the selection info
   *
   * @param[in] os outputstream
   */
  MVOID dump(std::ostream& os) const;

 private:
  class Implementor;
  Implementor* mImpl;
};

class VISIBILITY_PUBLIC BufferSelection {
 public:
  BufferSelection();
  virtual ~BufferSelection();

  /*
   * [Provider] set the buffer whether required or not
   * The request will get a null buffer in enque phrase if not required
   *
   * @param[in] required
   */
  BufferSelection& setRequired(MBOOL required);

  MBOOL getRequired() const;

  /*
   * [Provider] set the output buffer to optional
   * The request will probably get a buffer on data flow's demand if set
   * optional
   *
   * @param[in] optional
   */
  BufferSelection& setOptional(MBOOL optional);

  MBOOL getOptional() const;

  /*
   * [Provider] set the specified size if have added the size enum[SPECIFIED]
   *
   * @param[in] size: the width and height
   */
  BufferSelection& setSpecifiedSize(const MSize& size);

  /*
   * [Provider] set the buffer alignment for all size enum
   *
   * @param[in] width: the width alignment
   * @param[in] height: the height alignment
   */
  BufferSelection& setAlignment(MUINT32 width, MUINT32 height);

  /*
   * [Provider] add an acceptable image format for this buffer port
   *
   * @param[in] fmt EImageFormat
   */
  BufferSelection& addAcceptedFormat(MINT fmt);

  /*
   * [Provider] add an acceptable image size for this buffer port
   *
   * @param[in] size EImageSize
   */
  BufferSelection& addAcceptedSize(MINT size);

  /*
   * [Interface] add the supported image format for this buffer port
   *
   * @param[in] fmt EImageFormat
   */
  BufferSelection& addSupportFormat(MINT fmt);

  /*
   * [Interface] add the supported image size for this buffer port
   *
   * @param[in] size EImageSize
   */
  BufferSelection& addSupportSize(MINT size);

  /*
   * Check the negotiated result whether valid or not.
   * That must have the intersectional of formats and sizes
   *
   * @return valid if have successful negotiation
   */
  MBOOL isValid() const;

  /*
   * Get the intersection formats between interface side and provider side
   *
   * @return the vector of formats
   */
  const std::vector<MINT>& getFormats() const;

  /*
   * Get the intersection sizes between interface side and provider side
   *
   * @return the vector of sizes
   */
  const std::vector<MINT>& getSizes() const;

  /*
   * Get the specified size.
   *
   * @return the image's width and height
   */
  const MSize& getSpecifiedSize() const;

  /*
   * Get the alignment
   *
   * @return the alignment of width and height
   */
  MVOID getAlignment(MUINT32&, MUINT32&) const;

  /*
   * Dump the selection info
   *
   * @param[in] os outputstream
   */
  MVOID dump(std::ostream& os) const;

 private:
  class Implementor;
  Implementor* mImpl;
};

enum EImageSize {
  eImgSize_Full,
  eImgSize_Resized,
  eImgSize_Quarter,
  eImgSize_Specified,
  eImgSize_Arbitrary,
};

std::ostream& operator<<(std::ostream&, const BufferSelection&);
std::ostream& operator<<(std::ostream&, const MetadataSelection&);

/******************************************************************************
 * Plugin Registry
 ******************************************************************************/
template <class T>
class VISIBILITY_PUBLIC PluginRegistry {
 public:
  typedef typename PipelinePlugin<T>::IProvider::Ptr (*ConstructProvider)();
  typedef typename PipelinePlugin<T>::IInterface::Ptr (*ConstructInterface)();

  typedef std::vector<ConstructProvider> ProviderRegistry;
  typedef std::vector<ConstructInterface> InterfaceRegistry;

  static ProviderRegistry& ofProvider() { return sProviderRegistry; }

  static InterfaceRegistry& ofInterface() { return sInterfaceRegistry; }

  static void addProvider(ConstructProvider fnConstructor);

  static void addInterface(ConstructInterface fnConstructor);

 private:
  static ProviderRegistry sProviderRegistry;
  static InterfaceRegistry sInterfaceRegistry;
};

template <class T>
class PluginRegister {
 public:
  typedef typename PluginRegistry<T>::ConstructProvider ConstructProvider;
  typedef typename PluginRegistry<T>::ConstructInterface ConstructInterface;

  explicit PluginRegister(ConstructProvider fnConstructor) {
    PluginRegistry<T>::addProvider(fnConstructor);
  }

  explicit PluginRegister(ConstructInterface fnConstructor) {
    PluginRegistry<T>::addInterface(fnConstructor);
  }
};

#define REGISTER_PLUGIN_PROVIDER(T, type)                            \
  PipelinePlugin<T>::IProvider::Ptr _CreatePluginProvider_##type() { \
    return std::make_shared<Interceptor<T, type>>(STRINGIZE(type));  \
  }                                                                  \
  static PluginRegister<T> gPipelinePluginProvider_##type(           \
      _CreatePluginProvider_##type);

#define REGISTER_PLUGIN_INTERFACE(T, type)                             \
  PipelinePlugin<T>::IInterface::Ptr _CreatePluginInterface_##type() { \
    return std::make_shared<type>();                                   \
  }                                                                    \
  static PluginRegister<T> gPipelinePluginInterface_##type(            \
      _CreatePluginInterface_##type);

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSPipelinePlugin
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PIPELINEPLUGIN_H_
