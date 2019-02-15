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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_ISTREAMINFOSETCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_ISTREAMINFOSETCONTROL_H_
//
#include <map>
#include <memory>

#include <mtkcam/pipeline/stream/IStreamInfo.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * An interface of stream info set control.
 */
class SimpleStreamInfoSetControl : public virtual IStreamInfoSet {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  template <class IStreamInfoT>
  struct Map : public IMap<IStreamInfoT>,
               public std::map<StreamId_T, std::shared_ptr<IStreamInfoT>> {
   public:  ////
    typedef std::map<StreamId_T, std::shared_ptr<IStreamInfoT>> ParentT;
    typedef typename ParentT::key_type key_type;
    typedef typename ParentT::mapped_type value_type;

   public:  ////                        Operations.
    ~Map() {}
    virtual size_t size() const { return ParentT::size(); }

    virtual ssize_t indexOfKey(StreamId_T id) const {
      auto itIdx = ParentT::find(id);
      if (itIdx != ParentT::end()) {
        return std::distance(ParentT::begin(), itIdx);
      }
      return -ENOENT;
    }

    virtual std::shared_ptr<IStreamInfoT> valueFor(StreamId_T id) const {
      return ParentT::at(id);
    }

    virtual std::shared_ptr<IStreamInfoT> valueAt(size_t index) const {
      auto itIdx = ParentT::begin();
      if (ParentT::size() > index) {
        std::advance(itIdx, index);
        return itIdx->second;
      }
      return nullptr;
    }

    ssize_t addStream(value_type const& p) {
      ParentT::emplace(p->getStreamId(), p);
      return ParentT::size() - 1;
    }
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                            Data Members.
  std::shared_ptr<Map<IMetaStreamInfo>> mpMeta;
  std::shared_ptr<Map<IImageStreamInfo>> mpImage;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
  SimpleStreamInfoSetControl()
      : mpMeta(std::make_shared<Map<IMetaStreamInfo>>()),
        mpImage(std::make_shared<Map<IImageStreamInfo>>()) {}
  virtual ~SimpleStreamInfoSetControl() {}
  virtual Map<IMetaStreamInfo> const& getMeta() const { return *mpMeta; }
  virtual Map<IImageStreamInfo> const& getImage() const { return *mpImage; }

  virtual Map<IMetaStreamInfo>& editMeta() { return *mpMeta; }
  virtual Map<IImageStreamInfo>& editImage() { return *mpImage; }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamInfoSet Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
#define _IMPLEMENT_(_type_)                                                   \
  virtual std::shared_ptr<IMap<I##_type_##StreamInfo>> get##_type_##InfoMap() \
      const {                                                                 \
    return mp##_type_;                                                        \
  }                                                                           \
                                                                              \
  virtual size_t get##_type_##InfoNum() const { return mp##_type_->size(); }  \
                                                                              \
  virtual std::shared_ptr<I##_type_##StreamInfo> get##_type_##InfoFor(        \
      StreamId_T id) const {                                                  \
    return mp##_type_->valueFor(id);                                          \
  }                                                                           \
                                                                              \
  virtual std::shared_ptr<I##_type_##StreamInfo> get##_type_##InfoAt(         \
      size_t index) const {                                                   \
    return mp##_type_->valueAt(index);                                        \
  }

  _IMPLEMENT_(Meta)
  _IMPLEMENT_(Image)

#undef _IMPLEMENT_
};

/**
 * An interface of stream info set control.
 */
class IStreamInfoSetControl : public IStreamInfoSet {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamInfoSetControl Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Definitions.
  template <class IStreamInfoT>
  struct Map : public std::map<StreamId_T, std::shared_ptr<IStreamInfoT>> {
   public:  ////
    typedef std::map<StreamId_T, std::shared_ptr<IStreamInfoT>> ParentT;

    typedef typename ParentT::key_type key_type;
    typedef typename ParentT::mapped_type value_type;

   public:  ////                        Operations.
    ssize_t addStream(value_type const& p) {
      auto iter = ParentT::emplace(p->getStreamId(), p);
      return std::distance(ParentT::begin(), iter.first);
    }
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                            Definitions.
  template <class IStreamInfoT>
  struct Set : public IMap<IStreamInfoT> {
    typedef Map<IStreamInfoT> MapT;
    MapT mApp;
    MapT mHal;

    ~Set() {}
    size_t size() const { return mApp.size() + mHal.size(); }

    virtual ssize_t indexOfKey(StreamId_T id) const {
      auto itIdx = mApp.find(id);
      if (itIdx != mApp.end()) {
        return std::distance(mApp.begin(), itIdx);
      }
      itIdx = mHal.find(id);
      if (itIdx != mHal.end()) {
        return std::distance(mHal.begin(), itIdx) + mApp.size();
      }
      return -ENOENT;
    }

    virtual std::shared_ptr<IStreamInfoT> valueFor(StreamId_T id) const {
      auto itIdx = mApp.find(id);
      if (itIdx != mApp.end()) {
        return mApp.at(id);
      }
      itIdx = mHal.find(id);
      if (itIdx != mHal.end()) {
        return mHal.at(id);
      }
      return 0;
    }

    virtual std::shared_ptr<IStreamInfoT> valueAt(size_t index) const {
      auto itIdx = mApp.begin();
      if (mApp.size() > index) {
        std::advance(itIdx, index);
        return itIdx->second;
      }

      index -= mApp.size();
      itIdx = mHal.begin();
      if (mHal.size() > index) {
        std::advance(itIdx, index);
        return itIdx->second;
      }
      return 0;
    }
  };

 protected:  ////                            Data Members.
  std::shared_ptr<Set<IMetaStreamInfo>> mpSetMeta;
  std::shared_ptr<Set<IImageStreamInfo>> mpSetImage;

 public:  ////                            Operations.
  IStreamInfoSetControl()
      : mpSetMeta(std::make_shared<Set<IMetaStreamInfo>>()),
        mpSetImage(std::make_shared<Set<IImageStreamInfo>>()) {}
  virtual ~IStreamInfoSetControl() {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamInfoSetControl Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
  static std::shared_ptr<IStreamInfoSetControl> create() {
    auto ptr = std::make_shared<IStreamInfoSetControl>();
    return ptr;
  }

  virtual Map<IMetaStreamInfo> const& getAppMeta() const {
    return mpSetMeta->mApp;
  }
  virtual Map<IMetaStreamInfo> const& getHalMeta() const {
    return mpSetMeta->mHal;
  }
  virtual Map<IImageStreamInfo> const& getAppImage() const {
    return mpSetImage->mApp;
  }
  virtual Map<IImageStreamInfo> const& getHalImage() const {
    return mpSetImage->mHal;
  }

  virtual Map<IMetaStreamInfo>& editAppMeta() { return mpSetMeta->mApp; }
  virtual Map<IMetaStreamInfo>& editHalMeta() { return mpSetMeta->mHal; }
  virtual Map<IImageStreamInfo>& editAppImage() { return mpSetImage->mApp; }
  virtual Map<IImageStreamInfo>& editHalImage() { return mpSetImage->mHal; }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamInfoSet Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
#define _IMPLEMENT_(_type_)                                                   \
  virtual std::shared_ptr<IMap<I##_type_##StreamInfo>> get##_type_##InfoMap() \
      const {                                                                 \
    return mpSet##_type_;                                                     \
  }                                                                           \
                                                                              \
  virtual size_t get##_type_##InfoNum() const {                               \
    return mpSet##_type_->size();                                             \
  }                                                                           \
                                                                              \
  virtual std::shared_ptr<I##_type_##StreamInfo> get##_type_##InfoFor(        \
      StreamId_T id) const {                                                  \
    return mpSet##_type_->valueFor(id);                                       \
  }                                                                           \
                                                                              \
  virtual std::shared_ptr<I##_type_##StreamInfo> get##_type_##InfoAt(         \
      size_t index) const {                                                   \
    return mpSet##_type_->valueAt(index);                                     \
  }

  _IMPLEMENT_(Meta)   // IMetaStreamInfo, mAppMeta,  mHalMeta
  _IMPLEMENT_(Image)  // IImageStreamInfo, mAppImage, mHalImage

#undef _IMPLEMENT_
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_ISTREAMINFOSETCONTROL_H_
