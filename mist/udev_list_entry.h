// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_UDEV_LIST_ENTRY_H_
#define MIST_UDEV_LIST_ENTRY_H_

#include <base/basictypes.h>

struct udev_list_entry;

namespace mist {

// A udev list entry, which wraps a udev_list_entry C struct from libudev and
// related library functions into a C++ object.
class UdevListEntry {
 public:
  // Constructs a UdevListEntry object by taking a raw pointer to a
  // udev_list_entry struct as |list_entry|. The ownership of |list_entry| is
  // not transferred, and thus it should outlive this object.
  explicit UdevListEntry(udev_list_entry* list_entry);

  virtual ~UdevListEntry();

  // Wraps udev_list_entry_get_next(). The returned UdevListEntry object is not
  // managed and should be deleted by the caller after use.
  virtual UdevListEntry* GetNext() const;

  // Wraps udev_list_entry_get_by_name(). The returned UdevListEntry object is
  // not managed and should be deleted by the caller after use.
  virtual UdevListEntry* GetByName(const char* name) const;

  // Wraps udev_list_entry_get_name().
  virtual const char* GetName() const;

  // Wraps udev_list_entry_get_value().
  virtual const char* GetValue() const;

 private:
  // Allows MockUdevListEntry to invoke the private default constructor below.
  friend class MockUdevListEntry;

  // Constructs a UdevListEntry object without referencing a udev_list_entry
  // struct, which is only allowed to be called by MockUdevListEntry.
  UdevListEntry();

  udev_list_entry* const list_entry_;

  DISALLOW_COPY_AND_ASSIGN(UdevListEntry);
};

}  // namespace mist

#endif  // MIST_UDEV_LIST_ENTRY_H_
