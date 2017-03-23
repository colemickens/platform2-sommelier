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
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace {

// The user's extensions will be installed in:
// /home/chronos/u-*/Extensions
// We need to find extension manifests and parse the name and version out.
const char kChronosDirectory[] = "/home/chronos";
const char kUserDirectoryPattern[] = "u-*";
const char kExtensionsDirectory[] = "Extensions";
const char kExtensionManifestName[] = "manifest.json";

// Chrome extension manifest keys.
const char kManifestNameField[] = "name";
const char kManifestVersionField[] = "version";

// Returns a non-empty file path to the extension directory for
// an extension named |name| if one exists in |user_dir|.
// Additionally, if it finds an extension, |version| is populated
// with the extension version.
base::FilePath FindExtensionInUserDir(const base::FilePath& user_dir,
                                      const std::string& name,
                                      std::string* version) {
  base::FileEnumerator fe(user_dir.Append(kExtensionsDirectory),
                          /* recursive = */ true,
                          base::FileEnumerator::FileType::FILES);
  for (base::FilePath manifest_path = fe.Next();
       !manifest_path.empty();
       manifest_path = fe.Next()) {
    if (manifest_path.BaseName().value() != kExtensionManifestName)
      continue;

    // Read manifest and check name.
    std::string manifest;
    base::ReadFileToString(manifest_path, &manifest);
    JSONStringValueDeserializer deserializer(manifest);

    int error_code;
    std::string error_message;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(&error_code, &error_message);
    if (!value) {
      DLOG(WARNING) << "Failed to deserialize manifest \""
                    << manifest_path.value() << "\". Error "
                    << error_code << ": " << error_message;
      continue;
    }

    base::DictionaryValue* manifest_dict;
    if (!value->GetAsDictionary(&manifest_dict)) {
      DLOG(WARNING) << "Manifest \"" << manifest_path.value()
                    << "\" is not a JSON dictionary";
      continue;
    }

    std::string extension_name;
    if (!manifest_dict->GetString(kManifestNameField, &extension_name)) {
      DLOG(WARNING) << "Manifest \"" << manifest_path.value()
                    << "\" is malformed; no extension name specified";
      continue;
    }

    if (extension_name != name)
      continue;

    std::string extension_version;
    if (!manifest_dict->GetString(kManifestVersionField, &extension_version)) {
      DLOG(WARNING) << "Manifest \"" << manifest_path.value()
                    << "\" is malformed; no extension version specified";
      continue;
    }

    *version = extension_version;
    return manifest_path.DirName();
  }
  return base::FilePath();
}

// Searches all mounted user directories for an extension named |name| and
// returns the path to its extension directory. Additionally populates
// |version| with the extension version if found.
base::FilePath FindExtensionDirectory(const std::string& name,
                                      std::string* version) {
  base::FileEnumerator fe(base::FilePath(kChronosDirectory),
                          /* recursive = */ false,
                          base::FileEnumerator::FileType::DIRECTORIES,
                          kUserDirectoryPattern);
  for (base::FilePath user_dir = fe.Next();
       !user_dir.empty();
       user_dir = fe.Next()) {
    DVLOG(1) << "Searching user directory " << user_dir.value();
    base::FilePath extension_dir =
      FindExtensionInUserDir(user_dir, name, version);
    if (!extension_dir.empty())
      return extension_dir;
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


base::FilePath MountImage(const std::string& name,
                          const std::string& version,
                          const base::FilePath& component_dir) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

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

  std::string extension_version;
  base::FilePath extension_dir =
      FindExtensionDirectory(FLAGS_name, &extension_version);
  if (extension_dir.empty()) {
    LOG(ERROR) << "Could not find extension named \""
               << FLAGS_name << "\"";
    return 1;
  }

  DLOG(INFO) << "Found " << FLAGS_name << " " << extension_version;
  base::FilePath mount_dir = MountImage(FLAGS_name,
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
