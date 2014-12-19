// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/proxy_generator.h"

#include <utility>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/strings/string_utils.h>

#include "chromeos-dbus-bindings/dbus_signature.h"
#include "chromeos-dbus-bindings/indented_text.h"
#include "chromeos-dbus-bindings/name_parser.h"

using base::StringPrintf;
using std::pair;
using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

namespace {
// Helper struct to encapsulate information about method call parameter during
// code generation.
struct ParamDef {
  ParamDef(const string& param_type, const string& param_name, bool param_ref)
      : type(param_type), name(param_name), is_const_ref(param_ref) {}

  string type;
  string name;
  bool is_const_ref;
};

string GetParamString(const ParamDef& param_def) {
  return StringPrintf(param_def.is_const_ref ? "const %s& %s" : "%s* %s",
                      param_def.type.c_str(), param_def.name.c_str());
}
}  // anonymous namespace

// static
bool ProxyGenerator::GenerateProxies(
    const ServiceConfig& config,
    const std::vector<Interface>& interfaces,
    const base::FilePath& output_file) {
  IndentedText text;

  text.AddLine("// Automatic generation of D-Bus interfaces:");
  for (const auto& interface : interfaces) {
    text.AddLine(StringPrintf("//  - %s", interface.name.c_str()));
  }
  string header_guard = GenerateHeaderGuard(output_file);
  text.AddLine(StringPrintf("#ifndef %s", header_guard.c_str()));
  text.AddLine(StringPrintf("#define %s", header_guard.c_str()));
  text.AddLine("#include <memory>");
  text.AddLine("#include <string>");
  text.AddLine("#include <vector>");
  text.AddBlankLine();
  text.AddLine("#include <base/bind.h>");
  text.AddLine("#include <base/callback.h>");
  text.AddLine("#include <base/logging.h>");
  text.AddLine("#include <base/macros.h>");
  text.AddLine("#include <base/memory/ref_counted.h>");
  text.AddLine("#include <chromeos/any.h>");
  text.AddLine("#include <chromeos/dbus/dbus_method_invoker.h>");
  text.AddLine("#include <chromeos/dbus/dbus_property.h>");
  text.AddLine("#include <chromeos/dbus/dbus_signal_handler.h>");
  text.AddLine("#include <chromeos/errors/error.h>");
  text.AddLine("#include <chromeos/variant_dictionary.h>");
  text.AddLine("#include <dbus/bus.h>");
  text.AddLine("#include <dbus/message.h>");
  text.AddLine("#include <dbus/object_manager.h>");
  text.AddLine("#include <dbus/object_path.h>");
  text.AddLine("#include <dbus/object_proxy.h>");
  text.AddBlankLine();

  if (!config.object_manager.name.empty()) {
    // Add forward-declaration for Object Manager proxy class.
    NameParser parser{config.object_manager.name};
    parser.AddOpenNamespaces(&text, false);
    text.AddLine(StringPrintf("class %s;",
                              parser.MakeProxyName(false).c_str()));
    parser.AddCloseNamespaces(&text, false);
    text.AddBlankLine();
  }

  for (const auto& interface : interfaces) {
    GenerateInterfaceProxy(config, interface, &text);
  }

  ObjectManager::GenerateProxy(config, interfaces, &text);

  text.AddLine(StringPrintf("#endif  // %s", header_guard.c_str()));
  return WriteTextToFile(output_file, text);
}

// static
void ProxyGenerator::GenerateInterfaceProxy(const ServiceConfig& config,
                                            const Interface& interface,
                                            IndentedText* text) {
  NameParser parser{interface.name};
  string proxy_name = parser.MakeProxyName(false);

  parser.AddOpenNamespaces(text, false);
  text->AddBlankLine();

  text->AddLine(StringPrintf("// Interface proxy for %s.",
                             parser.MakeFullCppName().c_str()));
  text->AddComments(interface.doc_string);
  text->AddLine(StringPrintf("class %s final {", proxy_name.c_str()));
  text->AddLineWithOffset("public:", kScopeOffset);
  text->PushOffset(kBlockOffset);
  AddPropertySet(config, interface, text);
  AddConstructor(config, interface, proxy_name, text);
  AddDestructor(proxy_name, text);
  AddSignalHandlerRegistration(interface, text);
  AddReleaseObjectProxy(text);
  AddGetObjectPath(text);
  AddGetObjectProxy(text);
  if (!config.object_manager.name.empty() && !interface.properties.empty())
    AddPropertyPublicMethods(proxy_name, text);
  for (const auto& method : interface.methods) {
    AddMethodProxy(method, interface.name, text);
    AddAsyncMethodProxy(method, interface.name, text);
  }
  AddProperties(config, interface, text);

  text->PopOffset();
  text->AddLineWithOffset("private:", kScopeOffset);

  text->PushOffset(kBlockOffset);
  if (!config.object_manager.name.empty() && !interface.properties.empty())
    AddOnPropertyChanged(text);
  text->AddLine("scoped_refptr<dbus::Bus> bus_;");
  if (config.service_name.empty()) {
    text->AddLine("std::string service_name_;");
  } else {
    text->AddLine(StringPrintf("const std::string service_name_{\"%s\"};",
                               config.service_name.c_str()));
  }
  if (interface.path.empty()) {
    text->AddLine("dbus::ObjectPath object_path_;");
  } else {
    text->AddLine(StringPrintf("const dbus::ObjectPath object_path_{\"%s\"};",
                               interface.path.c_str()));
  }
  if (!config.object_manager.name.empty() && !interface.properties.empty()) {
    text->AddLine("PropertySet* property_set_;");
    text->AddLine(StringPrintf("base::Callback<void(%s*, const std::string&)> "
                               "on_property_changed_;",
                               proxy_name.c_str()));
  }
  text->AddLine("dbus::ObjectProxy* dbus_object_proxy_;");
  text->AddBlankLine();

  if (!config.object_manager.name.empty() && !interface.properties.empty()) {
    text->AddLine(StringPrintf(
        "friend class %s;",
        NameParser{config.object_manager.name}.MakeProxyName(true).c_str()));
  }
  text->AddLine(StringPrintf("DISALLOW_COPY_AND_ASSIGN(%s);",
                             proxy_name.c_str()));
  text->PopOffset();
  text->AddLine("};");

  text->AddBlankLine();

  parser.AddCloseNamespaces(text, false);

  text->AddBlankLine();
}

// static
void ProxyGenerator::AddConstructor(const ServiceConfig& config,
                                    const Interface& interface,
                                    const string& class_name,
                                    IndentedText* text) {
  IndentedText block;
  vector<ParamDef> args{{"scoped_refptr<dbus::Bus>", "bus", true}};
  if (config.service_name.empty())
    args.emplace_back("std::string", "service_name", true);
  if (interface.path.empty())
    args.emplace_back("dbus::ObjectPath", "object_path", true);
  if (!config.object_manager.name.empty() && !interface.properties.empty())
    args.emplace_back("PropertySet", "property_set", false);

  if (args.size() == 1) {
    block.AddLine(StringPrintf("%s(%s) :", class_name.c_str(),
                               GetParamString(args.front()).c_str()));
  } else {
    block.AddLine(StringPrintf("%s(", class_name.c_str()));
    block.PushOffset(kLineContinuationOffset);
    for (size_t i = 0; i < args.size() - 1; i++) {
      block.AddLine(StringPrintf("%s,", GetParamString(args[i]).c_str()));
    }
    block.AddLine(StringPrintf("%s) :", GetParamString(args.back()).c_str()));
  }
  block.PushOffset(kLineContinuationOffset);
  for (const auto& arg : args) {
    block.AddLine(StringPrintf("%s_{%s},", arg.name.c_str(),
                               arg.name.c_str()));
  }
  block.AddLine("dbus_object_proxy_{");
  block.AddLineWithOffset(
      "bus_->GetObjectProxy(service_name_, object_path_)} {",
      kLineContinuationOffset);
  block.PopOffset();
  if (args.size() > 1)
    block.PopOffset();
  block.AddLine("}");
  block.AddBlankLine();
  text->AddBlock(block);
}

// static
void ProxyGenerator::AddDestructor(const string& class_name,
                                   IndentedText* text) {
  IndentedText block;
  block.AddLine(StringPrintf("~%s() {", class_name.c_str()));
  block.AddLine("}");
  block.AddBlankLine();
  text->AddBlock(block);
}

// static
void ProxyGenerator::AddReleaseObjectProxy(IndentedText* text) {
  text->AddLine("void ReleaseObjectProxy(const base::Closure& callback) {");
  text->AddLineWithOffset(
      "bus_->RemoveObjectProxy(service_name_, object_path_, callback);",
      kBlockOffset);
  text->AddLine("}");
  text->AddBlankLine();
}

// static
void ProxyGenerator::AddGetObjectPath(IndentedText* text) {
  text->AddLine("const dbus::ObjectPath& GetObjectPath() const {");
  text->AddLineWithOffset("return object_path_;", kBlockOffset);
  text->AddLine("}");
  text->AddBlankLine();
}

// static
void ProxyGenerator::AddGetObjectProxy(IndentedText* text) {
  text->AddLine("dbus::ObjectProxy* GetObjectProxy() const { "
                "return dbus_object_proxy_; }");
  text->AddBlankLine();
}

// static
void ProxyGenerator::AddPropertyPublicMethods(const string& class_name,
                                              IndentedText* text) {
  text->AddLine("void SetPropertyChangedCallback(");
  text->AddLineWithOffset(
      StringPrintf("const base::Callback<void(%s*, "
                   "const std::string&)>& callback) {", class_name.c_str()),
      kLineContinuationOffset);
  text->AddLineWithOffset("on_property_changed_ = callback;", kBlockOffset);
  text->AddLine("}");
  text->AddBlankLine();

  text->AddLine("const PropertySet* GetProperties() const "
                "{ return property_set_; }");
  text->AddLine("PropertySet* GetProperties() { return property_set_; }");
  text->AddBlankLine();
}

// static
void ProxyGenerator::AddOnPropertyChanged(IndentedText* text) {
  text->AddLine("void OnPropertyChanged(const std::string& property_name) {");
  text->PushOffset(kBlockOffset);
  text->AddLine("if (!on_property_changed_.is_null())");
  text->PushOffset(kBlockOffset);
  text->AddLine("on_property_changed_.Run(this, property_name);");
  text->PopOffset();
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::AddSignalHandlerRegistration(const Interface& interface,
                                                  IndentedText* text) {
  IndentedText block;
  DbusSignature signature;
  for (const auto& signal : interface.signals) {
    block.AddLine(
        StringPrintf("void Register%sSignalHandler(", signal.name.c_str()));
    block.PushOffset(kLineContinuationOffset);
    if (signal.arguments.empty()) {
      block.AddLine("const base::Closure& signal_callback,");
    } else {
      string last_argument;
      string prefix{"const base::Callback<void("};
      for (const auto argument : signal.arguments) {
        if (!last_argument.empty()) {
          if (!prefix.empty()) {
            block.AddLineAndPushOffsetTo(
                StringPrintf("%s%s,", prefix.c_str(), last_argument.c_str()),
                1, '(');
            prefix.clear();
          } else {
            block.AddLine(StringPrintf("%s,", last_argument.c_str()));
          }
        }
        CHECK(signature.Parse(argument.type, &last_argument));
        MakeConstReferenceIfNeeded(&last_argument);
      }
      block.AddLine(StringPrintf("%s%s)>& signal_callback,",
                                 prefix.c_str(),
                                 last_argument.c_str()));
      if (prefix.empty()) {
        block.PopOffset();
      }
    }
    block.AddLine("dbus::ObjectProxy::OnConnectedCallback "
                  "on_connected_callback) {");
    block.PopOffset();  // Method signature arguments
    block.PushOffset(kBlockOffset);
    block.AddLine("chromeos::dbus_utils::ConnectToSignal(");
    block.PushOffset(kLineContinuationOffset);
    block.AddLine("dbus_object_proxy_,");
    block.AddLine(StringPrintf("\"%s\",", interface.name.c_str()));
    block.AddLine(StringPrintf("\"%s\",", signal.name.c_str()));
    block.AddLine("signal_callback,");
    block.AddLine("on_connected_callback);");
    block.PopOffset();  // Function call line continuation
    block.PopOffset();  // Method body
    block.AddLine("}");
    block.AddBlankLine();
  }
  text->AddBlock(block);
}

// static
void ProxyGenerator::AddPropertySet(const ServiceConfig& config,
                                    const Interface& interface,
                                    IndentedText* text) {
  // Must have ObjectManager in order for property system to work correctly.
  if (config.object_manager.name.empty())
    return;

  IndentedText block;
  block.AddLine("class PropertySet : public dbus::PropertySet {");
  block.AddLineWithOffset("public:", kScopeOffset);
  block.PushOffset(kBlockOffset);
  block.AddLineAndPushOffsetTo("PropertySet(dbus::ObjectProxy* object_proxy,",
                               1, '(');
  block.AddLine("const PropertyChangedCallback& callback)");
  block.PopOffset();
  block.PushOffset(kLineContinuationOffset);
  block.AddLineAndPushOffsetTo(": dbus::PropertySet{object_proxy,", 1, '{');
  block.AddLine(StringPrintf("\"%s\",", interface.name.c_str()));
  block.AddLine("callback} {");
  block.PopOffset();
  block.PopOffset();
  block.PushOffset(kBlockOffset);
  for (const auto& prop : interface.properties) {
    block.AddLine(
        StringPrintf("RegisterProperty(\"%s\", &%s);",
                     prop.name.c_str(),
                     NameParser{prop.name}.MakeVariableName().c_str()));
  }
  block.PopOffset();
  block.AddLine("}");
  block.AddBlankLine();

  DbusSignature signature;
  for (const auto& prop : interface.properties) {
    string type;
    CHECK(signature.Parse(prop.type, &type));
    block.AddLine(
        StringPrintf("chromeos::dbus_utils::Property<%s> %s;",
                     type.c_str(),
                     NameParser{prop.name}.MakeVariableName().c_str()));
  }
  block.AddBlankLine();

  block.PopOffset();
  block.AddLineWithOffset("private:", kScopeOffset);
  block.AddLineWithOffset("DISALLOW_COPY_AND_ASSIGN(PropertySet);",
                          kBlockOffset);
  block.AddLine("};");
  block.AddBlankLine();

  text->AddBlock(block);
}

// static
void ProxyGenerator::AddProperties(const ServiceConfig& config,
                                   const Interface& interface,
                                   IndentedText* text) {
  // Must have ObjectManager in order for property system to work correctly.
  if (config.object_manager.name.empty())
    return;

  DbusSignature signature;
  for (const auto& prop : interface.properties) {
    string type;
    CHECK(signature.Parse(prop.type, &type));
    MakeConstReferenceIfNeeded(&type);
    string name = NameParser{prop.name}.MakeVariableName();
    text->AddLine(
        StringPrintf("%s %s() const {",
                     type.c_str(),
                     name.c_str()));
    text->AddLineWithOffset(
        StringPrintf("return property_set_->%s.value();", name.c_str()),
        kBlockOffset);
    text->AddLine("}");
    text->AddBlankLine();
  }
}

// static
void ProxyGenerator::AddMethodProxy(const Interface::Method& method,
                                    const string& interface_name,
                                    IndentedText* text) {
  IndentedText block;
  DbusSignature signature;
  block.AddComments(method.doc_string);
  block.AddLine(StringPrintf("bool %s(", method.name.c_str()));
  block.PushOffset(kLineContinuationOffset);
  vector<string> argument_names;
  int argument_number = 0;
  for (const auto& argument : method.input_arguments) {
    string argument_type;
    CHECK(signature.Parse(argument.type, &argument_type));
    MakeConstReferenceIfNeeded(&argument_type);
    string argument_name = GetArgName("in", argument.name, ++argument_number);
    argument_names.push_back(argument_name);
    block.AddLine(StringPrintf(
        "%s %s,", argument_type.c_str(), argument_name.c_str()));
  }
  vector<string> out_param_names{"response.get()", "error"};
  for (const auto& argument : method.output_arguments) {
    string argument_type;
    CHECK(signature.Parse(argument.type, &argument_type));
    string argument_name = GetArgName("out", argument.name, ++argument_number);
    out_param_names.push_back(argument_name);
    block.AddLine(StringPrintf(
        "%s* %s,", argument_type.c_str(), argument_name.c_str()));
  }
  block.AddLine("chromeos::ErrorPtr* error,");
  block.AddLine("int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {");
  block.PopOffset();
  block.PushOffset(kBlockOffset);

  block.AddLine(
      "auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("timeout_ms,");
  block.AddLine("dbus_object_proxy_,");
  block.AddLine(StringPrintf("\"%s\",", interface_name.c_str()));
  block.AddLine(StringPrintf("\"%s\",", method.name.c_str()));
  string last_argument = "error";
  for (const auto& argument_name : argument_names) {
    block.AddLine(StringPrintf("%s,", last_argument.c_str()));
    last_argument = argument_name;
  }
  block.AddLine(StringPrintf("%s);", last_argument.c_str()));
  block.PopOffset();

  block.AddLine("return response && "
                "chromeos::dbus_utils::ExtractMethodCallResults(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine(chromeos::string_utils::Join(", ", out_param_names) + ");");
  block.PopOffset();
  block.PopOffset();
  block.AddLine("}");
  block.AddBlankLine();

  text->AddBlock(block);
}

// static
void ProxyGenerator::AddAsyncMethodProxy(const Interface::Method& method,
                                         const string& interface_name,
                                         IndentedText* text) {
  IndentedText block;
  DbusSignature signature;
  block.AddComments(method.doc_string);
  block.AddLine(StringPrintf("void %sAsync(", method.name.c_str()));
  block.PushOffset(kLineContinuationOffset);
  vector<string> argument_names;
  int argument_number = 0;
  for (const auto& argument : method.input_arguments) {
    string argument_type;
    CHECK(signature.Parse(argument.type, &argument_type));
    MakeConstReferenceIfNeeded(&argument_type);
    string argument_name = GetArgName("in", argument.name, ++argument_number);
    argument_names.push_back(argument_name);
    block.AddLine(StringPrintf(
        "%s %s,", argument_type.c_str(), argument_name.c_str()));
  }
  vector<string> out_params;
  for (const auto& argument : method.output_arguments) {
    string argument_type;
    CHECK(signature.Parse(argument.type, &argument_type));
    MakeConstReferenceIfNeeded(&argument_type);
    if (!argument.name.empty())
      base::StringAppendF(&argument_type, " /*%s*/", argument.name.c_str());
    out_params.push_back(argument_type);
  }
  block.AddLine(StringPrintf(
      "const base::Callback<void(%s)>& success_callback,",
      chromeos::string_utils::Join(", ", out_params).c_str()));
  block.AddLine(
      "const base::Callback<void(chromeos::Error*)>& error_callback,");
  block.AddLine("int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {");
  block.PopOffset();
  block.PushOffset(kBlockOffset);

  block.AddLine("chromeos::dbus_utils::CallMethodWithTimeout(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("timeout_ms,");
  block.AddLine("dbus_object_proxy_,");
  block.AddLine(StringPrintf("\"%s\",", interface_name.c_str()));
  block.AddLine(StringPrintf("\"%s\",", method.name.c_str()));
  block.AddLine("success_callback,");
  string last_argument = "error_callback";
  for (const auto& argument_name : argument_names) {
    block.AddLine(StringPrintf("%s,", last_argument.c_str()));
    last_argument = argument_name;
  }
  block.AddLine(StringPrintf("%s);", last_argument.c_str()));
  block.PopOffset();

  block.PopOffset();
  block.AddLine("}");
  block.AddBlankLine();

  text->AddBlock(block);
}

// static
void ProxyGenerator::ObjectManager::GenerateProxy(
    const ServiceConfig& config,
    const std::vector<Interface>& interfaces,
    IndentedText* text) {
  if (config.object_manager.name.empty())
    return;

  NameParser object_manager{config.object_manager.name};
  object_manager.AddOpenNamespaces(text, false);
  text->AddBlankLine();

  string class_name = object_manager.type_name + "Proxy";
  text->AddLine(StringPrintf("class %s : "
                             "public dbus::ObjectManager::Interface {",
                             class_name.c_str()));
  text->AddLineWithOffset("public:", kScopeOffset);
  text->PushOffset(kBlockOffset);

  AddConstructor(config, class_name, interfaces, text);
  AddDestructor(class_name, interfaces, text);
  AddGetObjectManagerProxy(text);
  for (const auto& itf : interfaces) {
    AddInterfaceAccessors(itf, text);
  }
  text->PopOffset();

  text->AddLineWithOffset("private:", kScopeOffset);
  text->PushOffset(kBlockOffset);
  AddOnPropertyChanged(interfaces, text);
  AddObjectAdded(interfaces, text);
  AddObjectRemoved(interfaces, text);
  AddCreateProperties(interfaces, class_name, text);
  AddDataMembers(interfaces, class_name, text);

  text->AddLine(StringPrintf("DISALLOW_COPY_AND_ASSIGN(%s);",
                              class_name.c_str()));
  text->PopOffset();
  text->AddLine("};");
  text->AddBlankLine();
  object_manager.AddCloseNamespaces(text, false);
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddConstructor(
    const ServiceConfig& config,
    const std::string& class_name,
    const std::vector<Interface>& interfaces,
    IndentedText* text) {
  if (config.service_name.empty()) {
    text->AddLineAndPushOffsetTo(
        StringPrintf("%s(const scoped_refptr<dbus::Bus>& bus,",
                     class_name.c_str()),
        1, '(');
    text->AddLine("const std::string& service_name)");
    text->PopOffset();
  } else {
    text->AddLine(StringPrintf("%s(const scoped_refptr<dbus::Bus>& bus)",
                               class_name.c_str()));
  }
  text->PushOffset(kLineContinuationOffset);
  text->AddLine(": bus_{bus},");
  text->PushOffset(kBlockOffset);
  text->AddLine("dbus_object_manager_{bus->GetObjectManager(");
  text->PushOffset(kLineContinuationOffset);
  if (config.service_name.empty()) {
    text->AddLine("service_name,");
  } else {
    text->AddLine(StringPrintf("\"%s\",", config.service_name.c_str()));
  }
  text->AddLine(StringPrintf("dbus::ObjectPath{\"%s\"})} {",
                             config.object_manager.object_path.c_str()));
  text->PopOffset();
  text->PopOffset();
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  for (const auto& itf : interfaces) {
    text->AddLine(
        StringPrintf("dbus_object_manager_->RegisterInterface(\"%s\", this);",
                     itf.name.c_str()));
  }
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddDestructor(
    const std::string& class_name,
    const std::vector<Interface>& interfaces,
    IndentedText* text) {
  text->AddLine(StringPrintf("~%s() override {", class_name.c_str()));
  text->PushOffset(kBlockOffset);
  for (const auto& itf : interfaces) {
    text->AddLine(
        StringPrintf("dbus_object_manager_->UnregisterInterface(\"%s\");",
                     itf.name.c_str()));
  }
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddGetObjectManagerProxy(
    IndentedText* text) {
  text->AddLine("dbus::ObjectManager* GetObjectManagerProxy() const {");
  text->AddLineWithOffset("return dbus_object_manager_;", kBlockOffset);
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddInterfaceAccessors(
    const Interface& interface,
    IndentedText* text) {
  NameParser itf_name{interface.name};
  string map_name = itf_name.MakeVariableName() + "_instances_";

  // GetProxy().
  text->AddLine(StringPrintf("%s* Get%s(",
                              itf_name.MakeProxyName(true).c_str(),
                              itf_name.MakeProxyName(false).c_str()));
  text->PushOffset(kLineContinuationOffset);
  text->AddLine("const dbus::ObjectPath& object_path) {");
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  text->AddLine(StringPrintf("auto p = %s.find(object_path);",
                              map_name.c_str()));
  text->AddLine(StringPrintf("if (p != %s.end())", map_name.c_str()));
  text->PushOffset(kBlockOffset);
  text->AddLine("return p->second.get();");
  text->PopOffset();
  text->AddLine("return nullptr;");
  text->PopOffset();
  text->AddLine("}");

  // GetInstances().
  text->AddLine(StringPrintf("std::vector<%s*> Get%sInstances() const {",
                              itf_name.MakeProxyName(true).c_str(),
                              itf_name.type_name.c_str()));
  text->PushOffset(kBlockOffset);
  text->AddLine(StringPrintf("std::vector<%s*> values;",
                             itf_name.MakeProxyName(true).c_str()));
  text->AddLine(StringPrintf("values.reserve(%s.size());", map_name.c_str()));
  text->AddLine(StringPrintf("for (const auto& pair : %s)", map_name.c_str()));
  text->AddLineWithOffset("values.push_back(pair.second.get());", kBlockOffset);
  text->AddLine("return values;");
  text->PopOffset();
  text->AddLine("}");

  // SetAddedCallback().
  text->AddLine(StringPrintf("void Set%sAddedCallback(",
                              itf_name.type_name.c_str()));
  text->PushOffset(kLineContinuationOffset);
  text->AddLine(
      StringPrintf("const base::Callback<void(%s*)>& callback) {",
                    itf_name.MakeProxyName(true).c_str()));
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  text->AddLine(StringPrintf("on_%s_added_ = callback;",
                              itf_name.MakeVariableName().c_str()));
  text->PopOffset();
  text->AddLine("}");

  // SetRemovedCallback().
  text->AddLine(StringPrintf("void Set%sRemovedCallback(",
                              itf_name.type_name.c_str()));
  text->PushOffset(kLineContinuationOffset);
  text->AddLine("const base::Callback<void(const dbus::ObjectPath&)>& "
                "callback) {");
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  text->AddLine(StringPrintf("on_%s_removed_ = callback;",
                              itf_name.MakeVariableName().c_str()));
  text->PopOffset();
  text->AddLine("}");

  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddOnPropertyChanged(
    const std::vector<Interface>& interfaces,
    IndentedText* text) {
  text->AddLineAndPushOffsetTo("void OnPropertyChanged("
                               "const dbus::ObjectPath& object_path,",
                               1, '(');
  text->AddLine("const std::string& interface_name,");
  text->AddLine("const std::string& property_name) {");
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  for (const auto& itf : interfaces) {
    if (itf.properties.empty())
      continue;

    NameParser itf_name{itf.name};
    text->AddLine(StringPrintf("if (interface_name == \"%s\") {",
                               itf.name.c_str()));
    text->PushOffset(kBlockOffset);
    string map_name = itf_name.MakeVariableName() + "_instances_";
    text->AddLine(StringPrintf("auto p = %s.find(object_path);",
                               map_name.c_str()));
    text->AddLine(StringPrintf("if (p == %s.end())", map_name.c_str()));
    text->PushOffset(kBlockOffset);
    text->AddLine("return;");
    text->PopOffset();
    text->AddLine("p->second->OnPropertyChanged(property_name);");
    text->AddLine("return;");
    text->PopOffset();
    text->AddLine("}");
  }
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddObjectAdded(
    const std::vector<Interface>& interfaces,
    IndentedText* text) {
  text->AddLine("void ObjectAdded(");
  text->PushOffset(kLineContinuationOffset);
  text->AddLine("const dbus::ObjectPath& object_path,");
  text->AddLine("const std::string& interface_name) override {");
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  for (const auto& itf : interfaces) {
    NameParser itf_name{itf.name};
    string var_name = itf_name.MakeVariableName();
    text->AddLine(StringPrintf("if (interface_name == \"%s\") {",
                               itf.name.c_str()));
    text->PushOffset(kBlockOffset);
    if (!itf.properties.empty()) {
      text->AddLine("auto property_set =");
      text->PushOffset(kLineContinuationOffset);
      text->AddLine(StringPrintf("static_cast<%s::PropertySet*>(",
                                 itf_name.MakeProxyName(true).c_str()));
      text->PushOffset(kLineContinuationOffset);
      text->AddLine("dbus_object_manager_->GetProperties(object_path, "
                    "interface_name));");
      text->PopOffset();
      text->PopOffset();
    }
    text->AddLine(StringPrintf("std::unique_ptr<%s> %s_proxy{",
                               itf_name.MakeProxyName(true).c_str(),
                               var_name.c_str()));
    text->PushOffset(kBlockOffset);
    string new_instance = StringPrintf("new %s{bus_",
                                       itf_name.MakeProxyName(true).c_str());
    if (itf.path.empty())
      new_instance += ", object_path";
    if (!itf.properties.empty())
      new_instance += ", property_set";
    new_instance += "}";
    text->AddLine(new_instance);
    text->PopOffset();
    text->AddLine("};");
    text->AddLine(StringPrintf("auto p = %s_instances_.emplace(object_path, "
                               "std::move(%s_proxy));",
                               var_name.c_str(), var_name.c_str()));
    text->AddLine(StringPrintf("if (!on_%s_added_.is_null())",
                               var_name.c_str()));
    text->PushOffset(kBlockOffset);
    text->AddLine(StringPrintf("on_%s_added_.Run(p.first->second.get());",
                               var_name.c_str()));
    text->PopOffset();
    text->AddLine("return;");
    text->PopOffset();
    text->AddLine("}");
  }
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddObjectRemoved(
    const std::vector<Interface>& interfaces,
    IndentedText* text) {
  text->AddLine("void ObjectRemoved(");
  text->PushOffset(kLineContinuationOffset);
  text->AddLine("const dbus::ObjectPath& object_path,");
  text->AddLine("const std::string& interface_name) override {");
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  for (const auto& itf : interfaces) {
    NameParser itf_name{itf.name};
    string var_name = itf_name.MakeVariableName();
    text->AddLine(StringPrintf("if (interface_name == \"%s\") {",
                               itf.name.c_str()));
    text->PushOffset(kBlockOffset);
    text->AddLine(StringPrintf("auto p = %s_instances_.find(object_path);",
                               var_name.c_str()));
    text->AddLine(StringPrintf("if (p != %s_instances_.end()) {",
                               var_name.c_str()));
    text->PushOffset(kBlockOffset);
    text->AddLine(StringPrintf("if (!on_%s_removed_.is_null())",
                               var_name.c_str()));
    text->PushOffset(kBlockOffset);
    text->AddLine(StringPrintf("on_%s_removed_.Run(object_path);",
                               var_name.c_str()));
    text->PopOffset();
    text->AddLine(StringPrintf("%s_instances_.erase(p);",
                               var_name.c_str()));
    text->PopOffset();
    text->AddLine("}");
    text->AddLine("return;");
    text->PopOffset();
    text->AddLine("}");
  }
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddCreateProperties(
    const std::vector<Interface>& interfaces,
    const std::string& class_name,
    IndentedText* text) {
  text->AddLine("dbus::PropertySet* CreateProperties(");
  text->PushOffset(kLineContinuationOffset);
  text->AddLine("dbus::ObjectProxy* object_proxy,");
  text->AddLine("const dbus::ObjectPath& object_path,");
  text->AddLine("const std::string& interface_name) override {");
  text->PopOffset();
  text->PushOffset(kBlockOffset);
  for (const auto& itf : interfaces) {
    NameParser itf_name{itf.name};
    text->AddLine(StringPrintf("if (interface_name == \"%s\") {",
                               itf.name.c_str()));
    text->PushOffset(kBlockOffset);
    text->AddLine(StringPrintf("return new %s::PropertySet{",
                               itf_name.MakeProxyName(true).c_str()));
    text->PushOffset(kLineContinuationOffset);
    text->AddLine("object_proxy,");
    text->AddLineAndPushOffsetTo(
        StringPrintf("base::Bind(&%s::OnPropertyChanged,",
                     class_name.c_str()),
        1, '(');
    text->AddLine("weak_ptr_factory_.GetWeakPtr(),");
    text->AddLine("object_path,");
    text->AddLine("interface_name)");
    text->PopOffset();
    text->PopOffset();
    text->AddLine("};");
    text->PopOffset();
    text->AddLine("}");
  }
  text->AddLineAndPushOffsetTo("LOG(FATAL) << \"Creating properties for "
                               "unsupported interface \"", 1, ' ');
  text->AddLine("<< interface_name;");
  text->PopOffset();
  text->AddLine("return nullptr;");
  text->PopOffset();
  text->AddLine("}");
  text->AddBlankLine();
}

void ProxyGenerator::ObjectManager::AddDataMembers(
    const std::vector<Interface>& interfaces,
    const std::string& class_name,
    IndentedText* text) {
  text->AddLine("scoped_refptr<dbus::Bus> bus_;");
  text->AddLine("dbus::ObjectManager* dbus_object_manager_;");
  for (const auto& itf : interfaces) {
    NameParser itf_name{itf.name};
    string var_name = itf_name.MakeVariableName();
    text->AddLineAndPushOffsetTo("std::map<dbus::ObjectPath,", 1, '<');
    text->AddLine(StringPrintf("std::unique_ptr<%s>> %s_instances_;",
                               itf_name.MakeProxyName(true).c_str(),
                               var_name.c_str()));
    text->PopOffset();
    text->AddLine(StringPrintf("base::Callback<void(%s*)> on_%s_added_;",
                               itf_name.MakeProxyName(true).c_str(),
                               var_name.c_str()));
    text->AddLine(StringPrintf("base::Callback<void(const dbus::ObjectPath&)> "
                               "on_%s_removed_;",
                               var_name.c_str()));
  }
  text->AddLine(
      StringPrintf("base::WeakPtrFactory<%s> weak_ptr_factory_{this};",
                   class_name.c_str()));
  text->AddBlankLine();
}

// static
string ProxyGenerator::GetHandlerNameForSignal(const string& signal) {
  return StringPrintf("On%sSignal", signal.c_str());
}

}  // namespace chromeos_dbus_bindings
