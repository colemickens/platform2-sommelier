// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/optional.h>
#include <base/time/time.h>
#include <base/values.h>
#include <brillo/flag_helper.h>

#include "diagnostics/telem/telem_connection.h"
#include "diagnostics/telem/telemetry_group_enum.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace {

void DisplayStringItem(const base::Value& string_item);
void DisplayIntItem(const base::Value& int_item);
void DisplayListItem(const base::Value& list_item);
bool DisplayTelemetryItem(const base::Value& telem_item);
bool DisplayOptionalTelemetryItem(
    const std::string& item_name,
    const base::Optional<base::Value>& telem_item);

// URI on which the gRPC interface exposed by the diagnosticsd daemon is
// listening. This is the same URI that the diagnosticsd daemon uses to
// communicate with the diagnostics_processor, so this tool should not
// be used when diagnostics_processor is running.
constexpr char kDiagnosticsdGrpcUri[] =
    "unix:/run/diagnostics/grpc_sockets/diagnosticsd_socket";

// Helper function to display a base::Value object which has a string
// representation.
void DisplayStringItem(const base::Value& string_item) {
  DCHECK(string_item.is_string());

  std::string val;
  string_item.GetAsString(&val);
  std::cout << val << std::endl;
}

// Helper function to display a base::Value object which has an integer
// representation.
void DisplayIntItem(const base::Value& int_item) {
  DCHECK(int_item.is_int());

  int val;
  int_item.GetAsInteger(&val);
  std::cout << val << std::endl;
}

// Helper function to display a base::Value object which has a list
// representation.
void DisplayListItem(const base::Value& list_item) {
  DCHECK(list_item.is_list());

  const base::ListValue* list;
  list_item.GetAsList(&list);
  const base::Value* item;

  // Print a newline so that the first list value starts on a new line.
  std::cout << std::endl;

  // Print each of the list values.
  for (int i = 0; i < list->GetSize(); i++) {
    list->Get(i, &item);
    DisplayTelemetryItem(*item);
  }
}

// Helper function to display a base::Value object which has an arbitrary
// representation.
bool DisplayTelemetryItem(const base::Value& telem_item) {
  if (telem_item.is_int()) {
    DisplayIntItem(telem_item);
  } else if (telem_item.is_string()) {
    DisplayStringItem(telem_item);
  } else if (telem_item.is_list()) {
    DisplayListItem(telem_item);
  } else {
    LOG(ERROR) << "Invalid format for telemetry item.";
    return false;
  }

  return true;
}

// Displays the telemetry item to the console. Returns
// true iff the item was successfully displayed.
bool DisplayOptionalTelemetryItem(
    const std::string& item_name,
    const base::Optional<base::Value>& telem_item) {
  if (!telem_item) {
    LOG(ERROR) << "No telemetry item received.";
    return false;
  }

  std::cout << item_name << ": ";
  return DisplayTelemetryItem(telem_item.value());
}

}  // namespace

// 'telem' command-line tool:
//
// Test driver for libtelem. Only supports requesting individual telemetry
// items. This program does not exercise the caching functionality of libtelem.
int main(int argc, char** argv) {
  DEFINE_string(item, "", "Telemetry item to retrieve.");
  DEFINE_string(group, "", "Group of telemetry items to retrieve.");
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");

  // Mapping between --item arguments and TelemetryItemEnums.
  const std::map<std::string, diagnostics::TelemetryItemEnum> kItemMap = {
      {"uptime", diagnostics::TelemetryItemEnum::kUptime},
      {"memtotal", diagnostics::TelemetryItemEnum::kMemTotalMebibytes},
      {"memfree", diagnostics::TelemetryItemEnum::kMemFreeMebibytes},
      {"runnable_entities",
       diagnostics::TelemetryItemEnum::kNumRunnableEntities},
      {"existing_entities",
       diagnostics::TelemetryItemEnum::kNumExistingEntities},
      {"idle_time_total", diagnostics::TelemetryItemEnum::kTotalIdleTimeUserHz},
      {"idle_time_per_cpu",
       diagnostics::TelemetryItemEnum::kIdleTimePerCPUUserHz},
      {"button", diagnostics::TelemetryItemEnum::kAcpiButton},
      {"netstat", diagnostics::TelemetryItemEnum::kNetStat},
      {"netdev", diagnostics::TelemetryItemEnum::kNetDev}};

  // Reversed kItemMap, to allow quick lookup of names for pretty printing.
  const std::map<diagnostics::TelemetryItemEnum, std::string> kNameLookup = {
      {diagnostics::TelemetryItemEnum::kUptime, "uptime"},
      {diagnostics::TelemetryItemEnum::kMemTotalMebibytes, "memtotal"},
      {diagnostics::TelemetryItemEnum::kMemFreeMebibytes, "memfree"},
      {diagnostics::TelemetryItemEnum::kNumRunnableEntities,
       "runnable_entities"},
      {diagnostics::TelemetryItemEnum::kNumExistingEntities,
       "existing_entities"},
      {diagnostics::TelemetryItemEnum::kTotalIdleTimeUserHz, "idle_time_total"},
      {diagnostics::TelemetryItemEnum::kIdleTimePerCPUUserHz,
       "idle_time_per_cpu"},
      {diagnostics::TelemetryItemEnum::kAcpiButton, "button"},
      {diagnostics::TelemetryItemEnum::kNetStat, "netstat"},
      {diagnostics::TelemetryItemEnum::kNetDev, "netdev"}};

  // Mapping between --group arguments and TelemetryGroupEnums.
  const std::map<std::string, diagnostics::TelemetryGroupEnum> kGroupMap = {
      {"disk", diagnostics::TelemetryGroupEnum::kDisk}};

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  diagnostics::TelemConnection telem_connection;

  telem_connection.Connect(kDiagnosticsdGrpcUri);

  // Make sure at least one item or group is specified.
  if (FLAGS_item == "" && FLAGS_group == "") {
    LOG(ERROR) << "No item or group specified.";
    return EXIT_FAILURE;
  }
  // Validate the item flag.
  if (FLAGS_item != "") {
    if (!kItemMap.count(FLAGS_item)) {
      LOG(ERROR) << "Invalid item: " << FLAGS_item;
      return EXIT_FAILURE;
    }

    // Retrieve and display the telemetry item.
    const base::Optional<base::Value> telem_item = telem_connection.GetItem(
        kItemMap.at(FLAGS_item), base::TimeDelta::FromSeconds(0));
    if (!DisplayOptionalTelemetryItem(FLAGS_item, telem_item))
      return EXIT_FAILURE;
  }
  // Validate the group flag.
  if (FLAGS_group != "") {
    if (!kGroupMap.count(FLAGS_group)) {
      LOG(ERROR) << "Invalid group: " << FLAGS_group;
      return EXIT_FAILURE;
    }

    // Retrieve and display the telemetry group.
    const std::vector<std::pair<diagnostics::TelemetryItemEnum,
                                const base::Optional<base::Value>>>
        telem_items = telem_connection.GetGroup(
            kGroupMap.at(FLAGS_group), base::TimeDelta::FromSeconds(0));
    for (auto item_pair : telem_items) {
      if (!DisplayOptionalTelemetryItem(kNameLookup.at(item_pair.first),
                                        item_pair.second))
        return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
