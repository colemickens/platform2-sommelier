// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <memory>
#include <utility>

#include <base/json/json_writer.h>
#include <base/memory/ptr_util.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>

#include "debugd/src/helpers/system_service_proxy.h"

namespace debugd {
namespace {

const char* const kModemInterfaces[] = {
    cromo::kModemInterface,
    cromo::kModemSimpleInterface,
    cromo::kModemGsmInterface,
    cromo::kModemGsmCardInterface,
    cromo::kModemGsmNetworkInterface,
    cromo::kModemCdmaInterface,
};

const char kModemManagerInterface[] = "org.freedesktop.ModemManager";
const char kModemManangerEnumerateDevicesMethod[] = "EnumerateDevices";
const char kModemManangerGetStatusMethod[] = "GetStatus";
const char kModemManangerGetInfoMethod[] = "GetInfo";

class CromoProxy : public SystemServiceProxy {
 public:
  static std::unique_ptr<CromoProxy> Create() {
    scoped_refptr<dbus::Bus> bus = ConnectToSystemBus();
    if (!bus)
      return nullptr;

    return std::unique_ptr<CromoProxy>(new CromoProxy(bus));
  }

  ~CromoProxy() override = default;

  std::unique_ptr<base::ListValue> EnumerateDevices() {
    dbus::MethodCall method_call(kModemManagerInterface,
                                 kModemManangerEnumerateDevicesMethod);
    auto result = base::ListValue::From(CallMethodAndGetResponse(
        dbus::ObjectPath(cromo::kCromoServicePath), &method_call));
    return result ? std::move(result) : base::MakeUnique<base::ListValue>();
  }

  std::unique_ptr<base::DictionaryValue> GetModemProperties(
      const dbus::ObjectPath& object_path) {
    auto result = base::MakeUnique<base::DictionaryValue>();
    result->SetString("service", cromo::kCromoServicePath);
    result->SetString("path", object_path.value());
    result->Set("status", GetStatus(object_path));
    result->Set("info", GetInfo(object_path));
    result->Set("properties", GetInterfaceProperties(object_path));
    return result;
  }

  std::unique_ptr<base::DictionaryValue> GetStatus(
      const dbus::ObjectPath& object_path) {
    dbus::MethodCall method_call(cromo::kModemSimpleInterface,
                                 kModemManangerGetStatusMethod);
    auto result = base::DictionaryValue::From(
        CallMethodAndGetResponse(object_path, &method_call));
    return result ? std::move(result)
                  : base::MakeUnique<base::DictionaryValue>();
  }

  std::unique_ptr<base::DictionaryValue> GetInfo(
      const dbus::ObjectPath& object_path) {
    dbus::MethodCall method_call(cromo::kModemInterface,
                                 kModemManangerGetInfoMethod);
    auto result = base::MakeUnique<base::DictionaryValue>();
    auto info = base::ListValue::From(
        CallMethodAndGetResponse(object_path, &method_call));
    if (info && info->GetSize() == 3) {
      std::string manufacturer, modem, version;
      if (info->GetString(0, &manufacturer) && info->GetString(1, &modem) &&
          info->GetString(2, &version)) {
        result->SetString("manufacturer", manufacturer);
        result->SetString("modem", modem);
        result->SetString("version", version);
      }
    }
    return result;
  }

  std::unique_ptr<base::DictionaryValue> GetInterfaceProperties(
      const dbus::ObjectPath& object_path) {
    auto result = base::MakeUnique<base::DictionaryValue>();
    for (std::string interface : kModemInterfaces) {
      auto interface_properties = GetProperties(interface, object_path);
      if (interface_properties) {
        result->Set(interface, std::move(interface_properties));
      }
    }
    return result;
  }

 private:
  explicit CromoProxy(scoped_refptr<dbus::Bus> bus)
      : SystemServiceProxy(bus, cromo::kCromoServiceName) {}

  DISALLOW_COPY_AND_ASSIGN(CromoProxy);
};

std::unique_ptr<base::Value> CollectModemStatus() {
  auto result = base::MakeUnique<base::ListValue>();

  auto proxy = CromoProxy::Create();
  if (!proxy)
    return std::move(result);

  auto modem_paths = proxy->EnumerateDevices();
  for (size_t i = 0; i < modem_paths->GetSize(); ++i) {
    std::string modem_path;
    if (modem_paths->GetString(i, &modem_path)) {
      result->Append(proxy->GetModemProperties(dbus::ObjectPath(modem_path)));
    }
  }
  return std::move(result);
}

}  // namespace
}  // namespace debugd

int main() {
  auto result = debugd::CollectModemStatus();
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *result, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  printf("%s\n", json.c_str());
  return 0;
}
