// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <google/protobuf/compiler/plugin.h>

#include "bidl/bidl_code_generator.h"

int main(int argc, char** argv) {
  BidlCodeGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
