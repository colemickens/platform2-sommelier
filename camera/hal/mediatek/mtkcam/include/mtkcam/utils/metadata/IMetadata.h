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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_IMETADATA_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_IMETADATA_H_
//
#include <memory>
#include <mtkcam/def/BasicTypes.h>
#include <mtkcam/def/common.h>
#include <mtkcam/def/Errors.h>
#include <mtkcam/def/TypeManip.h>
#include <mtkcam/def/UITypes.h>
//
#include <mutex>
#include <utility>
#include <vector>
//
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Camera Metadata Interface
 ******************************************************************************/
class VISIBILITY_PUBLIC IMetadata {
 public:
  // IMetadata::Memory is a buffer chunk module that:
  //
  // 1. dynamic allocation (using heap).
  // 2. continuous, can copy all data chunk via retrieving raw pointers from
  //    array() or editArray(), notice that, array() and editArray() are not
  //    thread-safe method.
  // 3. copy-on-write, caller calls methods whichs are not marked as const,
  //    the internal buffer will be copied immediately.
  // 4. reentraint.
  //
  // optional: thread-safe, except method array() and editArray().
  //           if programmer wants thread-safe support, define
  //           IMETADATA_MEMORY_THREAD_SAFE_SUPPORT to 1
#define IMETADATA_MEMORY_THREAD_SAFE_SUPPORT 1
  class VISIBILITY_PUBLIC Memory {
    // interfaces
   public:
    size_t size() const;
    void resize(const size_t size, uint8_t default_val = 0);
    size_t append(const Memory& other);
    const uint8_t* array() const;
    uint8_t* editArray();
    uint8_t itemAt(size_t index) const;
    void clear();

    std::vector<uint8_t>::iterator begin();
    std::vector<uint8_t>::iterator end();

    std::vector<uint8_t>::const_iterator cbegin() const;
    std::vector<uint8_t>::const_iterator cend() const;

    // android support
    size_t appendVector(const Memory& other);
    size_t appendVector(const std::vector<uint8_t>& v);

   public:
    Memory();
    Memory(const Memory& other);
    Memory(Memory&& other);

   public:
    Memory& operator=(Memory&& other);
    Memory& operator=(const Memory& other);
    const bool operator==(const Memory& other) const;
    const bool operator!=(const Memory& other) const;

   private:
    // try to duplicate shared data into an unique new one. if the _data is
    // unique, this function do nothing.
    void dup_data_locked();

    // attributes
   private:
    // we shared _data if without editing Memory (copy-on-write)
    mutable std::shared_ptr<std::vector<uint8_t> > _data;
#if IMETADATA_MEMORY_THREAD_SAFE_SUPPORT
    mutable std::mutex _data_lock;
#endif
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  typedef MUINT32 Tag_t;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Entry Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  class VISIBILITY_PUBLIC IEntry {
   public:  ////                    Definitions.
    enum { BAD_TAG = -1U };

   public:  ////                    Instantiation.
    virtual ~IEntry();
    explicit IEntry(Tag_t tag = BAD_TAG);

    /**
     * Copy constructor and copy assignment.
     */
    IEntry(IEntry const& other);
    IEntry& operator=(IEntry const& other);

    /**
     * Compare two IEntry instances, if the type and data are the same, this
     * method returns true. But if the type is IMetadata, this method always
     * returns true. Note: We don't support compare two IEntry<IMetadata>.
     */
    bool operator==(IEntry const& other) const;

    bool operator!=(IEntry const& other) const;

   public:  ////                    Accessors.
    /**
     * Return the tag.
     */
    virtual MUINT32 tag() const;

    /**
     * Return the type.
     */
    virtual MINT32 type() const;
    /**
     * Return the start address of IEntry's container.
     *  @note The memory chunk of container is always continuous.
     */
    virtual const void* data() const;
    /**
     * Check to see whether it is empty (no items) or not.
     */
    virtual MBOOL isEmpty() const;

    /**
     * Return the number of items.
     */
    virtual MUINT count() const;

    /**
     * Return how many items can be stored without reallocating the backing
     * store.
     */
    virtual MUINT capacity() const;

    /**
     * Set the capacity.
     */
    virtual MBOOL setCapacity(MUINT size);

   public:  ////                    Operations.
    /**
     * Clear all items.
     * Note: Copy-on write.
     */
    virtual MVOID clear();

    /**
     * Delete an item at a given index.
     * Note: Copy-on write.
     */
    virtual NSCam::MERROR removeAt(MUINT index);

#define IMETADATA_IENTRY_OPS_DECLARATION(_T)                               \
  virtual MVOID push_back(_T const& item, Type2Type<_T>);                  \
  virtual MVOID replaceItemAt(MUINT index, _T const& item, Type2Type<_T>); \
  virtual _T itemAt(MUINT index, Type2Type<_T>) const;

    IMETADATA_IENTRY_OPS_DECLARATION(MUINT8)
    IMETADATA_IENTRY_OPS_DECLARATION(MINT32)
    IMETADATA_IENTRY_OPS_DECLARATION(MFLOAT)
    IMETADATA_IENTRY_OPS_DECLARATION(MINT64)
    IMETADATA_IENTRY_OPS_DECLARATION(MDOUBLE)
    IMETADATA_IENTRY_OPS_DECLARATION(MRational)
    IMETADATA_IENTRY_OPS_DECLARATION(MPoint)
    IMETADATA_IENTRY_OPS_DECLARATION(MSize)
    IMETADATA_IENTRY_OPS_DECLARATION(MRect)
    IMETADATA_IENTRY_OPS_DECLARATION(IMetadata)
    IMETADATA_IENTRY_OPS_DECLARATION(Memory)
#undef IMETADATA_IENTRY_OPS_DECLARATION

    /**
     * Get index of target value in Entry.
     * If the target value doesn't exist, this function returns -1.
     *
     * @param entry [in]    The input entry for look up
     * @param target [in]   Target value in input entry ( Must cast to entry
     * value supported type, eg MDOUBLE)
     * @return              index of item  if target value found. Otherwise,
     * return -1.
     */
    template <typename T>
    static int indexOf(const IEntry& entry, const T& target) {
      for (size_t i = 0; i < entry.count(); i++) {
        if (entry.itemAt(i, Type2Type<T>()) == target) {
          return static_cast<int>(i);
        }
      }
      return -1;
    }

   protected:  ////                    Implementor.
    class Implementor;
    Implementor* mpImp;
    mutable std::mutex mEntryLock;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Instantiation.
  virtual ~IMetadata();
  IMetadata();

  /**
   * Copy constructor and copy assignment.
   */
  IMetadata(IMetadata const& other);
  IMetadata& operator=(IMetadata const& other);

  /**
   * operators
   */
  IMetadata& operator+=(IMetadata const& other);
  IMetadata const operator+(IMetadata const& other);

 public:  ////                        Accessors.
  /**
   * Check to see whether it is empty (no entries) or not.
   */
  virtual MBOOL isEmpty() const;

  /**
   * Return the number of entries.
   */
  virtual MUINT count() const;

 public:  ////                        Operations.
  /**
   * Add the given IMetadata, if the key exists both in the current and the
   * given one, the value will be replaced by the given one. E.g.: C = A.add(B);
   * --> C = A + B; Return the merged IMetadata. Note: Copy-on write.
   */
  virtual IMetadata add(IMetadata const& other) const;

  /**
   * Append the given IMetadata, if the key exists both in the current
   * and given one, the value will be replaced by the given one.
   * E.g.: A.append(B); --> A = A + B;
   * Return the appended IMetadata.
   * Note: Copy-on write.
   */
  virtual MVOID append(IMetadata const& other);

  /**
   * Clear all entries.
   * Note: Copy-on write.
   */
  virtual MVOID clear();

  /**
   * Delete an entry by tag.
   * Note: Copy-on write.
   */
  virtual NSCam::status_t remove(Tag_t tag);

  /**
   * Sort all entries for faster find.
   * Note: Copy-on write.
   */
  virtual NSCam::status_t sort();

  /**
   * Update metadata entry. An entry will be created if it doesn't exist
   * already. Note: Copy-on write.
   */
  virtual NSCam::status_t update(Tag_t tag, IEntry const& entry);

  /**
   * Get metadata entry by tag, with no editing.
   */
  virtual IEntry entryFor(Tag_t tag) const;

  /**
   * Get metadata entry by index, with no editing.
   */
  virtual IEntry entryAt(MUINT index) const;

  /**
   * Take metadata entry by tag. After invoked this method, the metadata enty of
   * tag in metadata will be removed. Note: Without all element copy.
   * Complexity: O(log N)
   */
  virtual IEntry takeEntryFor(Tag_t tag);

  /**
   * Take metadata entry by index. After invoked this method, the index-th
   * metadata entry will be removed. Note: Without all element copy. Complexity:
   * O(1)
   */
  virtual IEntry takeEntryAt(MUINT index);

  /**
   * Flatten IMetadata.
   */
  virtual ssize_t flatten(void* buf, size_t buf_size) const;

  /**
   * Unflatten IMetadata.
   */
  virtual ssize_t unflatten(void* buf, size_t buf_size);

  virtual void dump(int layer = 0);

 public:  ////                        Helpers.
  /**
   * Set metadata with given tag and value.
   * Add a pair a tag with its value into metadata (and replace the one that is
   * there).
   *
   * @param metadata [in,out]    The metadata to be updated
   * @param tag [in]             The tag to update
   * @param val [in]             The value to update
   * @return                     Entry is set or not
   * @retval                     OK on success
   * @retval                     INVALID_OPERATION if metadata is null
   * @retval                     BAD_INDEX if out of range
   * @retval                     NO_MEMORY if out of memory
   */
  template <typename T>
  static NSCam::status_t setEntry(IMetadata* metadata,
                                  MUINT32 const tag,
                                  T const& val) {
    if (nullptr == metadata) {
      return -EINVAL;  // BAD_VALUE
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    int err = metadata->update(entry.tag(), entry);
    return (err >= 0) ? 0 : err;
  }

  /**
   * Get metadata with given tag and value.
   * If the tag doesn't exist, this function returns false.
   *
   * @param metadata [in]    The constant pointer of IMetadata to look up
   * @param tag [in]         The tag to get
   * @param val [out]        Call by reference output if found
   * @param index [in]       Index of item in Entry you want to look up. Default
   * value is 0.
   * @return                 true if the corresponding entry exists
   */
  template <typename T>
  static bool getEntry(const IMetadata* metadata,
                       MUINT32 const tag,
                       T* val,
                       size_t index = 0) {
    if (nullptr == metadata) {
      return false;
    }

    IMetadata::IEntry entry = metadata->entryFor(tag);
    if (entry.count() > index) {
      *val = entry.itemAt(index, Type2Type<T>());
      return true;
    }

    return false;
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Bridge.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                        Implementor.
  class Implementor;
  Implementor* mpImp;
  mutable std::mutex mMetadataLock;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_IMETADATA_H_
