#include "lib/cros_config_struct.h"

static struct config_map all_configs[] = {
    {.platform_name = "",
     .device_tree_compatible_match = "google,some",
     .customization_id = "",
     .whitelabel_tag = "",
     .info = {.brand = "",
              .model = "some",
              .customization = "some",
              .signature_id = "some"}},

    {.platform_name = "",
     .device_tree_compatible_match = "google,whitelabel",
     .customization_id = "",
     .whitelabel_tag = "whitelabel1",
     .info = {.brand = "",
              .model = "whitelabel",
              .customization = "whitelabel1",
              .signature_id = "whitelabel"}},

    {.platform_name = "",
     .device_tree_compatible_match = "google,whitelabel",
     .customization_id = "",
     .whitelabel_tag = "whitelabel2",
     .info = {.brand = "",
              .model = "whitelabel",
              .customization = "whitelabel2",
              .signature_id = "whitelabel"}},

    {.platform_name = "",
     .device_tree_compatible_match = "google,whitelabel",
     .customization_id = "",
     .whitelabel_tag = "",
     .info = {.brand = "",
              .model = "whitelabel",
              .customization = "whitelabel",
              .signature_id = "whitelabel"}}
};

const struct config_map *cros_config_get_config_map(int *num_entries) {
  *num_entries = 4;
  return &all_configs[0];
}
