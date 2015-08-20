// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_TEST_MOCK_MDNS_H_
#define LIBWEAVE_INCLUDE_WEAVE_TEST_MOCK_MDNS_H_

#include <weave/mdns.h>

#include <map>
#include <string>

#include <gmock/gmock.h>

namespace weave {
namespace test {

class MockMdns : public Mdns {
 public:
  MOCK_METHOD3(PublishService,
               void(const std::string&,
                    uint16_t,
                    const std::map<std::string, std::string>&));
  MOCK_METHOD1(StopPublishing, void(const std::string&));
  MOCK_CONST_METHOD0(GetId, std::string());
};

}  // namespace test
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_TEST_MOCK_MDNS_H_
