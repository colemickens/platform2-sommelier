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

#define LOG_TAG "MtkCam/Metadata"

#include <map>
#include <memory>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/IMetadataTagSet.h>
#include <mtkcam/utils/metadata/client/TagMap.h>
#include <mtkcam/utils/metadata/mtk_metadata_types.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/common.h>

#include <string>

using NSCam::IDefaultMetadataTagSet;
using NSCam::IMetadataTagSet;

using NSCam::IMetadata;
using NSCam::MRational;
using NSCam::MRect;
using NSCam::MSize;
using NSCam::Type2TypeEnum;
using NSCam::TYPE_MDOUBLE;
using NSCam::TYPE_MFLOAT;
using NSCam::TYPE_MINT32;
using NSCam::TYPE_MINT64;
using NSCam::TYPE_MPoint;
using NSCam::TYPE_MRational;
using NSCam::TYPE_MRect;
using NSCam::TYPE_MSize;
using NSCam::TYPE_MUINT8;

/******************************************************************************
 *
 ******************************************************************************/
class DefaultMetadataTagSetImp : public IDefaultMetadataTagSet {
 public:
  DefaultMetadataTagSetImp();
  virtual ~DefaultMetadataTagSetImp() {}

 public:
  virtual IMetadataTagSet const& getTagSet() const;

 protected:
  IMetadataTagSet mData;
};

/******************************************************************************
 *
 ******************************************************************************/
DefaultMetadataTagSetImp::DefaultMetadataTagSetImp() {
#define _IMP_SECTION_INFO_(...)
#undef _IMP_TAG_INFO_

#define _IMP_TAG_INFO_(_tag_, _type_, _name_) \
  mData.addTag(_tag_, _name_, Type2TypeEnum<_type_>::typeEnum);

#include <custom_metadata/custom_metadata_tag_info.inl>

#undef _IMP_TAG_INFO_

#undef _IMP_TAGCONVERT_
#define _IMP_TAGCONVERT_(_android_tag_, _mtk_tag_) \
  mData.addTagMap(_android_tag_, _mtk_tag_);
#if (PLATFORM_SDK_VERSION >= 21)
  ADD_ALL_MEMBERS;
#endif

#undef _IMP_TAGCONVERT_
}

IDefaultMetadataTagSet* IDefaultMetadataTagSet::singleton() {
  static DefaultMetadataTagSetImp* tmp = new DefaultMetadataTagSetImp();
  return tmp;
}

IMetadataTagSet const& DefaultMetadataTagSetImp::getTagSet() const {
  return mData;
}

/******************************************************************************
 *
 ******************************************************************************/
class TagInfo {
 public:
  MUINT32 mTag;
  std::string mName;
  MINT32 mTypeEnum;

  TagInfo(MUINT32 const tag, char const* name, MINT32 const typeEnum)
      : mTag(tag), mName(name), mTypeEnum(typeEnum) {}

  MUINT32 tag() const { return mTag; }
  char const* name() const { return mName.c_str(); }
  MINT32 typeEnum() const { return mTypeEnum; }
};

/******************************************************************************
 *
 ******************************************************************************/
class IMetadataTagSet::Implementor {
 public:  ////                        Instantiation.
  virtual ~Implementor() {}
  Implementor() : android_to_mtk(), mtk_to_android() {}

  Implementor& operator=(Implementor const& other);
  Implementor(Implementor const& other);

  virtual MINT32 getType(MUINT32 tag) const {
    if (mTagInfoMap.find(tag) == mTagInfoMap.end()) {
      return TYPE_UNKNOWN;
    }
    std::shared_ptr<TagInfo> p = mTagInfoMap.at(tag);
    if (p != NULL) {
      return p->typeEnum();
    }
    //
    return TYPE_UNKNOWN;
  }

  virtual char const* getName(unsigned int tag) const {
    if (mTagInfoMap.find(tag) == mTagInfoMap.end()) {
      return NULL;
    }
    std::shared_ptr<TagInfo> p = mTagInfoMap.at(tag);
    if (p != NULL) {
      return p->name();
    }
    return NULL;
  }

  virtual MVOID addTag(MUINT32 tag, char const* name, MINT32 typeEnum) {
    mTagInfoMap.emplace(tag, new TagInfo(tag, name, typeEnum));
  }

  virtual MVOID addTagMap(MUINT32 androidTag, MUINT32 MtkTag) {
    android_to_mtk.emplace(androidTag, MtkTag);
    mtk_to_android.emplace(MtkTag, androidTag);
  }

  virtual MUINT32 getMtkTag(MUINT32 tag) {
    if (tag >= (MUINT32)VENDOR_SECTION_START) {
      return tag;
    }
    if (android_to_mtk.find((MUINT32)tag) == android_to_mtk.end()) {
      return -1;
    }
    return android_to_mtk.at((MUINT32)tag);
  }

  virtual MUINT32 getAndroidTag(MUINT32 tag) {
    if (tag >= (MUINT32)VENDOR_SECTION_START) {
      return tag;
    }
    if (mtk_to_android.find((MUINT32)tag) == mtk_to_android.end()) {
      return -1;
    }
    return mtk_to_android.at((MUINT32)tag);
  }

 protected:
  std::map<MUINT32, std::shared_ptr<TagInfo> > mTagInfoMap;
  std::map<MUINT32, MUINT32> android_to_mtk;
  std::map<MUINT32, MUINT32> mtk_to_android;
};

/******************************************************************************
 *
 ******************************************************************************/
IMetadataTagSet::Implementor::Implementor(
    IMetadataTagSet::Implementor const& other)
    : mTagInfoMap(other.mTagInfoMap),
      android_to_mtk(other.android_to_mtk),
      mtk_to_android(other.mtk_to_android) {}

IMetadataTagSet::Implementor& IMetadataTagSet::Implementor::operator=(
    IMetadataTagSet::Implementor const& other) {
  if (this != &other) {
    // release mMap'storage
    // assign other.mMap's storage pointer to mMap
    // add 1 to storage's sharebuffer
    mTagInfoMap = other.mTagInfoMap;
    android_to_mtk = other.android_to_mtk;
    mtk_to_android = other.mtk_to_android;
  } else {
    CAM_LOGW("this(%p) == other(%p)", this, &other);
  }

  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
IMetadataTagSet::IMetadataTagSet() : mpImp(new Implementor()) {}

IMetadataTagSet::~IMetadataTagSet() {
  if (mpImp) {
    delete mpImp;
  }
}

IMetadataTagSet::IMetadataTagSet(IMetadataTagSet const& other)
    : mpImp(new Implementor(*(other.mpImp))) {}

IMetadataTagSet& IMetadataTagSet::operator=(IMetadataTagSet const& other) {
  if (this != &other) {
    delete mpImp;
    mpImp = new Implementor(*(other.mpImp));
  } else {
    CAM_LOGW("this(%p) == other(%p)", this, &other);
  }

  return *this;
}

MINT32
IMetadataTagSet::getType(MUINT32 tag) const {
  if (mpImp) {
    return mpImp->getType(tag);
  }
  return 0;
}

char const* IMetadataTagSet::getName(unsigned int tag) const {
  if (mpImp) {
    return mpImp->getName(tag);
  }
  return NULL;
}

MVOID
IMetadataTagSet::addTag(MUINT32 tag, char const* name, MINT32 typeEnum) {
  if (mpImp) {
    mpImp->addTag(tag, name, typeEnum);
  }
}

MVOID
IMetadataTagSet::addTagMap(MUINT32 androidTag, MUINT32 MtkTag) {
  if (mpImp) {
    mpImp->addTagMap(androidTag, MtkTag);
  }
}

MUINT32
IMetadataTagSet::getMtkTag(MUINT32 tag) const {
#if (PLATFORM_SDK_VERSION >= 21)
  if (mpImp) {
    return mpImp->getMtkTag(tag);
  }
#endif
  return -1;
}

MUINT32
IMetadataTagSet::getAndroidTag(MUINT32 tag) const {
#if (PLATFORM_SDK_VERSION >= 21)
  if (mpImp) {
    return mpImp->getAndroidTag(tag);
  }
#endif
  return -1;
}
