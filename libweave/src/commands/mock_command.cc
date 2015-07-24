// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weave/mock_command.h"

#include <memory>
#include <string>

#include <base/values.h>

#include "libweave/src/commands/unittest_utils.h"

namespace weave {

using unittests::CreateDictionaryValue;

std::unique_ptr<base::DictionaryValue> MockCommand::GetParameters() const {
  return CreateDictionaryValue(MockGetParameters());
}

std::unique_ptr<base::DictionaryValue> MockCommand::GetProgress() const {
  return CreateDictionaryValue(MockGetProgress());
}

std::unique_ptr<base::DictionaryValue> MockCommand::GetResults() const {
  return CreateDictionaryValue(MockGetResults());
}

std::unique_ptr<base::DictionaryValue> MockCommand::ToJson() const {
  return CreateDictionaryValue(MockToJson());
}

}  // namespace weave
