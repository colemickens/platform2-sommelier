// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_COMMON_UTIL_H_
#define PSYCHE_COMMON_UTIL_H_

#include <memory>

namespace protobinder {
class BinderProxy;
class IBinder;
class StrongBinder;
}  // namespace protobinder

namespace psyche {
namespace util {

// TODO(derat): Move these methods to libprotobinder once the corresponding
// proto file lives there.

// Extracts a remote binder stored within |proto|. The field is cleared to make
// sure the proxy won't be accidentally extracted twice and double-freed.
std::unique_ptr<protobinder::BinderProxy> ExtractBinderProxyFromProto(
    protobinder::StrongBinder* proto);

// Copies |binder| into |proto|.
void CopyBinderToProto(const protobinder::IBinder& binder,
                       protobinder::StrongBinder* proto);

}  // namespace util
}  // namespace psyche

#endif  // PSYCHE_COMMON_UTIL_H_
