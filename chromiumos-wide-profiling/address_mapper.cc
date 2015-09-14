// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/address_mapper.h"

#include <vector>

#include "base/logging.h"

#include "chromiumos-wide-profiling/limits.h"

namespace quipper {

AddressMapper::AddressMapper(const AddressMapper& source) {
  mappings_ = source.mappings_;
}

bool AddressMapper::MapWithID(const uint64_t real_addr,
                              const uint64_t size,
                              const uint64_t id,
                              const uint64_t offset_base,
                              bool remove_existing_mappings) {
  MappedRange range;
  range.real_addr = real_addr;
  range.size = size;
  range.id = id;
  range.offset_base = offset_base;

  if (size == 0) {
    LOG(ERROR) << "Must allocate a nonzero-length address range.";
    return false;
  }

  // Check that this mapping does not overflow the address space.
  if (real_addr + size - 1 != kUint64Max &&
      !(real_addr + size > real_addr)) {
    DumpToLog();
    LOG(ERROR) << "Address mapping at " << std::hex << real_addr
               << " with size " << std::hex << size << " overflows.";
    return false;
  }

  // Check for collision with an existing mapping.  This must be an overlap that
  // does not result in one range being completely covered by another
  MappingList::iterator iter;
  std::vector<MappingList::iterator> mappings_to_delete;
  MappingList::iterator old_range_iter = mappings_.end();
  for (iter = mappings_.begin(); iter != mappings_.end(); ++iter) {
    if (!iter->Intersects(range))
      continue;
    // Quit if existing ranges that collide aren't supposed to be removed.
    if (!remove_existing_mappings)
      return false;
    if (old_range_iter == mappings_.end() && iter->Covers(range)
        && iter->size > range.size) {
      old_range_iter = iter;
      continue;
    }
    mappings_to_delete.push_back(iter);
  }

  for (MappingList::iterator mapping_iter : mappings_to_delete)
    Unmap(mapping_iter);
  mappings_to_delete.clear();

  // Otherwise check for this range being covered by another range.  If that
  // happens, split or reduce the existing range to make room.
  if (old_range_iter != mappings_.end()) {
    // Make a copy of the old mapping before removing it.
    const MappedRange old_range = *old_range_iter;
    Unmap(old_range_iter);

    uint64_t gap_before = range.real_addr - old_range.real_addr;
    uint64_t gap_after = (old_range.real_addr + old_range.size) -
                         (range.real_addr + range.size);

    if (gap_before) {
      if (!MapWithID(old_range.real_addr, gap_before, old_range.id,
                     old_range.offset_base, false)) {
        LOG(ERROR) << "Could not map old range from " << std::hex
                   << old_range.real_addr << " to "
                   << old_range.real_addr + gap_before;
        return false;
      }
    }

    if (!MapWithID(range.real_addr, range.size, id, offset_base, false)) {
      LOG(ERROR) << "Could not map new range at " << std::hex << range.real_addr
                 << " to " << range.real_addr + range.size << " over old range";
      return false;
    }

    if (gap_after) {
      if (!MapWithID(range.real_addr + range.size, gap_after, old_range.id,
                     old_range.offset_base + gap_before + range.size, false)) {
        LOG(ERROR) << "Could not map old range from " << std::hex
                   << old_range.real_addr << " to "
                   << old_range.real_addr + gap_before;
        return false;
      }
    }

    return true;
  }

  // Now search for a location for the new range.  It should be in the first
  // free block in quipper space.

  // If there is no existing mapping, add it to the beginning of quipper space.
  if (mappings_.empty()) {
    range.mapped_addr = 0;
    range.unmapped_space_after = kUint64Max - range.size;
    mappings_.push_back(range);
    return true;
  }

  // If there is space before the first mapped range in quipper space, use it.
  if (mappings_.begin()->mapped_addr >= range.size) {
    range.mapped_addr = 0;
    range.unmapped_space_after = mappings_.begin()->mapped_addr - range.size;
    mappings_.push_front(range);
    return true;
  }

  // Otherwise, search through the existing mappings for a free block after one
  // of them.
  for (iter = mappings_.begin(); iter != mappings_.end(); ++iter) {
    if (iter->unmapped_space_after < range.size)
      continue;

    range.mapped_addr = iter->mapped_addr + iter->size;
    range.unmapped_space_after = iter->unmapped_space_after - range.size;
    iter->unmapped_space_after = 0;

    mappings_.insert(++iter, range);
    return true;
  }

  // If it still hasn't succeeded in mapping, it means there is no free space in
  // quipper space large enough for a mapping of this size.
  DumpToLog();
  LOG(ERROR) << "Could not find space to map addr=" << std::hex << real_addr
             << " with size " << std::hex << size;
  return false;
}

void AddressMapper::DumpToLog() const {
  MappingList::const_iterator it;
  for (it = mappings_.begin(); it != mappings_.end(); ++it) {
    LOG(INFO) << " real_addr: " << std::hex << it->real_addr
              << " mapped: " << std::hex << it->mapped_addr
              << " id: " << std::hex << it->id
              << " size: " << std::hex << it->size;
  }
}

bool AddressMapper::GetMappedAddress(const uint64_t real_addr,
                                     uint64_t* mapped_addr) const {
  CHECK(mapped_addr);
  MappingList::const_iterator iter;
  for (iter = mappings_.begin(); iter != mappings_.end(); ++iter) {
    if (!iter->ContainsAddress(real_addr))
      continue;
    *mapped_addr = iter->mapped_addr + real_addr - iter->real_addr;
    return true;
  }
  return false;
}

bool AddressMapper::GetMappedIDAndOffset(const uint64_t real_addr,
                                         uint64_t* id,
                                         uint64_t* offset) const {
  CHECK(id);
  CHECK(offset);
  MappingList::const_iterator iter;
  for (iter = mappings_.begin(); iter != mappings_.end(); ++iter) {
    if (!iter->ContainsAddress(real_addr))
      continue;
    *id = iter->id;
    *offset = real_addr - iter->real_addr + iter->offset_base;
    return true;
  }
  return false;
}

uint64_t AddressMapper::GetMaxMappedLength() const {
  if (IsEmpty())
    return 0;

  uint64_t min = mappings_.begin()->mapped_addr;

  MappingList::const_iterator iter = mappings_.end();
  --iter;
  uint64_t max = iter->mapped_addr + iter->size;

  return max - min;
}

void AddressMapper::Unmap(MappingList::iterator mapping_iter) {
  // Add the freed up space to the free space counter of the previous
  // mapped region, if it exists.
  if (mapping_iter != mappings_.begin()) {
    const MappedRange& range = *mapping_iter;
    MappingList::iterator previous_range_iter = std::prev(mapping_iter);
    previous_range_iter->unmapped_space_after +=
        range.size + range.unmapped_space_after;
  }
  mappings_.erase(mapping_iter);
}

}  // namespace quipper
