// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Look up model / submodel from information provided

#include "chromeos-config/libcros_config/cros_config.h"

extern "C" {
#include <libfdt.h>
};

#include <string>

#include <base/logging.h>

namespace brillo {

int CrosConfig::FindIDsInMap(int node, const std::string& find_name,
                             int find_sku_id, std::string* platform_name_out) {
  const void* blob = blob_.c_str();
  VLOG(1) << "Trying " << fdt_get_name(blob, node, NULL);
  const char *smbios_name = static_cast<const char *>(
      fdt_getprop(blob, node, "smbios-name-match", NULL));
  if (smbios_name &&
      (find_name.empty() || strcmp(smbios_name, find_name.c_str()))) {
    LOG(ERROR) << "SMBIOS name " << smbios_name << " does not match "
               << find_name;
    return 0;
  }

  // If we have a single SKU, deal with that first
  int len = 0;
  const fdt32_t *data = (const fdt32_t *)fdt_getprop(blob, node, "single-sku",
                                                     &len);
  int found_phandle = 0;
  if (data) {
    if (len != sizeof(fdt32_t)) {
      LOG(ERROR) << "single-sku: Invalid length " << len;
      return -1;
    }
    found_phandle = fdt32_to_cpu(*data);
    LOG(INFO) << "Single SKU match";
  } else {
    // Locate the map and make sure it is a multiple of 2 cells (first is SKU
    // ID, second is phandle).
    const fdt32_t *data, *end, *ptr;
    data = static_cast<const fdt32_t *>(
        fdt_getprop(blob, node, "simple-sku-map", &len));
    if (!data) {
      LOG(ERROR) << "Cannot find simple-sku-map: " << fdt_strerror(node);
      return -1;
    }
    if (len % (sizeof(fdt32_t) * 2)) {
      // Validation of configuration should catch this, so this should never
      // happen. But we don't want to crash.
      LOG(ERROR) << "single-sku-map: " << fdt_get_name(blob, node, NULL)
                 << " invalid length " << len;
      return -1;
    }

    // Search for the SKU ID in the list.
    for (end = data + len / sizeof(fdt32_t), ptr = data; ptr < end; ptr += 2) {
      int sku_id = fdt32_to_cpu(ptr[0]);
      int phandle = fdt32_to_cpu(ptr[1]);

      if (sku_id == find_sku_id) {
        found_phandle = phandle;
        break;
      }
    }
    if (!found_phandle) {
      VLOG(1) << "SKU ID " << find_sku_id << " not found in mapping";
      return 0;
    }
    LOG(INFO) << "Simple SKU map match ";
  }

  const char *pname = static_cast<const char *>(
      fdt_getprop(blob, node, "platform-name", NULL));
  if (pname)
    *platform_name_out = pname;
  else
    *platform_name_out = "unknown";
  LOG(INFO) << "Platform name " << *platform_name_out;

  return found_phandle;
}

int CrosConfig::FindIDsInAllMaps(int mapping_node, const std::string& find_name,
                                 int find_sku_id,
                                 std::string* platform_name_out) {
  const void* blob = blob_.c_str();
  int subnode;;

  fdt_for_each_subnode(subnode, blob, mapping_node) {
    int phandle = FindIDsInMap(subnode, find_name, find_sku_id,
                               platform_name_out);
    if (phandle < 0) {
      return -1;
    } else if (phandle > 0) {
      return phandle;
    }
  }
  return 0;
}

int CrosConfig::FollowPhandle(int phandle, int* target_out) {
  const void* blob = blob_.c_str();

  // Follow the phandle to the target
  int node = fdt_node_offset_by_phandle(blob, phandle);
  if (node < 0) {
    LOG(ERROR) << "Cannot find phandle for sku ID: " << fdt_strerror(node);
    return -1;
  }

  // Figure out whether the target is a model or a sub-model
  int parent = fdt_parent_offset(blob, node);
  if (parent < 0) {
    LOG(ERROR) << "Cannot find parent of phandle target: "
                << fdt_strerror(node);
    return -1;
  }
  const char* parent_name = fdt_get_name(blob, parent, NULL);
  int model_node;
  if (!strcmp(parent_name, "submodels")) {
    model_node = fdt_parent_offset(blob, parent);
    if (model_node < 0) {
      LOG(ERROR) << "Cannot find sub-model parent: " << fdt_strerror(node);
      return -1;
    }
  } else if (!strcmp(parent_name, "models")) {
    model_node = node;
  } else {
    LOG(ERROR) << "Phandle target parent " << parent_name << " is invalid";
    return -1;
  }
  *target_out = node;

  return model_node;
}

bool CrosConfig::SelectModelConfigByIDs(const std::string &find_name,
    int find_sku_id, std::string& find_whitelabel_name) {
  const void* blob = blob_.c_str();
  LOG(INFO) << "Looking up name " << find_name << ", SKU ID " << find_sku_id;

  int mapping_node = fdt_path_offset(blob, "/chromeos/family/mapping");
  if (mapping_node < 0) {
    LOG(ERROR) << "Cannot find mapping node: " << fdt_strerror(mapping_node);
    return false;
  }

  std::string platform_name;
  int phandle = FindIDsInAllMaps(mapping_node, find_name, find_sku_id,
                                 &platform_name);
  if (phandle <= 0) {
    return false;
  }
  int target;
  int model_offset = FollowPhandle(phandle, &target);
  if (model_offset < 0) {
    return false;
  }

  //  We found the model node, so set up the data
  platform_name_ = platform_name;
  model_offset_ = model_offset;
  model_name_ = fdt_get_name(blob, model_offset_, NULL);
  if (target != model_offset_) {
    submodel_offset_ = target;
    submodel_name_ = fdt_get_name(blob, submodel_offset_, NULL);
  } else {
    submodel_offset_ = -1;
    submodel_name_ = "";
  }

  // If this is a whitelabel model, use the tag.
  int firmware_node = fdt_subnode_offset(blob, model_offset_, "firmware");
  if (firmware_node >= 0) {
    if (fdt_getprop(blob, firmware_node, "sig-id-in-customization-id", NULL)) {
      int models_node = fdt_path_offset(blob, "/chromeos/models");
      int wl_model = fdt_subnode_offset(blob, models_node,
                                        find_whitelabel_name.c_str());
      if (wl_model >= 0) {
        whitelabel_offset_ = model_offset_;
        model_offset_ = wl_model;
      } else {
        LOG(ERROR) << "Cannot find whitelabel model "
                   << find_whitelabel_name << ": using " << model_name_
                   << ": " << fdt_strerror(wl_model);
      }
    }
  }
  int wl_tags_node = fdt_subnode_offset(blob, model_offset_, "whitelabels");
  if (wl_tags_node >= 0) {
    int wl_tag = fdt_subnode_offset(blob, wl_tags_node,
                                    find_whitelabel_name.c_str());
    if (wl_tag >= 0) {
      whitelabel_tag_offset_ = wl_tag;
    } else {
        LOG(ERROR) << "Cannot find whitelabel tag "
                   << find_whitelabel_name << ": using " << model_name_
                   << ": " << fdt_strerror(wl_tag);
    }
  }

  return true;
}

}  // namespace brillo
