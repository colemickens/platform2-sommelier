// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_COMMON_CONSTANTS_H_
#define VM_TOOLS_COMMON_CONSTANTS_H_

namespace vm_tools {

constexpr int kMaitredPort = 8888;
constexpr int kGarconPort = 8889;
constexpr int kTremplinPort = 8890;
constexpr int kVshPort = 9001;

constexpr int kDefaultStartupListenerPort = 7777;
constexpr int kTremplinListenerPort = 7778;

// All ports above this value are reserved for seneschal servers.
constexpr uint32_t kFirstSeneschalServerPort = 16384;

}  // namespace vm_tools

#endif  // VM_TOOLS_COMMON_CONSTANTS_H_
