// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/dbus_constants.h"

namespace peerd {

namespace dbus_constants {

const char kServiceName[] = "org.chromium.peerd";

const char kManagerInterface[] = "org.chromium.peerd.Manager";
const char kManagerServicePath[] = "/org/chromium/peerd/Manager";

const char kManagerExposeIpService[] = "ExposeIpService";
const char kManagerPing[] = "Ping";
const char kManagerRemoveExposedService[] = "RemoveExposedService";
const char kManagerSetFriendlyName[] = "SetFriendlyName";
const char kManagerSetNote[] = "SetNote";
const char kManagerStartMonitoring[] = "StartMonitoring";
const char kManagerStopMonitoring[] = "StopMonitoring";

const char kPeerInterface[] = "org.chromium.peerd.Peer";
const char kSelfPath[] = "/org/chromium/peerd/Self";
const char kPeerPrefix[] = "/org/chromium/peerd/peers/";

const char kPeerFriendlyName[] = "FriendlyName";
const char kPeerLastSeen[] = "LastSeen";
const char kPeerNote[]  = "Note";
const char kPeerUUID[] = "UUID";

const char kServiceInterface[] = "org.chromium.peerd.Service";
const char kServicePathFragment[] = "services/";

const char kServiceId[] = "ServiceId";
const char kServiceInfo[] = "ServiceInfo";
const char kServiceIpInfos[] = "IpInfos";

const char kPingResponse[] = "Hello world!";

}  // namespace dbus_constants

}  // namespace peerd
