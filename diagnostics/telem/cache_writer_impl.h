// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_CACHE_WRITER_IMPL_H_
#define DIAGNOSTICS_TELEM_CACHE_WRITER_IMPL_H_

#include <unordered_map>

#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

// CacheWriterImpl is used only for testing purposes.
class CacheWriterImpl : public CacheWriter {
 public:
  void SetParsedData(TelemetryItemEnum item,
                     base::Optional<base::Value> data) override;

  // Tests whether |item| is in the cache. If |item| is not found inside the
  // cache, or if value corresponding to |item| is not equal to |data|, this
  // check will fail.
  void CheckParsedDataFor(TelemetryItemEnum item, base::Value data);

  // Tests whether |item| is not in the cache. If |item| is in the cache,
  // confirm that the corresponding optional value is null. If the optional
  // value is not null, this test will fail.
  void CheckParsedDataIsNull(TelemetryItemEnum item);

 private:
  std::unordered_map<TelemetryItemEnum, base::Optional<base::Value>> cache_;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_CACHE_WRITER_IMPL_H_
