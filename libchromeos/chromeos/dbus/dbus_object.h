// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DBusObject is a special helper class that simplifies the implementation of
// D-Bus objects in C++. It provides an easy way to define interfaces with
// methods and properties and offloads a lot of work to register the object and
// all of its interfaces, to marshal method calls (by converting D-Bus method
// parameters to native C++ types and invoking native method handlers), etc.

// The basic usage pattern of this class is as follows:
/*
class MyDbusObject {
 public:
  MyDbusObject(const ExportedObjectManager_WeakPtr& object_manager)
      : dbus_object_(object_manager, dbus::ObjectPath("/org/chromium/my_obj")) {
  }

  void Init(const AsyncEventSequencer::CompletionAction& callback) {
    DBusInterface* my_interface =
        dbus_object_.AddOrGetInterface("org.chromium.MyInterface");
    my_interface->AddMethodHandler("Method1", this, &MyDbusObject::Method1);
    my_interface->AddMethodHandler("Method2", this, &MyDbusObject::Method2);
    my_interface->AddProperty("Property1", &prop1_);
    my_interface->AddProperty("Property2", &prop2_);
    prop1_.SetValue("prop1_value");
    prop2_.SetValue(50);
    // Register the object by exporting its methods and properties and
    // exposing them to D-Bus clients.
    dbus_object_.RegisterAsync(callback);
  }

 private:
  DBusObject dbus_object_;

  // Make sure the properties outlive the DBusObject they are registered with.
  chromeos::dbus_utils::ExportedProperty<std::string> prop1_;
  chromeos::dbus_utils::ExportedProperty<int> prop2_;
  int Method1(chromeos::ErrorPtr* error);
  void Method2(chromeos::ErrorPtr* error, const std::string& message);

  DISALLOW_COPY_AND_ASSIGN(MyDbusObject);
};
*/

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/chromeos_export.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/dbus_object_internal_impl.h>
#include <chromeos/dbus/dbus_signal.h>
#include <chromeos/dbus/exported_property_set.h>
#include <chromeos/errors/error.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_path.h>

namespace chromeos {
namespace dbus_utils {

class ExportedObjectManager;
class ExportedPropertyBase;

// This is an abstract base class to allow dispatching a native C++ callback
// method when a corresponding D-Bus method is called.
class DBusInterfaceMethodHandler {
 public:
  virtual ~DBusInterfaceMethodHandler() = default;

  virtual std::unique_ptr<dbus::Response> HandleMethod(
      dbus::MethodCall* method_call) = 0;
};

// A template overload of DBusInterfaceMethodHandler that is specialized
// for particular method handler type signature. The handler is expected
// to take an arbitrary number of arguments of type |Args...| and return
// a value of type |R| (which could be 'void' as well).
template<typename R, typename... Args>
class TypedDBusInterfaceMethodHandler : public DBusInterfaceMethodHandler {
 public:
  // A constructor that takes a |handler| to be called when HandleMethod()
  // virtual function is invoked.
  TypedDBusInterfaceMethodHandler(
      const base::Callback<R(chromeos::ErrorPtr*, Args...)>& handler)
      : handler_(handler) {
  }

  // This method forwards the call to |handler_| and extracts the required
  // arguments from the D-Bus message buffer specified in |method_call|.
  // The return value of |handler_| (if any) is sent back via the returned
  // dbus::Response object, which could also include error details if the
  // handler call has failed.
  std::unique_ptr<dbus::Response> HandleMethod(
      dbus::MethodCall* method_call) override {
    dbus::MessageReader reader(method_call);
    using Handler = TypedReturnDBusMethodHandler<R, Args...>;
    return TypedReturnDBusInvoker<R, Handler, Args...>::Invoke(handler_,
                                                               method_call,
                                                               &reader);
  }

 private:
  // C++ callback to be called when a D-Bus method is dispatched.
  base::Callback<R(chromeos::ErrorPtr*, Args...)> handler_;

  DISALLOW_COPY_AND_ASSIGN(TypedDBusInterfaceMethodHandler);
};

// A specialization of TypedDBusInterfaceMethodHandler for returning
// dbus::Response object instead of arbitrary value. This specialization is
// used when C++ callback expects parsed input parameters but its return
// value is custom and it's up to the callback's implementer to return a valid
// D-Bus response object. Also note that the callback does not take ErrorPtr*
// as a first parameter, since the error information should be returned through
// the D-Bus error response object.
template<typename... Args>
class TypedDBusInterfaceMethodHandler<std::unique_ptr<dbus::Response>, Args...>
    : public DBusInterfaceMethodHandler {
 public:
  // A constructor that takes a |handler| to be called when HandleMethod()
  // virtual function is invoked.
  TypedDBusInterfaceMethodHandler(
      const base::Callback<std::unique_ptr<dbus::Response>(
          dbus::MethodCall* method_call, Args...)>& handler)
      : handler_(handler) {
  }

  // This method forwards the call to |handler_| and extracts the required
  // arguments from the D-Bus message buffer specified in |method_call|.
  // The dbus::Response return value of |handler_| is passed on to the caller.
  std::unique_ptr<dbus::Response> HandleMethod(
      dbus::MethodCall* method_call) override {
    dbus::MessageReader reader(method_call);
    using Handler = RawReturnDBusMethodHandler<Args...>;
    return RawReturnDBusInvoker<Handler, Args...>::Invoke(handler_,
                                                          method_call,
                                                          &reader);
  }

 private:
  // C++ callback to be called when a D-Bus method is dispatched.
  base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall* method_call,
                                                 Args...)> handler_;

  DISALLOW_COPY_AND_ASSIGN(TypedDBusInterfaceMethodHandler);
};

// An implementation of DBusInterfaceMethodHandler that has custom processing
// of both input and output parameters. This class is used by
// DBusObject::AddRawMethodHandler and expects the callback to be of the
// following signature:
//    std::unique_ptr<dbus::Response>(dbus::MethodCall* method_call)
// It will be up to the callback to parse the input parameters from the
// message buffer and construct the D-Bus Response object.
class RawDBusInterfaceMethodHandler : public DBusInterfaceMethodHandler {
 public:
  // A constructor that takes a |handler| to be called when HandleMethod()
  // virtual function is invoked.
  RawDBusInterfaceMethodHandler(
      const base::Callback<std::unique_ptr<dbus::Response>(
          dbus::MethodCall* method_call)>& handler)
      : handler_(handler) {
  }

  std::unique_ptr<dbus::Response> HandleMethod(
      dbus::MethodCall* method_call) override {
    return handler_.Run(method_call);
  }

 private:
  // C++ callback to be called when a D-Bus method is dispatched.
  base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall* method_call)>
      handler_;

  DISALLOW_COPY_AND_ASSIGN(RawDBusInterfaceMethodHandler);
};

class DBusObject;  // forward-declaration.

// This is an implementation proxy class for a D-Bus interface of an object.
// The important functionality for the users is the ability to add D-Bus method
// handlers and define D-Bus object properties. This is achieved by using one
// of the overload of AddMethodHandler() and AddProperty() respectively.
// There are six overloads for DBusInterface::AddMethodHandler():
//  1. That takes a handler as base::Callback<R(ErrorPtr*, Args...)>
//  2. That takes a static function of signature R(ErrorPtr*, Args...)
//  3. That takes a class instance pointer and a class member function of
//     signature R(ErrorPtr*, Args...).
//  4-6. Same as 1-3 above but expect the callback to provide a custom response:
//     std::unique_ptr<dbus::Response>(dbus::MethodCall*, Args...).
// There is also an AddRawMethodHandler() call that lets provide a custom
// handler that can parse its own input parameter and construct a custom
// response.
class CHROMEOS_EXPORT DBusInterface final {
 public:
  DBusInterface(DBusObject* dbus_object, const std::string& interface_name);

  // Register a DBus method handler for |method_name| as base::Callback.
  template<typename R, typename... Args>
  inline void AddMethodHandler(
      const std::string& method_name,
      const base::Callback<R(chromeos::ErrorPtr*, Args...)>& handler) {
    std::unique_ptr<DBusInterfaceMethodHandler> typed_method_handler(
        new TypedDBusInterfaceMethodHandler<R, Args...>(handler));
    AddHandlerImpl(method_name, std::move(typed_method_handler));
  }

  // Register a D-Bus method handler for |method_name| as static function.
  template<typename R, typename... Args>
  inline void AddMethodHandler(
      const std::string& method_name,
      R(*handler)(chromeos::ErrorPtr*, Args...)) {
    std::unique_ptr<DBusInterfaceMethodHandler> typed_method_handler(
        new TypedDBusInterfaceMethodHandler<R, Args...>(base::Bind(handler)));
    AddHandlerImpl(method_name, std::move(typed_method_handler));
  }

  // Register a D-Bus method handler for |method_name| as class member function.
  template<typename R, typename Instance, typename Class, typename... Args>
  inline void AddMethodHandler(
      const std::string& method_name,
      Instance instance,
      R(Class::*handler)(chromeos::ErrorPtr*, Args...)) {
    auto callback = base::Bind(handler, instance);
    std::unique_ptr<DBusInterfaceMethodHandler> typed_method_handler(
        new TypedDBusInterfaceMethodHandler<R, Args...>(callback));
    AddHandlerImpl(method_name, std::move(typed_method_handler));
  }

  // Register a D-Bus method handler for |method_name| as base::Callback.
  template<typename... Args>
  inline void AddMethodHandler(
      const std::string& method_name,
      const base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*,
                                                           Args...)>& handler) {
    std::unique_ptr<DBusInterfaceMethodHandler> typed_method_handler(
        new TypedDBusInterfaceMethodHandler<std::unique_ptr<dbus::Response>,
                                            Args...>(handler));
    AddHandlerImpl(method_name, std::move(typed_method_handler));
  }

  // Register a D-Bus method handler for |method_name| as static function.
  template<typename... Args>
  inline void AddMethodHandler(
      const std::string& method_name,
      std::unique_ptr<dbus::Response>(*handler)(dbus::MethodCall*, Args...)) {
    std::unique_ptr<DBusInterfaceMethodHandler> typed_method_handler(
        new TypedDBusInterfaceMethodHandler<std::unique_ptr<dbus::Response>,
                                            Args...>(base::Bind(handler)));
    AddHandlerImpl(method_name, std::move(typed_method_handler));
  }

  // Register a D-Bus method handler for |method_name| as class member function.
  template<typename Instance, typename Class, typename... Args>
  inline void AddMethodHandler(
      const std::string& method_name,
      Instance instance,
      std::unique_ptr<dbus::Response>(Class::*handler)(dbus::MethodCall*,
                                                       Args...)) {
    auto callback = base::Bind(handler, instance);
    std::unique_ptr<DBusInterfaceMethodHandler> typed_method_handler(
        new TypedDBusInterfaceMethodHandler<std::unique_ptr<dbus::Response>,
                                            Args...>(callback));
    AddHandlerImpl(method_name, std::move(typed_method_handler));
  }

  // Register a raw D-Bus method handler for |method_name| as base::Callback.
  inline void AddRawMethodHandler(
      const std::string& method_name,
      const base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)>&
          handler) {
    std::unique_ptr<DBusInterfaceMethodHandler> raw_method_handler(
        new RawDBusInterfaceMethodHandler(handler));
    AddHandlerImpl(method_name, std::move(raw_method_handler));
  }

  // Register a D-Bus property.
  void AddProperty(const std::string& property_name,
                   ExportedPropertyBase* prop_base);

  // Registers a D-Bus signal that has a specified number and types (|Args|) of
  // arguments. Returns a weak pointer to the DBusSignal object which can be
  // used to send the signal on this interface when needed:
  /*
    DBusInterface* itf = dbus_object->AddOrGetInterface("Interface");
    auto signal = itf->RegisterSignal<int, bool>("MySignal");
    ...
    // Send the Interface.MySig(12, true) signal.
    if (signal.lock()->Send(12, true)) { ... }
  */
  // Or if the signal signature is long or complex, you can alias the
  // DBusSignal<Args...> signal type and use RegisterSignalOfType method
  // instead:
  /*
    DBusInterface* itf = dbus_object->AddOrGetInterface("Interface");
    using MySignal = DBusSignal<int, bool>;
    auto signal = itf->RegisterSignalOfType<MySignal>("MySignal");
    ...
    // Send the Interface.MySig(12, true) signal.
    if (signal.lock()->Send(12, true)) { ... }
  */
  // If the signal with the given name was already registered, the existing
  // copy of the signal proxy object is returned as long as the method signature
  // of the original signal matches the current call. If it doesn't, the method
  // aborts.

  // RegisterSignalOfType can be used to create a signal if the type of the
  // complete DBusSignal<Args...> class which is pre-defined/aliased earlier.
  template<typename DBusSignalType>
  inline std::weak_ptr<DBusSignalType> RegisterSignalOfType(
      const std::string& signal_name) {
    auto signal = std::make_shared<DBusSignalType>(dbus_object_,
                                                   interface_name_,
                                                   signal_name);
    CHECK(signals_.insert(std::make_pair(signal_name, signal)).second)
        << "The signal '" << signal_name << "' is already registered";
    return signal;
  }

  // For simple signal arguments, you can specify their types directly in
  // RegisterSignal<t1, t2, ...>():
  //  auto signal = itf->RegisterSignal<int>("SignalName");
  // This will create a callback signal object that expects one int argument.
  template<typename... Args>
  inline std::weak_ptr<DBusSignal<Args...>> RegisterSignal(
      const std::string& signal_name) {
    return RegisterSignalOfType<DBusSignal<Args...>>(signal_name);
  }

 private:
  // A generic D-Bus method handler for the interface. It extracts the method
  // name from |method_call|, looks up a registered handler from |handlers_|
  // map and dispatched the call to that handler.
  CHROMEOS_PRIVATE std::unique_ptr<dbus::Response> HandleMethodCall(
      dbus::MethodCall* method_call);
  // Helper to add a handler for method |method_name| to the |handlers_| map.
  // Not marked CHROMEOS_PRIVATE because it needs to be called by the inline
  // template functions AddMethodHandler(...)
  void AddHandlerImpl(const std::string& method_name,
                      std::unique_ptr<DBusInterfaceMethodHandler> handler);
  // Exports all the methods and properties of this interface and claims the
  // D-Bus interface.
  // object_manager - ExportedObjectManager instance that notifies D-Bus
  //                  listeners of a new interface being claimed.
  // exported_object - instance of D-Bus object the interface is being added to.
  // object_path - D-Bus object path for the object instance.
  // interface_name - name of interface being registered.
  // completion_callback - a callback to be called when the asynchronous
  //                       registration operation is completed.
  CHROMEOS_PRIVATE void ExportAsync(
      ExportedObjectManager* object_manager,
      dbus::Bus* bus,
      dbus::ExportedObject* exported_object,
      const dbus::ObjectPath& object_path,
      const AsyncEventSequencer::CompletionAction& completion_callback);

  // Method registration map.
  std::map<std::string, std::unique_ptr<DBusInterfaceMethodHandler>> handlers_;
  // Signal registration map.
  std::map<std::string, std::shared_ptr<DBusSignalBase>> signals_;

  friend class DBusObject;
  DBusObject* dbus_object_;
  std::string interface_name_;

  DISALLOW_COPY_AND_ASSIGN(DBusInterface);
};

// A D-Bus object implementation class. Manages the interfaces implemented
// by this object.
class CHROMEOS_EXPORT DBusObject {
 public:
  // object_manager - ExportedObjectManager instance that notifies D-Bus
  //                  listeners of a new interface being claimed and property
  //                  changes on those interfaces.
  // object_path - D-Bus object path for the object instance.
  DBusObject(ExportedObjectManager* object_manager,
             const scoped_refptr<dbus::Bus>& bus,
             const dbus::ObjectPath& object_path);
  virtual ~DBusObject();

  // Returns an proxy handler for the interface |interface_name|. If the
  // interface proxy does not exist yet, it will be automatically created.
  DBusInterface* AddOrGetInterface(const std::string& interface_name);

  // Registers the object instance with D-Bus. This is an asynchronous call
  // that will call |completion_callback| when the object and all of its
  // interfaces are registered.
  virtual void RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback);

  // Finds a handler for the given method of a specific interface.
  // Returns nullptr if the interface is not registered or there is
  // no method with the specified name found on that interface.
  DBusInterfaceMethodHandler* FindMethodHandler(
      const std::string& interface_name, const std::string& method_name) const;

  // Returns the ExportedObjectManager proxy, if any. If DBusObject has been
  // constructed without an object manager, this method returns an empty
  // smart pointer (containing nullptr).
  const base::WeakPtr<ExportedObjectManager>& GetObjectManager() const {
    return object_manager_;
  }

  // Sends a signal from the exported D-Bus object.
  bool SendSignal(dbus::Signal* signal);

 private:
  // A map of all the interfaces added to this object.
  std::map<std::string, std::unique_ptr<DBusInterface>> interfaces_;
  // Exported property set for properties registered with the interfaces
  // implemented by this D-Bus object.
  ExportedPropertySet property_set_;
  // Delegate object implementing org.freedesktop.DBus.ObjectManager interface.
  base::WeakPtr<ExportedObjectManager> object_manager_;
  // D-Bus bus object.
  scoped_refptr<dbus::Bus> bus_;
  // D-Bus object path for this object.
  dbus::ObjectPath object_path_;
  // D-Bus object instance once this object is successfully exported.
  dbus::ExportedObject* exported_object_ = nullptr;  // weak; owned by |bus_|.

  friend class DBusInterface;
  DISALLOW_COPY_AND_ASSIGN(DBusObject);
};

// Dispatches a D-Bus method call to the corresponding handler.
// Used mostly for testing purposes. This method is inlined so that it is
// not included in the shipping code of libchromeos, and included at the
// call sites.
inline std::unique_ptr<dbus::Response> CallMethod(
    const DBusObject& object, dbus::MethodCall* method_call) {
  DBusInterfaceMethodHandler* handler = object.FindMethodHandler(
      method_call->GetInterface(), method_call->GetMember());
  std::unique_ptr<dbus::Response> response;
  if (!handler) {
    response = CreateDBusErrorResponse(
        method_call,
        DBUS_ERROR_UNKNOWN_METHOD,
        "Unknown method");
  } else {
    response = handler->HandleMethod(method_call);
  }
  return response;
}

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_H_
