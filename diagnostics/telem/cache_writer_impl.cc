// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/telem/cache_writer_impl.h"

#include <base/optional.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

void CacheWriterImpl::SetParsedData(TelemetryItemEnum item,
                                    base::Optional<base::Value> data) {
  cache_[item] = data;
}

void CacheWriterImpl::CheckParsedDataFor(TelemetryItemEnum item,
                                         base::Value data) {
  auto search = cache_.find(item);
  CHECK(search != cache_.end());
  ASSERT_EQ(search->second, data);
}

void CacheWriterImpl::CheckParsedDataIsNull(TelemetryItemEnum item) {
  auto search = cache_.find(item);
  if (search != cache_.end()) {
    CHECK(!cache_.at(item).has_value());
  }
}

}  // namespace diagnostics
