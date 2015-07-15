// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_
#define LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_

#include <base/values.h>
#include <chromeos/errors/error.h>

namespace weave {

class Command {
 public:
  // Returns JSON representation of the command.
  virtual std::unique_ptr<base::DictionaryValue> ToJson() const = 0;

 protected:
  virtual ~Command() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_
