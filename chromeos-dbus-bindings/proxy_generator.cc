// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/proxy_generator.h"

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "chromeos-dbus-bindings/dbus_signature.h"
#include "chromeos-dbus-bindings/indented_text.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

// static
bool ProxyGenerator::GenerateProxy(
    const Interface& interface,
    const base::FilePath& output_file) {
  IndentedText text;
  text.AddLine(StringPrintf("// Automatic generation of interface for %s",
                            interface.name.c_str()));
  string header_guard = GenerateHeaderGuard(output_file, interface.name);
  text.AddLine(StringPrintf("#ifndef %s", header_guard.c_str()));
  text.AddLine(StringPrintf("#define %s", header_guard.c_str()));
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
  text.AddLine("#include <chromeos/dbus/dbus_signal_handler.h>");
  text.AddLine("#include <chromeos/errors/error.h>");
  text.AddLine("#include <dbus/bus.h>");
  text.AddLine("#include <dbus/message.h>");
  text.AddLine("#include <dbus/object_path.h>");
  text.AddLine("#include <dbus/object_proxy.h>");
  text.AddBlankLine();

  vector<string> namespaces;
  string class_name;
  CHECK(GetNamespacesAndClassName(interface.name, &namespaces, &class_name));
  for (const auto& space : namespaces) {
    text.AddLine(StringPrintf("namespace %s {", space.c_str()));
  }
  text.AddBlankLine();

  string proxy_name = StringPrintf("%sProxy", class_name.c_str());
  text.AddLine(StringPrintf("class %s {", proxy_name.c_str()));
  text.AddLineWithOffset("public:", kScopeOffset);
  text.PushOffset(kBlockOffset);
  AddMethodInterface(interface, &text);
  AddConstructor(interface, proxy_name, &text);
  AddDestructor(proxy_name, &text);
  AddSignalConnectedCallback(&text);
  for (const auto& method : interface.methods) {
    AddMethodProxy(method, interface.name, &text);
  }

  text.PopOffset();
  text.AddBlankLine();
  text.AddLineWithOffset("private:", kScopeOffset);

  text.PushOffset(kBlockOffset);
  text.AddLine("scoped_refptr<dbus::Bus> bus_;");
  text.AddLine("std::string service_name_;");
  text.AddLine("dbus::ObjectPath object_path_;");
  text.AddLine("dbus::ObjectProxy* dbus_object_proxy_;");
  text.AddBlankLine();

  text.AddLine(StringPrintf(
      "DISALLOW_COPY_AND_ASSIGN(%s);", proxy_name.c_str()));
  text.PopOffset();
  text.AddLine("};");

  text.AddBlankLine();

  for (auto it = namespaces.rbegin(); it != namespaces.rend(); ++it) {
    text.AddLine(StringPrintf("}  // namespace %s", it->c_str()));
  }
  text.AddLine(StringPrintf("#endif  // %s", header_guard.c_str()));

  return WriteTextToFile(output_file, text);
}

// static
void ProxyGenerator::AddConstructor(const Interface& interface,
                                      const string& class_name,
                                      IndentedText* text) {
  IndentedText block;
  block.AddLine(StringPrintf("%s(", class_name.c_str()));
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("const scoped_refptr<dbus::Bus>& bus,");
  block.AddLine("const std::string& service_name,");
  block.AddLine("const std::string& object_path,");
  block.AddLine("SignalReceiver* signal_receiver)");
  block.AddLine(": bus_(bus),");
  block.PushOffset(kBlockOffset);
  block.AddLine("service_name_(service_name),");
  block.AddLine("object_path_(object_path),");
  block.AddLine("dbus_object_proxy_(");
  block.AddLineWithOffset(
      "bus_->GetObjectProxy(service_name_, object_path_)) {",
      kLineContinuationOffset);
  block.PopOffset();
  block.PopOffset();
  block.PushOffset(kBlockOffset);
  for (const auto& signal : interface.signals) {
    block.AddLine("chromeos::dbus_utils::ConnectToSignal(");
    block.PushOffset(kLineContinuationOffset);
    block.AddLine("dbus_object_proxy_,");
    block.AddLine(StringPrintf("\"%s\",", interface.name.c_str()));
    block.AddLine(StringPrintf("\"%s\",", signal.name.c_str()));
    block.AddLine("base::Bind(");
    block.PushOffset(kLineContinuationOffset);
    block.AddLine(StringPrintf(
        "&SignalReceiver::%s,",
        GetHandlerNameForSignal(signal.name).c_str()));
    block.AddLine("base::Unretained(signal_receiver)),");
    block.PopOffset();
    block.AddLine("base::Bind(");
    block.PushOffset(kLineContinuationOffset);
    block.AddLine(StringPrintf(
        "&%s::OnDBusSignalConnected,", class_name.c_str()));
    block.AddLine("base::Unretained(this)));");
    block.PopOffset();
    block.PopOffset();
  }
  block.PopOffset();
  block.AddLine("}");

  text->AddBlock(block);
}

// static
void ProxyGenerator::AddDestructor(const string& class_name,
                                   IndentedText* text) {
  IndentedText block;
  block.AddLine(StringPrintf("virtual ~%s() {", class_name.c_str()));
  block.PushOffset(kBlockOffset);
  block.AddLine("dbus_object_proxy_->Detach();");
  block.AddLine(
      "bus_->RemoveObjectProxy(service_name_, object_path_, base::Closure());");
  block.PopOffset();
  block.AddLine("}");
  text->AddBlock(block);
}

// static
void ProxyGenerator::AddSignalConnectedCallback(IndentedText* text) {
  IndentedText block;
  block.AddLine("void OnDBusSignalConnected(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("const std::string& interface,");
  block.AddLine("const std::string& signal,");
  block.AddLine("bool success) {");
  block.PopOffset();
  block.PushOffset(kBlockOffset);
  block.AddLine("if (!success) {");
  block.PushOffset(kBlockOffset);
  block.AddLine("LOG(ERROR)");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("<< \"Failed to connect to \" << interface << \".\" << signal");
  block.AddLine("<< \" for \" << service_name_ << \" at \"");
  block.AddLine("<< object_path_.value();");
  block.PopOffset();
  block.PopOffset();
  block.AddLine("}");
  block.PopOffset();
  block.AddLine("}");
  text->AddBlock(block);
}

// static
void ProxyGenerator::AddMethodInterface(const Interface& interface,
                                        IndentedText* text) {
  IndentedText block;
  block.AddLine("class SignalReceiver {");
  block.AddLineWithOffset("public:", kScopeOffset);
  block.PushOffset(kBlockOffset);
  DbusSignature signature;
  for (const auto& signal : interface.signals) {
    if (signal.arguments.empty()) {
      block.AddLine(StringPrintf("virtual void %s() {}",
                                 GetHandlerNameForSignal(signal.name).c_str()));
      continue;
    }
    block.AddLine(StringPrintf("virtual void %s(",
                               GetHandlerNameForSignal(signal.name).c_str()));
    block.PushOffset(kLineContinuationOffset);
    string last_argument;
    vector<string> argument_types;
    for (const auto& argument : signal.arguments) {
      if (!last_argument.empty()) {
         block.AddLine(StringPrintf("%s,", last_argument.c_str()));
      }
      CHECK(signature.Parse(argument.type, &last_argument));
      if (!IsIntegralType(last_argument)) {
        last_argument = StringPrintf("const %s&", last_argument.c_str());
      }
    }
    block.AddLine(StringPrintf("%s) {}", last_argument.c_str()));
    block.PopOffset();
  }
  block.PopOffset();
  block.AddLine("};");

  text->AddBlock(block);
}

// static
void ProxyGenerator::AddMethodProxy(const Interface::Method& method,
                                    const string& interface_name,
                                    IndentedText* text) {
  IndentedText block;
  string return_type("void");
  bool is_void_method = true;
  DbusSignature signature;
  if (!method.output_arguments.empty()) {
    if (method.output_arguments.size() > 1) {
      LOG(WARNING) << "Method " << method.name << " has "
                   << method.output_arguments.size()
                   << " output arguments which is unsupported.";
      return;
    }
    CHECK(signature.Parse(method.output_arguments[0].type, &return_type));
    is_void_method = false;
  }
  block.AddLine(StringPrintf("virtual %s %s(",
                             return_type.c_str(), method.name.c_str()));
  block.PushOffset(kLineContinuationOffset);
  vector<string> argument_names;
  int argument_number = 0;
  for (const auto& argument : method.input_arguments) {
    string argument_type;
    CHECK(signature.Parse(argument.type, &argument_type));
    if (!IsIntegralType(argument_type)) {
      argument_type = StringPrintf("const %s&", argument_type.c_str());
    }
    ++argument_number;
    string argument_prefix(argument.name.empty() ?
                           StringPrintf("argument%d", argument_number) :
                           argument.name);
    string argument_name(StringPrintf("%s_in", argument_prefix.c_str()));
    argument_names.push_back(argument_name);
    block.AddLine(StringPrintf(
        "%s %s,", argument_type.c_str(), argument_name.c_str()));
  }
  block.AddLine("chromeos::ErrorPtr* error) {");
  block.PopOffset();
  block.PushOffset(kBlockOffset);

  block.AddLine("auto response = chromeos::dbus_utils::CallMethodAndBlock(");
  block.PushOffset(kLineContinuationOffset);
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

  if (!is_void_method) {
    block.AddLine(StringPrintf("%s result{};", return_type.c_str()));
  }
  block.AddLine("if (!response) {");
  block.AddLineWithOffset(StringPrintf(
      "return%s;", is_void_method ? "" : " result"), kBlockOffset);
  block.AddLine("}");
  block.AddLine("chromeos::dbus_utils::ExtractMethodCallResults(");
  block.AddLineWithOffset(StringPrintf("response.get(), error%s);",
                                       is_void_method ? "" : ", &result"),
                          kLineContinuationOffset);
  if (!is_void_method) {
    block.AddLine("return result;");
  }

  block.PopOffset();
  block.AddLine("}");

  text->AddBlock(block);
}

// static
string ProxyGenerator::GetHandlerNameForSignal(const string& signal) {
  return StringPrintf("On%sSignal", signal.c_str());
}

}  // namespace chromeos_dbus_bindings
