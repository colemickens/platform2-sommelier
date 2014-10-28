// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_DBUS_CONSTANTS_H_
#define PEERD_DBUS_CONSTANTS_H_

namespace peerd {

namespace dbus_constants {

// The service name claimed by peerd.
extern const char kServiceName[];

// Path of the ObjectManager.
extern const char kRootServicePath[];

// Interface implemented by the object at kManagerServicePath.
extern const char kManagerInterface[];
extern const char kManagerServicePath[];

// Methods exposed as part of kManagerInterface.
extern const char kManagerExposeService[];
extern const char kManagerPing[];
extern const char kManagerRemoveExposedService[];
extern const char kManagerStartMonitoring[];
extern const char kManagerStopMonitoring[];

// Interface implemented by the objects at kSelfPath and under kPeerPrefix.
extern const char kPeerInterface[];
extern const char kSelfPath[];
extern const char kPeerPrefix[];

// Properties exposed as part of kPeerInterface.
extern const char kPeerLastSeen[];
extern const char kPeerUUID[];

// Interface implemented by the service objects.
extern const char kServiceInterface[];
extern const char kServicePathFragment[];

// Properties exposed as part of kServiceInterface.
extern const char kServiceId[];
extern const char kServiceInfo[];
extern const char kServiceIpInfos[];

extern const char kPingResponse[];

namespace avahi {

extern const char kServiceName[];

extern const char kServerInterface[];
extern const char kServerPath[];
extern const char kServerMethodEntryGroupNew[];
extern const char kServerMethodServiceBrowserNew[];
extern const char kServerMethodServiceResolverNew[];
extern const char kServerMethodGetHostName[];
extern const char kServerMethodGetState[];
extern const char kServerSignalStateChanged[];

extern const char kGroupInterface[];
extern const char kGroupMethodAddRecord[];
extern const char kGroupMethodAddService[];
extern const char kGroupMethodCommit[];
extern const char kGroupMethodFree[];
extern const char kGroupMethodReset[];

extern const char kServiceBrowserInterface[];
extern const char kServiceBrowserMethodFree[];
extern const char kServiceBrowserSignalItemNew[];
extern const char kServiceBrowserSignalItemRemove[];
extern const char kServiceBrowserSignalFailure[];

extern const char kServiceResolverInterface[];
extern const char kServiceResolverMethodFree[];
extern const char kServiceResolverSignalFound[];
extern const char kServiceResolverSignalFailure[];

}  // namespace avahi

}  // namespace dbus_constants

}  // namespace peerd

#endif  // PEERD_DBUS_CONSTANTS_H_
