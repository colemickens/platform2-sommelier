// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_DBUS_CONSTANTS_H_
#define PORTIER_DBUS_CONSTANTS_H_

namespace portier {

constexpr char kPortierInterface[] = "org.chromium.Portier";
constexpr char kPortierServicePath[] = "/org/chromium/Portier";
constexpr char kPortierServiceName[] = "org.chromium.Portier";

constexpr char kBindInterfaceMethod[] = "BindInterface";
constexpr char kReleaseInterfaceMethod[] = "ReleaseInterface";

constexpr char kCreateProxyGroupMethod[] = "CreateProxyGroup";
constexpr char kReleaseProxyGroupMethod[] = "ReleaseProxyGroup";

constexpr char kAddToGroupMethod[] = "AddToGroup";
constexpr char kRemoveFromGroupMethod[] = "RemoveFromGroup";

constexpr char kSetUpstreamInterfaceMethod[] = "SetUpstreamInterface";
constexpr char kUnsetUpstreamInterfaceMethod[] = "UnsetUpstreamInterface";

}  // namespace portier

#endif  // PORTIER_DBUS_CONSTANTS_H_
