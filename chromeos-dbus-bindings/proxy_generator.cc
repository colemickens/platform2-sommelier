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

using base::StringPrintf;
using std::pair;
using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

namespace {
string GetParamString(const pair<string, string>& param_def) {
  return StringPrintf("const %s& %s",
                      param_def.first.c_str(), param_def.second.c_str());
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
  text.AddLine("#include <chromeos/variant_dictionary.h>");
  text.AddLine("#include <dbus/bus.h>");
  text.AddLine("#include <dbus/message.h>");
  text.AddLine("#include <dbus/object_path.h>");
  text.AddLine("#include <dbus/object_proxy.h>");
  text.AddBlankLine();

  for (const auto& interface : interfaces) {
    GenerateInterfaceProxy(config, interface, &text);
  }

  text.AddLine(StringPrintf("#endif  // %s", header_guard.c_str()));
  return WriteTextToFile(output_file, text);
}

// static
void ProxyGenerator::GenerateInterfaceProxy(const ServiceConfig& config,
                                            const Interface& interface,
                                            IndentedText* text) {
  vector<string> namespaces;
  string itf_name;
  CHECK(GetNamespacesAndClassName(interface.name,
                                  &namespaces,
                                  &itf_name));
  string proxy_name = itf_name + "Proxy";

  for (const auto& space : namespaces) {
    text->AddLine(StringPrintf("namespace %s {", space.c_str()));
  }
  text->AddBlankLine();

  text->AddLine(StringPrintf("// Interface proxy for %s.",
                             GetFullClassName(namespaces, itf_name).c_str()));
  text->AddLine(StringPrintf("class %s final {", proxy_name.c_str()));
  text->AddLineWithOffset("public:", kScopeOffset);
  text->PushOffset(kBlockOffset);
  AddSignalReceiver(interface, text);
  AddConstructor(config, interface, proxy_name, text);
  AddDestructor(proxy_name, text);
  AddReleaseObjectProxy(text);
  if (!interface.signals.empty())
    AddSignalConnectedCallback(text);
  for (const auto& method : interface.methods) {
    AddMethodProxy(method, interface.name, text);
  }

  text->PopOffset();
  text->AddLineWithOffset("private:", kScopeOffset);

  text->PushOffset(kBlockOffset);
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
  text->AddLine("dbus::ObjectProxy* dbus_object_proxy_;");
  text->AddBlankLine();

  text->AddLine(StringPrintf(
      "DISALLOW_COPY_AND_ASSIGN(%s);", proxy_name.c_str()));
  text->PopOffset();
  text->AddLine("};");

  text->AddBlankLine();

  for (auto it = namespaces.rbegin(); it != namespaces.rend(); ++it) {
    text->AddLine(StringPrintf("}  // namespace %s", it->c_str()));
  }

  text->AddBlankLine();
}

// static
void ProxyGenerator::AddConstructor(const ServiceConfig& config,
                                    const Interface& interface,
                                    const string& class_name,
                                    IndentedText* text) {
  IndentedText block;
  vector<std::pair<string, string>> args{{"scoped_refptr<dbus::Bus>", "bus"}};
  if (config.service_name.empty())
    args.emplace_back("std::string", "service_name");
  if (interface.path.empty())
    args.emplace_back("std::string", "object_path");

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
    block.AddLine(StringPrintf("%s_(%s),", arg.second.c_str(),
                               arg.second.c_str()));
  }
  block.AddLine("dbus_object_proxy_(");
  block.AddLineWithOffset(
      "bus_->GetObjectProxy(service_name_, object_path_)) {",
      kLineContinuationOffset);
  block.PopOffset();
  if (args.size() > 1)
    block.PopOffset();
  block.AddLine("}");
  if (!interface.signals.empty()) {
    block.AddBlankLine();
    block.AddLine(StringPrintf("%s(", class_name.c_str()));
    block.PushOffset(kLineContinuationOffset);
    vector<string> param_names;
    for (const auto& arg : args) {
      block.AddLine(StringPrintf("%s,", GetParamString(arg).c_str()));
      param_names.push_back(arg.second);
    }
    block.AddLine("SignalReceiver* signal_receiver) :");
    string param_list = chromeos::string_utils::Join(", ", param_names);
    block.PushOffset(kLineContinuationOffset);
    block.AddLine(StringPrintf("%s(%s) {", class_name.c_str(),
                               param_list.c_str()));
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
  }
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
  IndentedText block;
  block.AddLine("void ReleaseObjectProxy(const base::Closure& callback) {");
  block.PushOffset(kBlockOffset);
  block.AddLine(
      "bus_->RemoveObjectProxy(service_name_, object_path_, callback);");
  block.PopOffset();
  block.AddLine("}");
  block.AddBlankLine();
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
  block.AddBlankLine();
  text->AddBlock(block);
}

// static
void ProxyGenerator::AddSignalReceiver(const Interface& interface,
                                       IndentedText* text) {
  if (interface.signals.empty())
    return;

  IndentedText block;
  block.AddLine("class SignalReceiver {");
  block.AddLineWithOffset("public:", kScopeOffset);
  block.PushOffset(kBlockOffset);
  DbusSignature signature;
  for (const auto& signal : interface.signals) {
    block.AddComments(signal.doc_string);
    string signal_begin = StringPrintf(
        "virtual void %s(", GetHandlerNameForSignal(signal.name).c_str());
    string signal_end = ") {}";

    if (signal.arguments.empty()) {
      block.AddLine(signal_begin + signal_end);
      continue;
    }
    block.AddLine(signal_begin);
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
      if (!argument.name.empty()) {
        last_argument += ' ';
        last_argument += argument.name;
      }
    }
    block.AddLine(last_argument + signal_end);
    block.PopOffset();
  }
  block.PopOffset();
  block.AddLine("};");
  block.AddBlankLine();

  text->AddBlock(block);
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
    if (!IsIntegralType(argument_type)) {
      argument_type = StringPrintf("const %s&", argument_type.c_str());
    }
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
string ProxyGenerator::GetHandlerNameForSignal(const string& signal) {
  return StringPrintf("On%sSignal", signal.c_str());
}

}  // namespace chromeos_dbus_bindings
