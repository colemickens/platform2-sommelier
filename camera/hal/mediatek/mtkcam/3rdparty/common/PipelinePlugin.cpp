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
#include <map>
#include <memory>
#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
#include <mtkcam/3rdparty/plugin/PipelinePluginType.h>
#include <mtkcam/utils/std/Format.h>
#include <vector>

using NSCam::Utils::Format::queryImageFormatName;
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

template <typename T>
typename PluginRegistry<T>::ProviderRegistry
    PluginRegistry<T>::sProviderRegistry;

template <typename T>
typename PluginRegistry<T>::InterfaceRegistry
    PluginRegistry<T>::sInterfaceRegistry;

template <typename T>
void PluginRegistry<T>::addProvider(ConstructProvider fnConstructor) {
  ProviderRegistry& reg = ofProvider();
  reg.push_back(fnConstructor);
}

template <typename T>
void PluginRegistry<T>::addInterface(ConstructInterface fnConstructor) {
  InterfaceRegistry& reg = ofInterface();
  reg.push_back(fnConstructor);
}

template class PluginRegistry<Raw>;
template class PluginRegistry<Yuv>;
template class PluginRegistry<Join>;

/******************************************************************************
 * Template Implementation
 ******************************************************************************/

template <typename T>
std::map<MINT, typename PipelinePlugin<T>::WeakPtr>
    PipelinePlugin<T>::mInstances;

template <typename T>
const std::vector<typename PipelinePlugin<T>::IProvider::Ptr>&
PipelinePlugin<T>::getProviders() {
  typedef PluginRegistry<T> Registry;

  if (mpProviders.size() == 0) {
    for (auto c : Registry::ofProvider()) {
      typename IProvider::Ptr provider = c();
      provider->set(mOpenId);
      mpProviders.push_back(provider);
    }
  }

  return mpProviders;
}

template <typename T>
typename PipelinePlugin<T>::Request::Ptr PipelinePlugin<T>::createRequest() {
  return std::make_shared<PipelinePlugin<T>::Request>();
}

template <typename T>
typename PipelinePlugin<T>::IInterface::Ptr PipelinePlugin<T>::getInterface() {
  typedef PluginRegistry<T> Registry;

  if (mpInterface == nullptr) {
    for (auto c : Registry::ofInterface()) {
      mpInterface = c();
      break;
    }
  }

  return mpInterface;
}

template <typename T>
const typename PipelinePlugin<T>::Selection& PipelinePlugin<T>::getSelection(
    typename IProvider::Ptr provider) {
  static Selection sel;
  auto intf = getInterface();
  if (intf != nullptr) {
    Selection sel;
    intf->offer(&sel);
    provider->negotiate(&sel);
  }

  return sel;
}

template <typename T>
typename PipelinePlugin<T>::Selection::Ptr
PipelinePlugin<T>::createSelection() {
  return std::make_shared<PipelinePlugin<T>::Selection>();
}

template <typename T>
MVOID PipelinePlugin<T>::pushSelection(typename IProvider::Ptr provider,
                                       typename Selection::Ptr selection) {
  std::lock_guard<std::mutex> l(mMutex);
  mpSelections[provider].push(selection);
}

template <typename T>
typename PipelinePlugin<T>::Selection::Ptr PipelinePlugin<T>::popSelection(
    typename IProvider::Ptr provider) {
  std::lock_guard<std::mutex> l(mMutex);
  auto& selections = mpSelections[provider];
  if (selections.empty()) {
    return nullptr;
  }

  auto sel = selections.front();
  selections.pop();

  return sel;
}

template <typename T>
MVOID PipelinePlugin<T>::dump(std::ostream& os) {}

template <typename T>
typename PipelinePlugin<T>::Ptr PipelinePlugin<T>::getInstance(
    MINT32 iOpenId, MINT32 iOpenId2) {
  MINT index = iOpenId;
  // Hash to unique key
  if (iOpenId2 > 0) {
    index += (iOpenId2 + 1) * 100;
  }

  Ptr sp = mInstances[index].lock();
  if (sp == nullptr) {
    sp = std::make_shared<PipelinePlugin<T>>(iOpenId, iOpenId2);
    mInstances[index] = sp;
  }

  return sp;
}

template <typename T>
PipelinePlugin<T>::~PipelinePlugin() {
  mpInterface = nullptr;
  mpProviders.clear();
}

template class PipelinePlugin<Raw>;
template class PipelinePlugin<Yuv>;
template class PipelinePlugin<Join>;

/******************************************************************************
 *
 ******************************************************************************/
class MetadataSelection::Implementor {
 public:
  Implementor() : mRequired(MFALSE) {}

  MBOOL mRequired;
  MetadataPtr mControl;
  MetadataPtr mAddtional;
  MetadataPtr mDummy;
};

MetadataSelection::MetadataSelection() : mImpl(new Implementor()) {}

MetadataSelection::~MetadataSelection() {
  delete mImpl;
}

MetadataSelection& MetadataSelection::setRequired(MBOOL required) {
  mImpl->mRequired = required;
  return *this;
}

MBOOL MetadataSelection::getRequired() const {
  return mImpl->mRequired;
}

MetadataSelection& MetadataSelection::setControl(MetadataPtr control) {
  mImpl->mControl = control;
  return *this;
}

MetadataSelection& MetadataSelection::setAddtional(MetadataPtr addtional) {
  mImpl->mAddtional = addtional;
  return *this;
}

MetadataSelection& MetadataSelection::setDummy(MetadataPtr dummy) {
  mImpl->mDummy = dummy;
  return *this;
}

MetadataPtr MetadataSelection::getControl() const {
  return mImpl->mControl;
}

MetadataPtr MetadataSelection::getAddtional() const {
  return mImpl->mAddtional;
}

MetadataPtr MetadataSelection::getDummy() const {
  return mImpl->mDummy;
}

MVOID MetadataSelection::dump(std::ostream& os) const {
  if (!mImpl->mRequired) {
    os << "{ non-required }";
  } else {
    os << "{ required }";
  }
}

/******************************************************************************
 *
 ******************************************************************************/
class BufferSelection::Implementor {
 public:
  Implementor()
      : mRequired(MFALSE),
        mOptional(MFALSE),
        mSpecifiedSize(0, 0),
        mAlignment(0, 0),
        mDirtyFormats(MTRUE),
        mDirtySizes(MTRUE) {}

  MBOOL mRequired;
  MBOOL mOptional;
  std::vector<MINT> mAcceptedFormats;
  std::vector<MINT> mAcceptedSizes;
  MSize mSpecifiedSize;
  MSize mAlignment;
  std::vector<MINT> mSupportFormats;
  std::vector<MINT> mSupportSizes;
  std::vector<MINT> mFormats;
  std::vector<MINT> mSizes;
  MBOOL mDirtyFormats;
  MBOOL mDirtySizes;
};

BufferSelection::BufferSelection() : mImpl(new Implementor()) {}

BufferSelection::~BufferSelection() {
  delete mImpl;
}

BufferSelection& BufferSelection::setRequired(MBOOL required) {
  mImpl->mRequired = required;
  return *this;
}

BufferSelection& BufferSelection::setOptional(MBOOL optional) {
  mImpl->mOptional = optional;
  return *this;
}

MBOOL BufferSelection::getRequired() const {
  return mImpl->mRequired;
}

MBOOL BufferSelection::getOptional() const {
  return mImpl->mOptional;
}

BufferSelection& BufferSelection::addAcceptedFormat(MINT fmt) {
  mImpl->mAcceptedFormats.push_back(fmt);
  mImpl->mDirtyFormats = MTRUE;
  return *this;
}

BufferSelection& BufferSelection::addAcceptedSize(MINT sz) {
  mImpl->mAcceptedSizes.push_back(sz);
  mImpl->mDirtySizes = MTRUE;
  return *this;
}

BufferSelection& BufferSelection::setSpecifiedSize(const MSize& sz) {
  mImpl->mSpecifiedSize = sz;
  return *this;
}

BufferSelection& BufferSelection::setAlignment(MUINT32 width, MUINT32 height) {
  mImpl->mAlignment.w = width;
  mImpl->mAlignment.h = height;
  return *this;
}

const MSize& BufferSelection::getSpecifiedSize() const {
  return mImpl->mSpecifiedSize;
}

MVOID BufferSelection::getAlignment(MUINT32& width, MUINT32& height) const {
  width = mImpl->mAlignment.w;
  height = mImpl->mAlignment.h;
}

MBOOL BufferSelection::isValid() const {
  return getSizes().size() > 0 && getFormats().size() > 0;
}

BufferSelection& BufferSelection::addSupportFormat(MINT fmt) {
  mImpl->mSupportFormats.push_back(fmt);
  mImpl->mDirtyFormats = MTRUE;
  return *this;
}

BufferSelection& BufferSelection::addSupportSize(MINT sz) {
  mImpl->mSupportSizes.push_back(sz);
  mImpl->mDirtySizes = MTRUE;
  return *this;
}

const std::vector<MINT>& BufferSelection::getFormats() const {
  auto& formats = mImpl->mFormats;
  if (mImpl->mDirtyFormats) {
    formats.clear();

    auto& rSupport = mImpl->mSupportFormats;
    for (MINT fmt : mImpl->mAcceptedFormats) {
      if (find(rSupport.begin(), rSupport.end(), fmt) != rSupport.end()) {
        formats.push_back(fmt);
      }
    }
  }

  return formats;
}

const std::vector<MINT>& BufferSelection::getSizes() const {
  auto& sizes = mImpl->mSizes;
  if (mImpl->mDirtySizes) {
    sizes.clear();

    auto& rSupport = mImpl->mSupportSizes;
    for (MINT fmt : mImpl->mAcceptedSizes) {
      if (find(rSupport.begin(), rSupport.end(), fmt) != rSupport.end()) {
        sizes.push_back(fmt);
      }
    }
  }

  return sizes;
}

MVOID BufferSelection::dump(std::ostream& os) const {
  if (mImpl->mRequired) {
    os << "{ required";
  } else if (!mImpl->mRequired) {
    os << "{ non-requireda";
  }

  // print support format
  bool firstElement = true;
  for (auto fmt : mImpl->mSupportFormats) {
    if (firstElement) {
      os << ", support formats: [";
      firstElement = false;
    } else {
      os << ", ";
    }

    os << queryImageFormatName(fmt);
  }
  if (!firstElement) {
    os << "]";
  }

  auto stringizeImageSize = [](MINT s) -> const char* {
    switch (s) {
      case eImgSize_Full:
        return "Full";
      case eImgSize_Resized:
        return "Resized";
      case eImgSize_Specified:
        return "Specified";
    }
    return "Unknown";
  };

  // print support size
  firstElement = true;
  for (auto v : mImpl->mSupportSizes) {
    if (firstElement) {
      os << ", support sizes: [";
      firstElement = false;
    } else {
      os << ", ";
    }

    os << stringizeImageSize(v);
  }
  if (!firstElement) {
    os << "]";
  }

  // print accept format
  firstElement = true;
  for (auto fmt : mImpl->mAcceptedFormats) {
    if (firstElement) {
      os << ", accepted formats: [";
      firstElement = false;
    } else {
      os << ", ";
    }

    os << queryImageFormatName(fmt);
  }
  if (!firstElement) {
    os << "]";
  }

  // print accept size
  firstElement = true;
  for (auto v : mImpl->mAcceptedSizes) {
    if (firstElement) {
      os << ", accepted sizes: [";
      firstElement = false;
    } else {
      os << ", ";
    }

    os << stringizeImageSize(v);
  }
  if (!firstElement) {
    os << "]";
  }
  // print specific size
  if (!!mImpl->mSpecifiedSize) {
    os << ", specific size: (" << mImpl->mSpecifiedSize.w << "x"
       << mImpl->mSpecifiedSize.h << ")";
  }

  if (!!mImpl->mAlignment) {
    os << ", alignment: (" << mImpl->mAlignment.w << "x" << mImpl->mAlignment.h
       << ")";
  }
  os << " }";
}

/******************************************************************************
 * Object Printer
 ******************************************************************************/
std::ostream& operator<<(std::ostream& os,
                         const std::shared_ptr<BufferHandle> hnd) {
  if (hnd == nullptr) {
    return os << "{ null }";
  }

  hnd->dump(os);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::shared_ptr<MetadataHandle> hnd) {
  if (hnd == nullptr) {
    return os << "{ null }";
  }

  hnd->dump(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const BufferSelection& sel) {
  sel.dump(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const MetadataSelection& sel) {
  sel.dump(os);
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSPipelinePlugin
};  // namespace NSCam
