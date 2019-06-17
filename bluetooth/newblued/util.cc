// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/util.h"

#include <utility>

#include "bluetooth/common/util.h"

namespace bluetooth {

std::unique_ptr<GattService> ConvertToGattService(
    const struct GattTraversedService& service) {
  // struct GattTraversedService is the result of primary service traversal, so
  // it is safe to assume that primary is always true in this case.
  auto s = std::make_unique<GattService>(service.firstHandle,
                                         service.lastHandle, true /* primary */,
                                         ConvertToUuid(service.uuid));

  const auto* included_service = service.inclSvcs;
  for (int i = 0; i < service.numInclSvcs; ++i, ++included_service) {
    auto is = std::make_unique<GattIncludedService>(
        s.get(), included_service->includeDefHandle,
        included_service->firstHandle, included_service->lastHandle,
        ConvertToUuid(included_service->uuid));
    s->AddIncludedService(std::move(is));
  }

  const auto* characteristic = service.chars;
  for (int i = 0; i < service.numChars; ++i, ++characteristic) {
    auto c = std::make_unique<GattCharacteristic>(
        s.get(), characteristic->valHandle, characteristic->firstHandle,
        characteristic->lastHandle, characteristic->charProps,
        ConvertToUuid(characteristic->uuid));

    const auto* descriptor = characteristic->descrs;
    for (int j = 0; j < characteristic->numDescrs; ++j, ++descriptor) {
      auto d = std::make_unique<GattDescriptor>(
          c.get(), descriptor->handle, ConvertToUuid(descriptor->uuid));
      c->AddDescriptor(std::move(d));
    }
    s->AddCharacteristic(std::move(c));
  }

  return s;
}

}  // namespace bluetooth
