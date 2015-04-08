// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DEVICE_REGISTRATION_STORAGE_KEYS_H_
#define BUFFET_DEVICE_REGISTRATION_STORAGE_KEYS_H_

// These are the keys used to identify specific device registration information
// being saved to a storage. Used mostly internally by DeviceRegistrationInfo
// but also exposed so that tests can access them.
namespace buffet {
namespace storage_keys {

extern const char kRefreshToken[];
extern const char kDeviceId[];
extern const char kRobotAccount[];
extern const char kName[];
extern const char kDescription[];
extern const char kLocation[];

}  // namespace storage_keys
}  // namespace buffet

#endif  // BUFFET_DEVICE_REGISTRATION_STORAGE_KEYS_H_
