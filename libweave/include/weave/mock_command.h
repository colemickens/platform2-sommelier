// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_MOCK_COMMAND_H_
#define LIBWEAVE_INCLUDE_WEAVE_MOCK_COMMAND_H_

#include "weave/command.h"

#include <memory>
#include <string>

#include <base/values.h>

#include <gmock/gmock.h>

namespace weave {
namespace unittests {

class MockCommand : public Command {
 public:
  ~MockCommand() override = default;

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_CONST_METHOD0(GetID, const std::string&());
  MOCK_CONST_METHOD0(GetName, const std::string&());
  MOCK_CONST_METHOD0(GetCategory, const std::string&());
  MOCK_CONST_METHOD0(GetStatus, CommandStatus());
  MOCK_CONST_METHOD0(GetOrigin, CommandOrigin());
  MOCK_CONST_METHOD0(MockGetParameters, const std::string&());
  MOCK_CONST_METHOD0(MockGetProgress, const std::string&());
  MOCK_CONST_METHOD0(MockGetResults, const std::string&());
  MOCK_METHOD2(SetProgress,
               bool(const base::DictionaryValue&, chromeos::ErrorPtr*));
  MOCK_METHOD2(SetResults,
               bool(const base::DictionaryValue&, chromeos::ErrorPtr*));
  MOCK_METHOD0(Abort, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(Done, void());
  MOCK_CONST_METHOD0(MockToJson, const std::string&());

  std::unique_ptr<base::DictionaryValue> GetParameters() const override;
  std::unique_ptr<base::DictionaryValue> GetProgress() const override;
  std::unique_ptr<base::DictionaryValue> GetResults() const override;
  std::unique_ptr<base::DictionaryValue> ToJson() const override;
};

}  // namespace unittests
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MOCK_COMMAND_H_
