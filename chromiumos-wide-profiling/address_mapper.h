// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ADDRESS_MAPPER_
#define ADDRESS_MAPPER_

#include <list>
#include <string>

#include "base/basictypes.h"

class AddressMapper {
 public:
  AddressMapper() {}

  // Maps a new address range to quipper space.
  // |remove_existing_mappings| indicates whether to remove old mappings that
  // collide with the new range in real address space, indicating it has been
  // unmapped.
  // Returns true if mapping was successful.
  bool Map(const uint64 real_addr, const uint64 length,
           bool remove_existing_mappings);

  // Looks up |real_addr| and returns the mapped address.
  bool GetMappedAddress(const uint64 real_addr, uint64* mapped_addr) const;

  // Returns true if there are no mappings.
  bool IsEmpty() const {
    return mappings_.empty();
  }

  // Returns the number of address ranges that are currently mapped.
  unsigned int GetNumMappedRanges() const {
    return mappings_.size();
  }

 private:
  struct MappedRange {
    uint64 real_addr;
    uint64 mapped_addr;
    uint64 size;

    // Length of unmapped space after this range.
    uint64 unmapped_space_after;

    // Determines if this range intersects another range in real space.
    inline bool Intersects(const MappedRange& range) const {
      return (real_addr <= range.real_addr + range.size - 1) &&
             (real_addr + size - 1 >= range.real_addr);
    }

    // Determines if this range fully covers another range in real space.
    inline bool Covers(const MappedRange& range) const {
      return (real_addr <= range.real_addr) &&
             (real_addr + size - 1 >= range.real_addr + range.size - 1);
    }

    // Determines if this range fully contains another range in real space.
    // This is different from Covers() in that the boundaries cannot overlap.
    inline bool Contains(const MappedRange& range) const {
      return (real_addr < range.real_addr) &&
             (real_addr + size - 1 > range.real_addr + range.size - 1);
    }
  };

  // TODO(sque): implement with set or map to improve searching.
  typedef std::list<MappedRange> MappingList;

  // Removes an existing address mapping.
  // Returns true if successful, false if no mapped address range was found.
  bool Unmap(const MappedRange& range);

  // Container for all the existing mappings.
  MappingList mappings_;

  DISALLOW_COPY_AND_ASSIGN(AddressMapper);
};

#endif  // ADDRESS_MAPPER_
