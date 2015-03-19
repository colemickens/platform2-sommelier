// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_definition.h"

namespace buffet {

CommandDefinition::CommandDefinition(
    const std::string& category,
    std::unique_ptr<const ObjectSchema> parameters,
    std::unique_ptr<const ObjectSchema> results)
        : category_{category},
          parameters_{std::move(parameters)},
          results_{std::move(results)} {}

}  // namespace buffet
