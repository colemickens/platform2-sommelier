// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <base/test/fuzzed_data_provider.h>

#include "shill/cellular/cellular_pco.h"

namespace shill {
namespace {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);

  // Prepare a few random elements to search for.
  std::vector<uint16_t> elements(data_provider.ConsumeUint32InRange(0, 10));
  for (size_t i = 0; i < elements.size(); i++)
    elements[i] = data_provider.ConsumeUint16();

  const std::string& str = data_provider.ConsumeRemainingBytes();
  const std::vector<uint8_t> raw_data(str.begin(), str.end());
  std::unique_ptr<CellularPco> pco = CellularPco::CreateFromRawData(raw_data);

  if (pco)
    for (auto e : elements)
      pco->FindElement(e);

  return 0;
}

}  // namespace
}  // namespace shill
