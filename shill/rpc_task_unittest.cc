// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/rpc_task.h"

#include <gtest/gtest.h>

#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"

using std::map;
using std::string;

namespace shill {

class RPCTaskTest : public testing::Test,
                    public RPCTaskDelegate {
 public:
  RPCTaskTest() : notify_calls_(0), task_(&control_, this) {}

  // Inherited from RPCTaskDelegate.
  virtual void Notify(const string &reason, const map<string, string> &dict);

 protected:
  int notify_calls_;
  string last_notify_reason_;
  map<string, string> last_notify_dict_;
  NiceMockControl control_;
  RPCTask task_;
};

void RPCTaskTest::Notify(const string &reason,
                         const map<string, string> &dict) {

  notify_calls_++;
  last_notify_reason_ = reason;
  last_notify_dict_ = dict;
}

TEST_F(RPCTaskTest, GetRpcIdentifiers) {
  EXPECT_EQ(RPCTaskMockAdaptor::kRpcId, task_.GetRpcIdentifier());
  EXPECT_EQ(RPCTaskMockAdaptor::kRpcInterfaceId,
            task_.GetRpcInterfaceIdentifier());
  EXPECT_EQ(RPCTaskMockAdaptor::kRpcConnId, task_.GetRpcConnectionIdentifier());
}

TEST_F(RPCTaskTest, Notify) {
  static const char kReason[] = "up";
  map<string, string> dict;
  dict["foo"] = "bar";
  task_.Notify(kReason, dict);
  EXPECT_EQ(1, notify_calls_);
  EXPECT_EQ(kReason, last_notify_reason_);
  EXPECT_EQ("bar", last_notify_dict_["foo"]);
}

}  // namespace shill
