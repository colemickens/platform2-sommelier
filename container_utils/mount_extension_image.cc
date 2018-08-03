// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <sys/stat.h>

#include <string>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/json/json_string_value_serializer.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <brillo/cryptohome.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <imageloader/dbus-proxies.h>
#include <session_manager/dbus-proxies.h>

namespace {

// Walk the user's extensions dir.  We need to find extension manifests and
// parse the name and version out.
const char kExtensionsDirectory[] = "Extensions";
const char kExtensionManifestName[] = "container.json";

// Chrome extension manifest keys.
const char kManifestNameField[] = "name";
const char kManifestVersionField[] = "version";

// When we find a matching container, we want to know where it's located
// and what the version string is so imageloader can load it.
struct ContainerInfo {
  base::FilePath container_dir;
  std::string version;
};

// Returns true if |container_dir| contains a container whose name matches
// |name|.  Additionally, if it finds an extension, |version| is populated
// with the extension version.
bool MatchContainer(const base::FilePath& container_dir,
                    const std::string& name,
                    std::string* version) {
  base::FilePath manifest_path = container_dir.Append(kExtensionManifestName);

  // Read manifest and check name.
  std::string manifest;
  if (!base::ReadFileToString(manifest_path, &manifest))
    return false;

  JSONStringValueDeserializer deserializer(manifest);

  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (!value) {
    DLOG(WARNING) << "Failed to deserialize manifest \""
                  << manifest_path.value() << "\". Error "
                  << error_code << ": " << error_message;
    return false;
  }

  base::DictionaryValue* manifest_dict;
  if (!value->GetAsDictionary(&manifest_dict)) {
    DLOG(WARNING) << "Manifest \"" << manifest_path.value()
                  << "\" is not a JSON dictionary";
    return false;
  }

  std::string extension_name;
  if (!manifest_dict->GetString(kManifestNameField, &extension_name)) {
    DLOG(WARNING) << "Manifest \"" << manifest_path.value()
                  << "\" is malformed; no extension name specified";
    return false;
  }

  if (extension_name != name)
    return false;

  std::string extension_version;
  if (!manifest_dict->GetString(kManifestVersionField, &extension_version)) {
    DLOG(WARNING) << "Manifest \"" << manifest_path.value()
                  << "\" is malformed; no extension version specified";
    return false;
  }

  *version = extension_version;
  return true;
}

// Searches all mounted user directories for an extension named |name| and
// returns the path to its extension directory. Additionally populates
// |version| with the extension version if found.
std::vector<ContainerInfo> FindExtensionDirectory(scoped_refptr<dbus::Bus> bus,
                                                  const std::string& name) {
  std::map<std::string, std::string> sessions;
  std::vector<ContainerInfo> container_infos;
  brillo::ErrorPtr error;
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxy> proxy(
      new org::chromium::SessionManagerInterfaceProxy(bus));

  // Ask cryptohome for all the active user sessions.
  proxy->RetrieveActiveSessions(&sessions, &error);
  if (error) {
    LOG(ERROR) << "Error calling D-Bus proxy call to interface "
               << "'" << proxy->GetObjectPath().value() << "': "
               << error->GetMessage();
    return std::vector<ContainerInfo>();
  }

  // Walk all active sessions and poke their Extensions dir for containers.
  base::FilePath extensions_dir;
  for (auto session : sessions) {
    DVLOG(1) << "Searching user directory " << session.second;
    extensions_dir = brillo::cryptohome::home::GetHashedUserPath(
        session.second).Append(kExtensionsDirectory);

    // Scan all the directories to see any of them are containers.
    base::FileEnumerator fe(extensions_dir,
                            /* recursive = */ true,
                            base::FileEnumerator::FileType::DIRECTORIES);
    for (base::FilePath container_dir = fe.Next();
         !container_dir.empty();
         container_dir = fe.Next()) {
      std::string version;
      if (MatchContainer(container_dir, name, &version))
        container_infos.push_back(ContainerInfo{container_dir, version});
    }
  }

  return container_infos;
}

bool CopyImageDirectory(const base::FilePath& from_dir,
                        const base::FilePath& to_dir) {
  if (!base::SetPosixFilePermissions(to_dir, 0755))
    return false;

  base::FileEnumerator fe(from_dir,
                          /* recursive = */ true,
                          (base::FileEnumerator::FileType::FILES |
                           base::FileEnumerator::FileType::DIRECTORIES));
  for (base::FilePath path = fe.Next(); !path.empty(); path = fe.Next()) {
    if (from_dir == path)
      continue;

    base::FilePath to_file = to_dir;
    if (!from_dir.AppendRelativePath(path, &to_file))
      return false;
    if (!base::CopyFile(path, to_file))
      return false;

    DVLOG(1) << "Changing permissions on " << to_file.value();
    mode_t mode = fe.GetInfo().IsDirectory() ? 0755 : 0644;
    if (!base::SetPosixFilePermissions(to_file, mode))
      return false;
  }
  return true;
}

base::FilePath MountImage(scoped_refptr<dbus::Bus> bus,
                          const std::string& name,
                          const std::string& version,
                          const base::FilePath& component_dir) {
  org::chromium::ImageLoaderInterfaceProxy proxy(bus);

  // If imageloader has this version already, we can skip registration
  // and just ask it to load the component.
  std::string current_version;
  if (!proxy.GetComponentVersion(name, &current_version, nullptr) ||
      current_version != version) {
    // This temporary directory ensures imageloader can see the
    // image files.
    base::FilePath temp_dir;
    if (!base::CreateNewTempDirectory(std::string(),
                                      &temp_dir)) {
      LOG(ERROR) << "Failed to create temp dir";
      return base::FilePath();
    }
    // Ensure cleanup no matter how we fail here.
    base::ScopedTempDir scoped_temp_dir;
    CHECK(scoped_temp_dir.Set(temp_dir));

    if (!CopyImageDirectory(component_dir, temp_dir)) {
      LOG(ERROR) << "Failed to copy image into temp dir";
      return base::FilePath();
    }

    bool success;
    if (!proxy.RegisterComponent(
        name, version, temp_dir.value(), &success, nullptr) || !success) {
      LOG(ERROR) << "Registering component failed";
      return base::FilePath();
    }
  }

  std::string mount_point;
  if (!proxy.LoadComponent(name, &mount_point, nullptr))
    return base::FilePath();

  return base::FilePath(mount_point);
}

}  // namespace

int main(int argc, char **argv) {
  DEFINE_string(name, "", "Name of container");
  brillo::FlagHelper::Init(argc, argv,
                           "Mounts a container image out of an extension.");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_name.empty()) {
    LOG(ERROR) << "Nothing to mount";
    return 1;
  }

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  std::vector<ContainerInfo> container_infos =
      FindExtensionDirectory(bus, FLAGS_name);
  if (container_infos.empty()) {
    LOG(ERROR) << "Could not find container named \"" << FLAGS_name << "\"";
    return 1;
  }

  for (const auto& info : container_infos) {
    DLOG(INFO) << "Found " << FLAGS_name << " " << info.version;
    base::FilePath mount_dir = MountImage(bus,
                                          FLAGS_name,
                                          info.version,
                                          info.container_dir);
    if (!mount_dir.empty()) {
      printf("%s\n", mount_dir.value().c_str());
      return 0;
    }

    LOG(ERROR) << "Could not mount container image from \""
               << info.container_dir.value() << "\"";
  }

  LOG(ERROR) << "Could not mount any containers";
  return 1;
}
