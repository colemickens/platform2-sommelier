// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_DBUS_ADAPTOR_H_
#define LOGIN_MANAGER_SESSION_MANAGER_DBUS_ADAPTOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <dbus/exported_object.h>  // For ResponseSender.

namespace dbus {
class MethodCall;
class ObjectProxy;
class Response;
}  // namespace dbus

namespace org {
namespace chromium {
class SessionManagerInterfaceInterface;
}  // namespace chromium
}  // namespace org

namespace login_manager {

// Implements the DBus SessionManagerInterface.
//
// All signatures used in the methods of the ownership API are SHA1 with RSA
// encryption.
class SessionManagerDBusAdaptor {
 public:
  // Does not take |impl|'s ownership.
  explicit SessionManagerDBusAdaptor(
      org::chromium::SessionManagerInterfaceInterface* impl);
  virtual ~SessionManagerDBusAdaptor();

  void ExportDBusMethods(dbus::ExportedObject* object);

  //////////////////////////////////////////////////////////////////////////////
  // Methods exposed via RPC are defined below.
  // Explanatory comments are in session_manager.xml
  std::unique_ptr<dbus::Response> EmitLoginPromptVisible(
      dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> EnableChromeTesting(dbus::MethodCall* call);

  std::unique_ptr<dbus::Response> StartSession(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> StopSession(dbus::MethodCall* call);

  // Asynchronous.
  void StorePolicy(dbus::MethodCall* call,
                   dbus::ExportedObject::ResponseSender sender);
  void StoreUnsignedPolicy(dbus::MethodCall* call,
                           dbus::ExportedObject::ResponseSender sender);
  std::unique_ptr<dbus::Response> RetrievePolicy(dbus::MethodCall* call);

  // Asynchronous.
  void StorePolicyForUser(dbus::MethodCall* call,
                          dbus::ExportedObject::ResponseSender sender);
  void StoreUnsignedPolicyForUser(dbus::MethodCall* call,
                                  dbus::ExportedObject::ResponseSender sender);
  std::unique_ptr<dbus::Response> RetrievePolicyForUser(dbus::MethodCall* call);

  // Asynchronous.
  void StoreDeviceLocalAccountPolicy(
      dbus::MethodCall* call, dbus::ExportedObject::ResponseSender sender);
  std::unique_ptr<dbus::Response> RetrieveDeviceLocalAccountPolicy(
      dbus::MethodCall* call);

  std::unique_ptr<dbus::Response> RetrieveSessionState(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> RetrieveActiveSessions(
      dbus::MethodCall* call);

  std::unique_ptr<dbus::Response> HandleSupervisedUserCreationStarting(
      dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> HandleSupervisedUserCreationFinished(
      dbus::MethodCall* call);

  std::unique_ptr<dbus::Response> LockScreen(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> HandleLockScreenShown(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> HandleLockScreenDismissed(
      dbus::MethodCall* call);

  std::unique_ptr<dbus::Response> RestartJob(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> StartDeviceWipe(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> SetFlagsForUser(dbus::MethodCall* call);

  // Asynchronous.
  void GetServerBackedStateKeys(dbus::MethodCall* call,
                                dbus::ExportedObject::ResponseSender sender);
  std::unique_ptr<dbus::Response> InitMachineInfo(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> StartContainer(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> StopContainer(dbus::MethodCall* call);

  std::unique_ptr<dbus::Response> StartArcInstance(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> StopArcInstance(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> SetArcCpuRestriction(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> EmitArcBooted(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> GetArcStartTimeTicks(dbus::MethodCall* call);
  std::unique_ptr<dbus::Response> RemoveArcData(dbus::MethodCall* call);

 private:
  // Pointer to a member function for handling a DBus method call.
  typedef std::unique_ptr<dbus::Response> (
      SessionManagerDBusAdaptor::*SyncDBusMethodCallMemberFunction)(
          dbus::MethodCall*);
  // Pointer to a member function for handling an async DBus method call.
  typedef void (SessionManagerDBusAdaptor::*AsyncDBusMethodCallMemberFunction)(
      dbus::MethodCall*,
      dbus::ExportedObject::ResponseSender);

  void ExportSyncDBusMethod(dbus::ExportedObject* object,
                            const std::string& method_name,
                            SyncDBusMethodCallMemberFunction member);
  void ExportAsyncDBusMethod(dbus::ExportedObject* object,
                             const std::string& method_name,
                             AsyncDBusMethodCallMemberFunction member);

  org::chromium::SessionManagerInterfaceInterface* const impl_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(SessionManagerDBusAdaptor);
};

}  // namespace login_manager
#endif  // LOGIN_MANAGER_SESSION_MANAGER_DBUS_ADAPTOR_H_
