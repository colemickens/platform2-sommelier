// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_manager.h"

namespace buffet {

const CommandDictionary& CommandManager::GetCommandDictionary() const {
  return dictionary_;
}

}  // namespace buffet
