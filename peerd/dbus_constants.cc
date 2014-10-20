// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/dbus_constants.h"

namespace peerd {

namespace dbus_constants {

const char kServiceName[] = "org.chromium.peerd";

const char kRootServicePath[] = "/org/chromium/peerd";

const char kManagerInterface[] = "org.chromium.peerd.Manager";
const char kManagerServicePath[] = "/org/chromium/peerd/Manager";

const char kManagerExposeService[] = "ExposeService";
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
const char kServicePathFragment[] = "/services/";

const char kServiceId[] = "ServiceId";
const char kServiceInfo[] = "ServiceInfo";
const char kServiceIpInfos[] = "IpInfos";

const char kPingResponse[] = "Hello world!";

namespace avahi {

const char kServiceName[] = "org.freedesktop.Avahi";

const char kServerInterface[] = "org.freedesktop.Avahi.Server";
const char kServerPath[] = "/";
const char kServerMethodEntryGroupNew[] = "EntryGroupNew";
const char kServerMethodServiceBrowserNew[] = "ServiceBrowserNew";
const char kServerMethodServiceResolverNew[] = "ServiceResolverNew";
const char kServerMethodGetHostName[] = "GetHostName";
const char kServerMethodGetState[] = "GetState";
const char kServerSignalStateChanged[] = "StateChanged";

const char kGroupInterface[] = "org.freedesktop.Avahi.EntryGroup";
const char kGroupMethodAddRecord[] = "AddRecord";
const char kGroupMethodAddService[] = "AddService";
const char kGroupMethodCommit[] = "Commit";
const char kGroupMethodFree[] = "Free";
const char kGroupMethodReset[]= "Reset";

const char kServiceBrowserInterface[] = "org.freedesktop.Avahi.ServiceBrowser";
const char kServiceBrowserMethodFree[] = "Free";
const char kServiceBrowserSignalItemNew[] = "ItemNew";
const char kServiceBrowserSignalItemRemove[] = "ItemRemove";
const char kServiceBrowserSignalFailure[] = "Failure";

const char kServiceResolverInterface[] =
    "org.freedesktop.Avahi.ServiceResolver";
const char kServiceResolverMethodFree[] = "Free";
const char kServiceResolverSignalFound[] = "Found";
const char kServiceResolverSignalFailure[] = "Failure";

}  // namespace avahi

}  // namespace dbus_constants

}  // namespace peerd
