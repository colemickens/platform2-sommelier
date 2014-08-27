// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/dbus_object.h>

#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/dbus/exported_property_set.h>
#include <dbus/property.h>

namespace chromeos {
namespace dbus_utils {

//////////////////////////////////////////////////////////////////////////////

DBusInterface::DBusInterface(DBusObject* dbus_object,
                             const std::string& interface_name)
    : dbus_object_(dbus_object),
      interface_name_(interface_name) {}

void DBusInterface::AddProperty(const std::string& property_name,
                                ExportedPropertyBase* prop_base) {
  dbus_object_->property_set_.RegisterProperty(interface_name_,
                                               property_name,
                                               prop_base);
}

void DBusInterface::ExportAsync(
    ExportedObjectManager* object_manager,
    dbus::Bus* bus,
    dbus::ExportedObject* exported_object,
    const dbus::ObjectPath& object_path,
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  for (const auto& pair : handlers_) {
    std::string method_name = pair.first;
    std::string export_error = "Failed exporting " + method_name + " method";
    auto export_handler = sequencer->GetExportHandler(
        interface_name_, method_name, export_error, true);
    auto method_handler = dbus_utils::GetExportableDBusMethod(
        base::Bind(&DBusInterface::HandleMethodCall, base::Unretained(this)));
    exported_object->ExportMethod(
        interface_name_, method_name, method_handler, export_handler);
  }

  std::vector<AsyncEventSequencer::CompletionAction> actions;
  if (object_manager) {
    auto property_writer_callback =
        dbus_object_->property_set_.GetPropertyWriter(interface_name_);
    actions.push_back(sequencer->WrapCompletionTask(
        base::Bind(&ExportedObjectManager::ClaimInterface,
                   object_manager->AsWeakPtr(),
                   object_path,
                   interface_name_,
                   property_writer_callback)));
  }
  actions.push_back(completion_callback);
  sequencer->OnAllTasksCompletedCall(actions);
}

std::unique_ptr<dbus::Response> DBusInterface::HandleMethodCall(
    dbus::MethodCall* method_call) {
  std::string method_name = method_call->GetMember();
  auto pair = handlers_.find(method_name);
  if (pair == handlers_.end()) {
    return CreateDBusErrorResponse(method_call,
                                   DBUS_ERROR_UNKNOWN_METHOD,
                                   "Unknown method: " + method_name);
  }
  LOG(INFO) << "Dispatching DBus message call: " << method_name;
  return pair->second->HandleMethod(method_call);
}

void DBusInterface::AddHandlerImpl(
    const std::string& method_name,
    std::unique_ptr<DBusInterfaceMethodHandler> handler) {
  auto res = handlers_.insert(std::make_pair(method_name, std::move(handler)));
  CHECK(res.second) << "Method '" << method_name << "' already exists";
}

///////////////////////////////////////////////////////////////////////////////

DBusObject::DBusObject(ExportedObjectManager* object_manager,
                       const scoped_refptr<dbus::Bus>& bus,
                       const dbus::ObjectPath& object_path)
    : property_set_(bus),
      bus_(bus),
      object_path_(object_path) {
  if (object_manager)
    object_manager_ = object_manager->AsWeakPtr();
}

DBusObject::~DBusObject() {
  if (object_manager_) {
    for (const auto& pair : interfaces_)
      object_manager_->ReleaseInterface(object_path_, pair.first);
  }

  if (exported_object_)
    exported_object_->Unregister();
}

DBusInterface* DBusObject::AddOrGetInterface(
    const std::string& interface_name) {
  auto iter = interfaces_.find(interface_name);
  if (iter == interfaces_.end()) {
    // Interface doesn't exist yet. Create one...
    std::unique_ptr<DBusInterface> new_itf(
        new DBusInterface(this, interface_name));
    iter = interfaces_.insert(
        std::make_pair(interface_name, std::move(new_itf))).first;
  }
  return iter->second.get();
}

DBusInterfaceMethodHandler* DBusObject::FindMethodHandler(
      const std::string& interface_name, const std::string& method_name) const {
  auto itf_iter = interfaces_.find(interface_name);
  if (itf_iter == interfaces_.end())
    return nullptr;

  auto handler_iter = itf_iter->second->handlers_.find(method_name);
  if (handler_iter == itf_iter->second->handlers_.end())
    return nullptr;
  return handler_iter->second.get();
}

void DBusObject::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  CHECK(exported_object_ == nullptr) << "Object already registered.";
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  exported_object_ = bus_->GetExportedObject(object_path_);

  // Add the org.freedesktop.DBus.Properties interface to the object.
  DBusInterface* prop_interface = AddOrGetInterface(dbus::kPropertiesInterface);
  prop_interface->AddMethodHandler(dbus::kPropertiesGetAll,
                                   base::Unretained(&property_set_),
                                   &ExportedPropertySet::HandleGetAll);
  prop_interface->AddMethodHandler(dbus::kPropertiesGet,
                                   base::Unretained(&property_set_),
                                   &ExportedPropertySet::HandleGet);
  prop_interface->AddMethodHandler(dbus::kPropertiesSet,
                                   base::Unretained(&property_set_),
                                   &ExportedPropertySet::HandleSet);
  property_set_.OnPropertiesInterfaceExported(prop_interface);

  // Export interface methods
  for (const auto& pair : interfaces_) {
    pair.second->ExportAsync(
        object_manager_.get(),
        bus_.get(),
        exported_object_,
        object_path_,
        sequencer->GetHandler("Failed to export interface " + pair.first,
                              false));
  }

  sequencer->OnAllTasksCompletedCall({completion_callback});
}

bool DBusObject::SendSignal(dbus::Signal* signal) {
  if (exported_object_) {
    exported_object_->SendSignal(signal);
    return true;
  }
  LOG(ERROR) << "Trying to send a signal from an object that is not exported";
  return false;
}

}  // namespace dbus_utils
}  // namespace chromeos
