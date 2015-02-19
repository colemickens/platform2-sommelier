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

// Persistent keys
extern const char kClientId[];
extern const char kClientSecret[];
extern const char kApiKey[];
extern const char kRefreshToken[];
extern const char kDeviceId[];
extern const char kOAuthURL[];
extern const char kServiceURL[];
extern const char kRobotAccount[];
// Transient keys
extern const char kDeviceKind[];
extern const char kName[];
extern const char kDisplayName[];
extern const char kDescription[];
extern const char kLocation[];
extern const char kModelId[];

}  // namespace storage_keys
}  // namespace buffet

#endif  // BUFFET_DEVICE_REGISTRATION_STORAGE_KEYS_H_
