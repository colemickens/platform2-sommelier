// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: These tests are not generated. They test generated code.

#include <gtest/gtest.h>

#include "trunks/tpm_generated.h"

namespace trunks {

// This test is designed to get good coverage of the different types generated
// code for Serializing and Parsing structures / unions / typedefs.
TEST(GeneratorTest, SerializeParseStruct) {
  TPM2B_CREATION_DATA data;
  memset(&data, 0, sizeof(TPM2B_CREATION_DATA));
  data.creation_data.pcr_select.count = 1;
  data.creation_data.pcr_select.pcr_selections[0].hash = TPM_ALG_SHA256;
  data.creation_data.pcr_select.pcr_selections[0].sizeof_select = 1;
  data.creation_data.pcr_select.pcr_selections[0].pcr_select[0] = 0;
  data.creation_data.pcr_digest.size = 2;
  data.creation_data.locality = 0;
  data.creation_data.parent_name_alg = TPM_ALG_SHA256;
  data.creation_data.parent_name.size = 3;
  data.creation_data.parent_qualified_name.size = 4;
  data.creation_data.outside_info.size = 5;
  std::string buffer;
  TPM_RC rc = Serialize_TPM2B_CREATION_DATA(data, &buffer);
  ASSERT_EQ(TPM_RC_SUCCESS, rc);
  EXPECT_EQ(35, buffer.size());
  TPM2B_CREATION_DATA data2;
  memset(&data2, 0, sizeof(TPM2B_CREATION_DATA));
  std::string buffer_before = buffer;
  std::string buffer_parsed;
  rc = Parse_TPM2B_CREATION_DATA(&buffer, &data2, &buffer_parsed);
  ASSERT_EQ(TPM_RC_SUCCESS, rc);
  EXPECT_EQ(0, buffer.size());
  EXPECT_EQ(buffer_before, buffer_parsed);
  EXPECT_EQ(0, memcmp(&data, &data2, sizeof(TPM2B_CREATION_DATA)));
}

}  // namespace trunks
