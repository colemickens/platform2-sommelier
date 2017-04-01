// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>
#include <sstream>
#include <string>
#include <sys/mount.h>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>

#include <libcontainer/libcontainer.h>

#include "run_oci/container_config_parser.h"
#include "run_oci/container_options.h"

namespace {

using run_oci::BindMount;
using run_oci::BindMounts;
using run_oci::ContainerOptions;
using run_oci::OciConfigPtr;

using ContainerConfigPtr = std::unique_ptr<container_config,
                                           decltype(&container_config_destroy)>;

// Converts a single UID map to a string.
std::string GetIdMapString(const OciLinuxNamespaceMapping& map) {
  std::ostringstream oss;
  oss << map.containerID << " " << map.hostID << " " << map.size;
  return oss.str();
}

// Converts an array of UID mappings given in |maps| to the string format the
// kernel understands and puts that string in |map_string_out|.
std::string IdStringFromMap(const std::vector<OciLinuxNamespaceMapping>& maps) {
  std::ostringstream oss;
  bool first = true;
  for (const auto& map : maps) {
    if (first)
      first = false;
    else
      oss << ",";
    oss << GetIdMapString(map);
  }
  return oss.str();
}

// Parses the options from the OCI mount in to either mount flags in |flags_out|
// or a data string for mount(2) in |option_string_out|.
std::string ParseMountOptions(const std::vector<std::string>& options,
                              int* flags_out, int* loopback_out,
                              std::string *verity_options) {
  std::string option_string_out;
  *flags_out = 0;
  *loopback_out = 0;

  for (const auto& option : options) {
    if (option == "nodev") {
      *flags_out |= MS_NODEV;
    } else if (option == "noexec") {
      *flags_out |= MS_NOEXEC;
    } else if (option == "nosuid") {
      *flags_out |= MS_NOSUID;
    } else if (option == "bind") {
      *flags_out |= MS_BIND;
    } else if (option == "ro") {
      *flags_out |= MS_RDONLY;
    } else if (option == "private") {
      *flags_out |= MS_PRIVATE;
    } else if (option == "recursive") {
      *flags_out |= MS_REC;
    } else if (option == "slave") {
      *flags_out |= MS_SLAVE;
    } else if (option == "remount") {
      *flags_out |= MS_REMOUNT;
    } else if (option == "loop") {
      *loopback_out = 1;
    } else if (base::StartsWith(option, "dm=", base::CompareCase::SENSITIVE)) {
      *verity_options = option.substr(3, std::string::npos);
    } else {
      // Unknown options get appended to the string passed to mount data.
      if (!option_string_out.empty())
        option_string_out += ",";
      option_string_out += option;
    }
  }

  return option_string_out;
}

// Sanitize |flags| that can be used for filesystem of a given |type|.
int SanitizeFlags(const std::string &type, int flags) {
    int sanitized_flags = flags;
    // Right now, only sanitize sysfs and procfs.
    if (type != "sysfs" && type != "proc")
        return flags;

    // sysfs and proc should always have nodev, noexec, nosuid.
    // Warn the user if these weren't specified, then turn them on.
    sanitized_flags |= (MS_NODEV | MS_NOEXEC | MS_NOSUID);
    if (flags ^ sanitized_flags)
        LOG(WARNING) << "Sanitized mount of type " << type << ".";

    return sanitized_flags;
}

// Adds the mounts specified in |mounts| to |config_out|.
void ConfigureMounts(const std::vector<OciMount>& mounts,
                     uid_t uid, gid_t gid,
                     container_config* config_out) {
  for (const auto& mount : mounts) {
    int flags, loopback;
    std::string verity_options;
    std::string options = ParseMountOptions(mount.options, &flags, &loopback,
                                            &verity_options);
    flags = SanitizeFlags(mount.type, flags);

    container_config_add_mount(config_out,
                               "mount",
                               mount.source.c_str(),
                               mount.destination.c_str(),
                               mount.type.c_str(),
                               options.empty() ?
                                   NULL : options.c_str(),
                               verity_options.empty() ?
                                   NULL : verity_options.c_str(),
                               flags,
                               uid,
                               gid,
                               0750,
                               // Loopback devices have to be mounted outside.
                               !loopback,
                               1,
                               loopback);
  }
}

// Adds the devices specified in |devices| to |config_out|.
void ConfigureDevices(const std::vector<OciLinuxDevice>& devices,
                      container_config* config_out) {
  for (const auto& device : devices) {
    container_config_add_device(config_out,
                                device.type.c_str()[0],
                                device.path.c_str(),
                                device.fileMode,
                                device.major,
                                device.minor,
                                0,
                                device.uid,
                                device.gid,
                                0,  // Cgroup permission are now in 'resources'.
                                0,
                                0);
  }
}

// Adds the cgroup device permissions specified in |devices| to |config_out|.
void ConfigureCgroupDevices(const std::vector<OciLinuxCgroupDevice>& devices,
                            container_config* config_out) {
  for (const auto& device : devices) {
    bool read_set = device.access.find('r') != std::string::npos;
    bool write_set = device.access.find('w') != std::string::npos;
    bool make_set = device.access.find('m') != std::string::npos;
    container_config_add_cgroup_device(config_out,
                                       device.allow,
                                       device.type.c_str()[0],
                                       device.major,
                                       device.minor,
                                       read_set,
                                       write_set,
                                       make_set);
  }
}

// Fills the libcontainer container_config struct given in |config_out| by
// pulling the apropriate fields from the OCI configuration given in |oci|.
bool ContainerConfigFromOci(const OciConfig& oci,
                            const base::FilePath& container_root,
                            const std::vector<std::string>& extra_args,
                            container_config* config_out) {
  // Process configuration
  container_config_config_root(config_out, container_root.value().c_str());
  container_config_uid(config_out, oci.process.user.uid);
  container_config_gid(config_out, oci.process.user.gid);
  base::FilePath root_dir = container_root.Append(oci.root.path);
  container_config_premounted_runfs(config_out, root_dir.value().c_str());

  std::vector<const char *> argv;
  for (const auto& arg : oci.process.args)
    argv.push_back(arg.c_str());
  for (const auto& arg : extra_args)
    argv.push_back(arg.c_str());
  container_config_program_argv(config_out, argv.data(), argv.size());

  std::string uid_maps = IdStringFromMap(oci.linux_config.uidMappings);
  container_config_uid_map(config_out, uid_maps.c_str());
  std::string gid_maps = IdStringFromMap(oci.linux_config.gidMappings);
  container_config_gid_map(config_out, gid_maps.c_str());

  ConfigureMounts(oci.mounts, oci.process.user.uid,
                  oci.process.user.gid, config_out);
  ConfigureDevices(oci.linux_config.devices, config_out);
  ConfigureCgroupDevices(oci.linux_config.resources.devices, config_out);

  return true;
}

// Reads json configuration of a container from |config_path| and filles
// |oci_out| with the specified container configuration.
bool OciConfigFromFile(const base::FilePath& config_path,
                       const OciConfigPtr& oci_out) {
  std::string config_json_data;
  if (!base::ReadFileToString(config_path, &config_json_data)) {
    PLOG(ERROR) << "Fail to config container.";
    return false;
  }

  if (!run_oci::ParseContainerConfig(config_json_data, oci_out)) {
    LOG(ERROR) << "Fail to parse config.json.";
    return false;
  }

  return true;
}

// Appends additional mounts specified in |bind_mounts| to the configuration
// given in |config_out|.
bool AppendMounts(const BindMounts& bind_mounts, container_config* config_out) {
  for (auto & mount : bind_mounts) {
    if (container_config_add_mount(config_out,
                                   "mount",
                                   mount.first.value().c_str(),
                                   mount.second.value().c_str(),
                                   "bind",
                                   NULL,
                                   NULL,
                                   MS_MGC_VAL | MS_BIND,
                                   0,
                                   0,
                                   0750,
                                   1,
                                   1,
                                   0)) {
      PLOG(ERROR) << "Failed to add mount of " << mount.first.value();
      return false;
    }
  }

  return true;
}

// Runs an OCI image that is mounted at |container_dir|.  Blocks until the
// program specified in config.json exits.  Returns -1 on error.
int RunOci(const base::FilePath& container_dir,
           const ContainerOptions& container_options) {
  base::ScopedTempDir temp_dir;
  base::FilePath container_config_file = container_dir.Append("config.json");

  OciConfigPtr oci_config(new OciConfig());
  if (!OciConfigFromFile(container_config_file, oci_config)) {
    return -1;
  }

  ContainerConfigPtr config(container_config_create(),
                            &container_config_destroy);
  if (!ContainerConfigFromOci(*oci_config,
                              container_dir,
                              container_options.extra_program_args,
                              config.get())) {
    PLOG(ERROR) << "Failed to create container from oci config.";
    return -1;
  }

  AppendMounts(container_options.bind_mounts, config.get());
  // Create a container based on the config.  The run_dir argument will be
  // unused as this container will be run in place where it was mounted.
  std::unique_ptr<container, decltype(&container_destroy)>
      container(container_new(oci_config->hostname.c_str(), "/unused"),
                &container_destroy);

  container_config_keep_fds_open(config.get());

  if (container_options.cgroup_parent.length() > 0) {
    container_config_set_cgroup_parent(config.get(),
                                       container_options.cgroup_parent.c_str(),
                                       container_config_get_uid(config.get()),
                                       container_config_get_gid(config.get()));
  }

  if (container_options.use_current_user) {
    OciLinuxNamespaceMapping single_map = {
      getuid(),  // hostID
      0,         // containerID
      1          // size
    };
    std::string map_string = GetIdMapString(single_map);
    container_config_uid_map(config.get(), map_string.c_str());
    container_config_gid_map(config.get(), map_string.c_str());
  }

  if (!container_options.alt_syscall_table.empty()) {
    container_config_alt_syscall_table(
        config.get(), container_options.alt_syscall_table.c_str());
  }

  int rc;
  rc = container_start(container.get(), config.get());
  if (rc) {
    PLOG(ERROR) << "start failed.";
    return -1;
  }

  return container_wait(container.get());
}

const struct option longopts[] = {
  { "bind_mount", required_argument, NULL, 'b' },
  { "help", no_argument, NULL, 'h' },
  { "cgroup_parent", required_argument, NULL, 'p' },
  { "alt_syscall", required_argument, NULL, 's' },
  { "use_current_user", no_argument, NULL, 'u' },
  { 0, 0, 0, 0 },
};

void print_help(const char *argv0) {
  printf("usage: %s [OPTIONS] <container path> -- [Command Args]\n", argv0);
  printf("  -b, --bind_mount=<A>:<B>       Mount path A to B container.\n");
  printf("  -h, --help                     Print this message and exit.\n");
  printf("  -p, --cgroup_parent=<NAME>     Set parent cgroup for container.\n");
  printf("  -s, --alt_syscall=<NAME>       Set the alt-syscall table.\n");
  printf("  -u, --use_current_user         Map the current user/group only.\n");
  printf("\n");
}

}  // anonymous namespace

int main(int argc, char **argv) {
  ContainerOptions container_options;
  int c;

  while ((c = getopt_long(argc, argv, "b:hp:s:uU", longopts, NULL)) != -1) {
    switch (c) {
    case 'b': {
      std::istringstream ss(optarg);
      std::string outside_path;
      std::string inside_path;
      std::getline(ss, outside_path, ':');
      std::getline(ss, inside_path, ':');
      if (outside_path.empty() || inside_path.empty()) {
        print_help(argv[0]);
        return -1;
      }
      container_options.bind_mounts.push_back(
          BindMount(base::FilePath(outside_path), base::FilePath(inside_path)));
      break;
    }
    case 'u':
      container_options.use_current_user = true;
      break;
    case 'p':
      container_options.cgroup_parent = optarg;
      break;
    case 's':
      container_options.alt_syscall_table = optarg;
      break;
    case 'h':
      print_help(argv[0]);
      return 0;
    default:
      print_help(argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    LOG(ERROR) << "Container path is required.";
    print_help(argv[0]);
    return -1;
  }

  int path_arg_index = optind;
  for (optind++; optind < argc; optind++)
    container_options.extra_program_args.push_back(std::string(argv[optind]));

  return RunOci(base::FilePath(argv[path_arg_index]), container_options);
}
