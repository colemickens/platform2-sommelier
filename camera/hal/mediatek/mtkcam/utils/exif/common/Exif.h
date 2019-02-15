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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_H_

#include <exif/common/IBaseExif.h>
//
#ifdef swap16
#undef swap16
#endif
//
#ifdef swap32
#undef swap32
#endif

/*****************************************************************************
 *
 ******************************************************************************/

struct ifdNode_t;
struct zeroIFDList_t;
struct exifIFDList_t;
struct gpsIFDList_t;
struct firstIFDList_t;
struct itopIFDList_t;

/****************************************************************************
 *  Exif Interface
 ****************************************************************************/
class ExifUtils : public IBaseExif {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Instantiation.
  virtual ~ExifUtils();
  ExifUtils();

  virtual bool init(unsigned int const gpsEnFlag);
  virtual bool uninit();
  virtual size_t exifApp1SizeGet() const;

 public:  ////                        Operations.
  virtual unsigned int exifApp1Make(exifImageInfo_t* pexifImgInfo,
                                    exifAPP1Info_t* pexifAPP1Info,
                                    unsigned int* pretSize);
  virtual unsigned int exifAppnMake(unsigned int appn,
                                    unsigned char* paddr,
                                    unsigned char* pdata,
                                    unsigned int dataSize,
                                    unsigned int* pretSize,
                                    unsigned int defaultSize);

 public:  //// exif_ifdinit.cpp
  virtual unsigned int ifdValueInit();
  virtual unsigned int ifdZeroIFDValInit(ifdNode_t* pnode,
                                         struct zeroIFDList_t* plist);
  virtual unsigned int ifdExifIFDValInit(ifdNode_t* pnode,
                                         struct exifIFDList_t* plist);
  virtual unsigned int ifdGpsIFDValInit(ifdNode_t* pnode,
                                        struct gpsIFDList_t* plist);
  virtual unsigned int ifdFirstIFDValInit(ifdNode_t* pnode,
                                          struct firstIFDList_t* plist);
  virtual unsigned int ifdItopIFDValInit(ifdNode_t* pnode,
                                         struct itopIFDList_t* plist);

 public:  //// exif_ifdlist.cpp
  virtual unsigned int ifdListInit();
  virtual unsigned int ifdListUninit();
  virtual ifdNode_t* ifdListNodeAlloc(unsigned int ifdType);
  virtual unsigned int ifdListNodeInsert(unsigned int ifdType,
                                         ifdNode_t* pnode,
                                         void* pdata);
  virtual unsigned int ifdListNodeModify(uint16_t ifdType,
                                         uint16_t tagId,
                                         void* pdata);
  virtual unsigned int ifdListNodeDelete(unsigned int ifdType, uint16_t tagId);
  virtual unsigned int ifdListNodeInfoGet(uint16_t ifdType,
                                          uint16_t tagId,
                                          ifdNode_t** pnode,
                                          MUINTPTR* pbufAddr);
  virtual ifdNode_t* idfListHeadNodeGet(unsigned int ifdType);
  virtual unsigned int ifdListHeadNodeSet(unsigned int ifdType,
                                          ifdNode_t* pheadNode);
  virtual zeroIFDList_t* ifdZeroListGet() const { return mpzeroList; }
  virtual exifIFDList_t* ifdExifListGet() const { return mpexifList; }
  virtual gpsIFDList_t* ifdGpsListGet() const { return mpgpsList; }
  virtual firstIFDList_t* ifdFirstListGet() const { return mpfirstList; }
  virtual itopIFDList_t* ifdItopListGet() const { return mpitopList; }

 public:  //// exif_ifdmisc.cpp
  virtual unsigned int ifdListSizeof() const;
  virtual unsigned char* ifdListValBufGet(unsigned int ifdType);
  virtual unsigned int ifdListValBufSizeof(unsigned int ifdType);
  virtual unsigned int ifdListNodeCntGet(unsigned int ifdType);

 public:  //// exif_hdr.cpp
  virtual unsigned int exifAPP1Write(unsigned char* pdata,
                                     unsigned int* pretSize);
  virtual unsigned int exifSOIWrite(unsigned char* pdata,
                                    unsigned int* pretSize);
  virtual unsigned char* exifHdrTmplAddrGet() const { return mpexifHdrTmplBuf; }
  virtual void exifHdrTmplAddrSet(unsigned char* paddr) {
    mpexifHdrTmplBuf = paddr;
  }

 public:  //// exif_misc.cpp
  virtual uint16_t mySwap16(uint16_t x);
  virtual unsigned int mySwap32(unsigned int x);
  virtual uint16_t mySwap16ByOrder(uint16_t order, uint16_t x);
  virtual unsigned int mySwap32ByOrder(uint16_t order, unsigned int x);
  virtual uint16_t read16(void* psrc);
  virtual unsigned int read32(void* psrc);
  virtual void write16(void* pdst, uint16_t src);
  virtual void write32(void* pdst, unsigned int src);
  virtual unsigned int exifMemcmp(unsigned char* pdst,
                                  unsigned char* psrc,
                                  unsigned int size);
  virtual unsigned int exifApp1Sizeof() const;
  virtual unsigned int exifIFDValueSizeof(uint16_t type, unsigned int count);
  virtual void exifErrPrint(unsigned char* pname, unsigned int err);

 public:  //// exif_make.cpp
  virtual unsigned int exifIsGpsOnFlag() const { return exifGpsEnFlag; }
  virtual unsigned int exifTagUpdate(exifImageInfo_t* pexifImgInfo,
                                     exifAPP1Info_t* pexifAPP1Info);

 private:  ////    Data Members.
  zeroIFDList_t* mpzeroList;
  exifIFDList_t* mpexifList;
  gpsIFDList_t* mpgpsList;
  firstIFDList_t* mpfirstList;
  itopIFDList_t* mpitopList;
  //
  unsigned int exifGpsEnFlag;
  unsigned char* mpexifHdrTmplBuf;
  //
  signed int miLogLevel;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_H_
