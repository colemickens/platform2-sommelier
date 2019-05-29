// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/optional.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <base/values.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "diagnostics/constants/grpc_constants.h"
#include "diagnostics/telem/telemetry.h"
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

struct {
  const char* switch_name;
  diagnostics::TelemetryItemEnum telemetry_item;
} kTelemetryItemSwitches[] = {
    {"uptime", diagnostics::TelemetryItemEnum::kUptime},
    {"memtotal", diagnostics::TelemetryItemEnum::kMemTotalMebibytes},
    {"memfree", diagnostics::TelemetryItemEnum::kMemFreeMebibytes},
    {"runnable_entities", diagnostics::TelemetryItemEnum::kNumRunnableEntities},
    {"existing_entities", diagnostics::TelemetryItemEnum::kNumExistingEntities},
    {"idle_time_total", diagnostics::TelemetryItemEnum::kTotalIdleTimeUserHz},
    {"idle_time_per_cpu",
     diagnostics::TelemetryItemEnum::kIdleTimePerCPUUserHz},
    {"netstat", diagnostics::TelemetryItemEnum::kNetStat},
    {"netdev", diagnostics::TelemetryItemEnum::kNetDev},
    {"battery_cycle_count", diagnostics::TelemetryItemEnum::kBatteryCycleCount},
    {"battery_voltage", diagnostics::TelemetryItemEnum::kBatteryVoltage},
    {"battery_manufacturer",
     diagnostics::TelemetryItemEnum::kBatteryManufacturer},
};

struct {
  const char* switch_name;
  diagnostics::TelemetryGroupEnum telemetry_group;
} kTelemetryGroupSwitches[] = {
    {"memory", diagnostics::TelemetryGroupEnum::kMemory},
};

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
    LOG(ERROR) << "No telemetry data found for " << item_name << ".";
    return false;
  }

  std::cout << item_name << ": ";
  return DisplayTelemetryItem(telem_item.value());
}

// Create a stringified list of the item names for use in help.
std::string GetItemsAsString() {
  std::stringstream ss;
  ss << "[";
  const char* sep = "";
  for (auto telemetry_item_switch : kTelemetryItemSwitches) {
    ss << sep << telemetry_item_switch.switch_name;
    sep = ", ";
  }
  ss << "]";
  return ss.str();
}

std::string GetGroupsAsString() {
  std::stringstream ss;
  ss << "[";
  const char* sep = "";
  for (auto telemetry_group_switch : kTelemetryGroupSwitches) {
    ss << sep << telemetry_group_switch.switch_name;
    sep = ", ";
  }
  ss << "]";
  return ss.str();
}

}  // namespace

// 'telem' command-line tool:
//
// Test driver for libtelem. Only supports requesting individual telemetry
// items. This program does not exercise the caching functionality of libtelem.
int main(int argc, char** argv) {
  std::string item_help = base::StringPrintf("Telemetry item to display. %s",
                                             GetItemsAsString().c_str());
  std::string group_help = base::StringPrintf(
      "Group of telemetry items to display. %s", GetGroupsAsString().c_str());
  DEFINE_string(item, "", item_help.c_str());
  DEFINE_string(group, "", group_help.c_str());
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  std::map<std::string, diagnostics::TelemetryItemEnum> switch_to_item;
  std::map<diagnostics::TelemetryItemEnum, std::string> item_to_switch;
  for (const auto& item : kTelemetryItemSwitches) {
    switch_to_item[item.switch_name] = item.telemetry_item;
    item_to_switch[item.telemetry_item] = item.switch_name;
  }

  std::map<std::string, diagnostics::TelemetryGroupEnum> group_map;
  for (const auto& group : kTelemetryGroupSwitches) {
    group_map[group.switch_name] = group.telemetry_group;
  }

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  diagnostics::Telemetry telemetry;

  // Make sure at least one item or group is specified.
  if (FLAGS_item == "" && FLAGS_group == "") {
    LOG(ERROR) << "No item or group specified.";
    return EXIT_FAILURE;
  }
  // Validate the item flag.
  if (FLAGS_item != "") {
    if (!switch_to_item.count(FLAGS_item)) {
      LOG(ERROR) << "Invalid item: " << FLAGS_item;
      return EXIT_FAILURE;
    }

    // Retrieve and display the telemetry item.
    const base::Optional<base::Value> telem_item = telemetry.GetItem(
        switch_to_item.at(FLAGS_item), base::TimeDelta::FromSeconds(0));
    if (!DisplayOptionalTelemetryItem(FLAGS_item, telem_item))
      return EXIT_FAILURE;
  }
  // Validate the group flag.
  if (FLAGS_group != "") {
    if (!group_map.count(FLAGS_group)) {
      LOG(ERROR) << "Invalid group: " << FLAGS_group;
      return EXIT_FAILURE;
    }

    // Retrieve and display the telemetry group.
    const std::vector<
        std::pair<diagnostics::TelemetryItemEnum, base::Optional<base::Value>>>
        telem_items = telemetry.GetGroup(group_map.at(FLAGS_group),
                                         base::TimeDelta::FromSeconds(0));
    for (auto item_pair : telem_items) {
      if (!DisplayOptionalTelemetryItem(item_to_switch.at(item_pair.first),
                                        item_pair.second))
        return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
