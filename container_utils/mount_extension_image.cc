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
#include <session_manager/dbus-proxies.h>

namespace {

// Walk the user's extensions dir.  We need to find extension manifests and
// parse the name and version out.
const char kExtensionsDirectory[] = "Extensions";
const char kExtensionManifestName[] = "container.json";

// Chrome extension manifest keys.
const char kManifestNameField[] = "name";
const char kManifestVersionField[] = "version";

// Returns true if |container_dir| contains a container whose name matches
// |name|.  Additionally, if it finds an extension, |version| is populated
// with the extension version.
bool MatchContainer(const base::FilePath& container_dir,
                    const std::string& name,
                    std::string* version) {
  base::FilePath manifest_path = container_dir.Append(kExtensionManifestName);

  // Read manifest and check name.
  std::string manifest;
  if (base::ReadFileToString(manifest_path, &manifest))
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
base::FilePath FindExtensionDirectory(scoped_refptr<dbus::Bus> bus,
                                      const std::string& name,
                                      std::string* version) {
  std::map<std::string, std::string> sessions;
  brillo::ErrorPtr error;
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxy> proxy(
      new org::chromium::SessionManagerInterfaceProxy(bus));

  // Ask cryptohome for all the active user sessions.
  proxy->RetrieveActiveSessions(&sessions, &error);
  if (error) {
    LOG(ERROR) << "Error calling D-Bus proxy call to interface "
               << "'" << proxy->GetObjectPath().value() << "': "
               << error->GetMessage();
    return base::FilePath();
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
      if (MatchContainer(container_dir, name, version))
        return container_dir;
    }
  }

  return base::FilePath();
}

// D-Bus calls to imageloader.
std::string GetComponentVersion(dbus::ObjectProxy* proxy,
                                const std::string& name) {
  dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                               imageloader::kGetComponentVersion);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response)
    return "";

  std::string version;
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(&version))
    return "";

  return version;
}

bool RegisterComponent(dbus::ObjectProxy* proxy,
                       const std::string& name,
                       const std::string& version,
                       const std::string& component_dir) {
  dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                               imageloader::kRegisterComponent);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  writer.AppendString(version);
  writer.AppendString(component_dir);
  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response)
    return false;

  bool succeeded;
  dbus::MessageReader reader(response.get());
  if (!reader.PopBool(&succeeded))
    return false;

  return succeeded;
}

std::string LoadComponent(dbus::ObjectProxy* proxy, const std::string& name) {
  dbus::MethodCall method_call(imageloader::kImageLoaderServiceInterface,
                               imageloader::kLoadComponent);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response)
    return "";

  std::string mount_dir;
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(&mount_dir))
    return "";

  return mount_dir;
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
  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      imageloader::kImageLoaderServiceName,
      dbus::ObjectPath(imageloader::kImageLoaderServicePath));
  if (!proxy)
    return base::FilePath();

  // If imageloader has this version already, we can skip registration
  // and just ask it to load the component.
  std::string current_version = GetComponentVersion(proxy, name);
  if (current_version != version) {
    // This temporary directory ensures imageloader can see the
    // image files.
    base::FilePath temp_dir;
    if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
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

    if (!RegisterComponent(proxy, name, version, temp_dir.value())) {
      LOG(ERROR) << "Registering component failed";
      return base::FilePath();
    }
  }

  return base::FilePath(LoadComponent(proxy, name));
}

}  // namespace

int main(int argc, char **argv) {
  DEFINE_string(name, "", "Name of extension which packages the container");
  brillo::FlagHelper::Init(argc, argv,
                           "Mounts a container image out of an extension.");
  brillo::InitLog(brillo::kLogToSyslog);

  if (FLAGS_name.empty()) {
    LOG(ERROR) << "Nothing to mount";
    return 1;
  }

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  std::string extension_version;
  base::FilePath extension_dir =
      FindExtensionDirectory(bus, FLAGS_name, &extension_version);
  if (extension_dir.empty()) {
    LOG(ERROR) << "Could not find extension named \""
               << FLAGS_name << "\"";
    return 1;
  }

  DLOG(INFO) << "Found " << FLAGS_name << " " << extension_version;
  base::FilePath mount_dir = MountImage(bus,
                                        FLAGS_name,
                                        extension_version,
                                        extension_dir);
  if (mount_dir.empty()) {
    LOG(ERROR) << "Could not mount image from \""
               << extension_dir.value() << "\"";
    return 1;
  }

  printf("%s\n", mount_dir.value().c_str());
  return 0;
}
