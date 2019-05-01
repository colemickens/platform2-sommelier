// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(dead_code)]

use std::collections::HashMap;
use std::error::Error;
use std::fmt;
use std::fs::OpenOptions;
use std::os::unix::fs::OpenOptionsExt;
use std::os::unix::io::IntoRawFd;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

use dbus::{BusType, Connection, ConnectionItem, Message, OwnedFd};
use protobuf::Message as ProtoMessage;

use backends::{Backend, VmFeatures};
use lsb_release::{LsbRelease, ReleaseChannel};
use proto::system_api::cicerone_service::{self, *};
use proto::system_api::seneschal_service::*;
use proto::system_api::service::*;

const IMAGE_TYPE_QCOW2: &str = "qcow2";
const QCOW_IMAGE_EXTENSION: &str = ".qcow2";
const REMOVABLE_MEDIA_ROOT: &str = "/media/removable";
const CRYPTOHOME_USER: &str = "/home/user";
const CRYPTOHOME_ROOT: &str = "/home";
const DOWNLOADS_DIR: &str = "Downloads";
const MNT_SHARED_ROOT: &str = "/mnt/shared";

/// Round to disk block size.
const DISK_SIZE_MASK: u64 = !511;
const DEFAULT_TIMEOUT_MS: i32 = 80 * 1000;
const EXPORT_DISK_TIMEOUT_MS: i32 = 15 * 60 * 1000;
const COMPONENT_UPDATER_TIMEOUT_MS: i32 = 120 * 1000;

// debugd dbus-constants.h
const DEBUGD_INTERFACE: &str = "org.chromium.debugd";
const DEBUGD_SERVICE_PATH: &str = "/org/chromium/debugd";
const DEBUGD_SERVICE_NAME: &str = "org.chromium.debugd";
const START_VM_CONCIERGE: &str = "StartVmConcierge";

// concierge dbus-constants.h
const VM_CONCIERGE_INTERFACE: &str = "org.chromium.VmConcierge";
const VM_CONCIERGE_SERVICE_PATH: &str = "/org/chromium/VmConcierge";
const VM_CONCIERGE_SERVICE_NAME: &str = "org.chromium.VmConcierge";
const START_VM_METHOD: &str = "StartVm";
const STOP_VM_METHOD: &str = "StopVm";
const STOP_ALL_VMS_METHOD: &str = "StopAllVms";
const GET_VM_INFO_METHOD: &str = "GetVmInfo";
const CREATE_DISK_IMAGE_METHOD: &str = "CreateDiskImage";
const DESTROY_DISK_IMAGE_METHOD: &str = "DestroyDiskImage";
const EXPORT_DISK_IMAGE_METHOD: &str = "ExportDiskImage";
const LIST_VM_DISKS_METHOD: &str = "ListVmDisks";
const START_CONTAINER_METHOD: &str = "StartContainer";
const GET_CONTAINER_SSH_KEYS_METHOD: &str = "GetContainerSshKeys";
const SYNC_VM_TIMES_METHOD: &str = "SyncVmTimes";
const ATTACH_USB_DEVICE_METHOD: &str = "AttachUsbDevice";
const DETACH_USB_DEVICE_METHOD: &str = "DetachUsbDevice";
const LIST_USB_DEVICE_METHOD: &str = "ListUsbDevices";
const START_PLUGIN_VM_METHOD: &str = "StartPluginVm";
const CONTAINER_STARTUP_FAILED_SIGNAL: &str = "ContainerStartupFailed";

// cicerone dbus-constants.h
const VM_CICERONE_INTERFACE: &str = "org.chromium.VmCicerone";
const VM_CICERONE_SERVICE_PATH: &str = "/org/chromium/VmCicerone";
const VM_CICERONE_SERVICE_NAME: &str = "org.chromium.VmCicerone";
const NOTIFY_VM_STARTED_METHOD: &str = "NotifyVmStarted";
const NOTIFY_VM_STOPPED_METHOD: &str = "NotifyVmStopped";
const GET_CONTAINER_TOKEN_METHOD: &str = "GetContainerToken";
const LAUNCH_CONTAINER_APPLICATION_METHOD: &str = "LaunchContainerApplication";
const GET_CONTAINER_APP_ICON_METHOD: &str = "GetContainerAppIcon";
const LAUNCH_VSHD_METHOD: &str = "LaunchVshd";
const GET_LINUX_PACKAGE_INFO_METHOD: &str = "GetLinuxPackageInfo";
const INSTALL_LINUX_PACKAGE_METHOD: &str = "InstallLinuxPackage";
const UNINSTALL_PACKAGE_OWNING_FILE_METHOD: &str = "UninstallPackageOwningFile";
const CREATE_LXD_CONTAINER_METHOD: &str = "CreateLxdContainer";
const START_LXD_CONTAINER_METHOD: &str = "StartLxdContainer";
const GET_LXD_CONTAINER_USERNAME_METHOD: &str = "GetLxdContainerUsername";
const SET_UP_LXD_CONTAINER_USER_METHOD: &str = "SetUpLxdContainerUser";
const APP_SEARCH_METHOD: &str = "AppSearch";
const GET_DEBUG_INFORMATION: &str = "GetDebugInformation";
const CONTAINER_STARTED_SIGNAL: &str = "ContainerStarted";
const CONTAINER_SHUTDOWN_SIGNAL: &str = "ContainerShutdown";
const INSTALL_LINUX_PACKAGE_PROGRESS_SIGNAL: &str = "InstallLinuxPackageProgress";
const UNINSTALL_PACKAGE_PROGRESS_SIGNAL: &str = "UninstallPackageProgress";
const LXD_CONTAINER_CREATED_SIGNAL: &str = "LxdContainerCreated";
const LXD_CONTAINER_DOWNLOADING_SIGNAL: &str = "LxdContainerDownloading";
const LXD_CONTAINER_STARTING_SIGNAL: &str = "LxdContainerStarting";
const TREMPLIN_STARTED_SIGNAL: &str = "TremplinStarted";

// seneschal dbus-constants.h
const SENESCHAL_INTERFACE: &str = "org.chromium.Seneschal";
const SENESCHAL_SERVICE_PATH: &str = "/org/chromium/Seneschal";
const SENESCHAL_SERVICE_NAME: &str = "org.chromium.Seneschal";
const START_SERVER_METHOD: &str = "StartServer";
const STOP_SERVER_METHOD: &str = "StopServer";
const SHARE_PATH_METHOD: &str = "SharePath";

// permission_broker dbus-constants.h
const PERMISSION_BROKER_INTERFACE: &str = "org.chromium.PermissionBroker";
const PERMISSION_BROKER_SERVICE_PATH: &str = "/org/chromium/PermissionBroker";
const PERMISSION_BROKER_SERVICE_NAME: &str = "org.chromium.PermissionBroker";
const CHECK_PATH_ACCESS: &str = "CheckPathAccess";
const OPEN_PATH: &str = "OpenPath";
const REQUEST_TCP_PORT_ACCESS: &str = "RequestTcpPortAccess";
const REQUEST_UDP_PORT_ACCESS: &str = "RequestUdpPortAccess";
const RELEASE_TCP_PORT: &str = "ReleaseTcpPort";
const RELEASE_UDP_PORT: &str = "ReleaseUdpPort";
const REQUEST_VPN_SETUP: &str = "RequestVpnSetup";
const REMOVE_VPN_SETUP: &str = "RemoveVpnSetup";
const POWER_CYCLE_USB_PORTS: &str = "PowerCycleUsbPorts";

enum ChromeOSError {
    BadConciergeStatus,
    BadDiskImageStatus(DiskImageStatus, String),
    BadVmStatus(VmStatus, String),
    EnableGpuOnStable,
    ExportPathExists,
    FailedAttachUsb(String),
    FailedComponentUpdater(String),
    FailedCreateContainer(CreateLxdContainerResponse_Status, String),
    FailedCreateContainerSignal(LxdContainerCreatedSignal_Status, String),
    FailedDetachUsb(String),
    FailedGetOpenPath(PathBuf),
    FailedGetVmInfo,
    FailedListDiskImages(String),
    FailedListUsb,
    FailedMetricsSend { exit_code: Option<i32> },
    FailedOpenPath(dbus::Error),
    FailedSetupContainerUser(SetUpLxdContainerUserResponse_Status, String),
    FailedSharePath(String),
    FailedStartContainerSignal(LxdContainerStartingSignal_Status, String),
    FailedStartContainerStatus(StartLxdContainerResponse_Status, String),
    FailedStopVm { vm_name: String, reason: String },
    InvalidExportPath,
    RetrieveActiveSessions,
    TpmOnStable,
}

use self::ChromeOSError::*;

impl fmt::Display for ChromeOSError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BadConciergeStatus => write!(f, "failed to start concierge"),
            BadDiskImageStatus(s, reason) => {
                write!(f, "bad disk image status: `{:?}`: {}", s, reason)
            }
            BadVmStatus(s, reason) => write!(f, "bad VM status: `{:?}`: {}", s, reason),
            EnableGpuOnStable => write!(f, "gpu support is disabled on the stable channel"),
            ExportPathExists => write!(f, "disk export path already exists"),
            FailedAttachUsb(reason) => write!(f, "failed to attach usb device to vm: {}", reason),
            FailedDetachUsb(reason) => write!(f, "failed to detach usb device from vm: {}", reason),
            FailedComponentUpdater(name) => {
                write!(f, "component updater could not load component `{}`", name)
            }
            FailedCreateContainer(s, reason) => {
                write!(f, "failed to create container: `{:?}`: {}", s, reason)
            }
            FailedCreateContainerSignal(s, reason) => {
                write!(f, "failed to create container: `{:?}`: {}", s, reason)
            }
            FailedStartContainerSignal(s, reason) => {
                write!(f, "failed to start container: `{:?}`: {}", s, reason)
            }
            FailedGetOpenPath(path) => write!(f, "failed to request OpenPath {}", path.display()),
            FailedGetVmInfo => write!(f, "failed to get vm info"),
            FailedSetupContainerUser(s, reason) => {
                write!(f, "failed to setup container user: `{:?}`: {}", s, reason)
            }
            FailedSharePath(reason) => write!(f, "failed to share path with vm: {}", reason),
            FailedStartContainerStatus(s, reason) => {
                write!(f, "failed to start container: `{:?}`: {}", s, reason)
            }
            FailedListDiskImages(reason) => write!(f, "failed to list disk images: {}", reason),
            FailedListUsb => write!(f, "failed to get list of usb devices attached to vm"),
            FailedMetricsSend { exit_code } => {
                write!(f, "failed to send metrics")?;
                if let Some(code) = exit_code {
                    write!(f, ": exited with non-zero code {}", code)?;
                }
                Ok(())
            }
            FailedOpenPath(e) => write!(
                f,
                "failed permission_broker OpenPath: {}",
                e.message().unwrap_or("")
            ),
            FailedStopVm { vm_name, reason } => {
                write!(f, "failed to stop vm `{}`: {}", vm_name, reason)
            }
            InvalidExportPath => write!(f, "disk export path is invalid"),
            RetrieveActiveSessions => write!(f, "failed to retrieve active sessions"),
            TpmOnStable => write!(f, "TPM device is not available on stable channel"),
        }
    }
}

impl fmt::Debug for ChromeOSError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <Self as fmt::Display>::fmt(self, f)
    }
}

impl Error for ChromeOSError {}

fn dbus_message_to_proto<T: ProtoMessage>(message: &Message) -> Result<T, Box<Error>> {
    let raw_buffer: Vec<u8> = message.read1()?;
    let mut proto = T::new();
    proto.merge_from_bytes(&raw_buffer)?;
    Ok(proto)
}

/// Uses the standard ChromeOS interfaces to implement the backends methods with the least possible
/// privilege. Uses a combination of D-Bus, protobufs, and shell protocols.
pub struct ChromeOS {
    connection: Connection,
}

impl ChromeOS {
    /// Initiates a D-Bus connection and returns an initialized backend.
    pub fn new() -> Result<ChromeOS, Box<Error>> {
        let connection = Connection::get_private(BusType::System)?;
        Ok(ChromeOS { connection })
    }
}

impl ChromeOS {
    /// Helper for doing protobuf over dbus requests and responses.
    fn sync_protobus<I: ProtoMessage, O: ProtoMessage>(
        &mut self,
        message: Message,
        request: &I,
    ) -> Result<O, Box<Error>> {
        self.sync_protobus_timeout(message, request, &[], DEFAULT_TIMEOUT_MS)
    }

    /// Helper for doing protobuf over dbus requests and responses.
    fn sync_protobus_timeout<I: ProtoMessage, O: ProtoMessage>(
        &mut self,
        message: Message,
        request: &I,
        fds: &[OwnedFd],
        timeout_millis: i32,
    ) -> Result<O, Box<Error>> {
        let method = message.append1(request.write_to_bytes()?).append_ref(fds);
        let message = self
            .connection
            .send_with_reply_and_block(method, timeout_millis)?;
        dbus_message_to_proto(&message)
    }

    fn protobus_wait_for_signal_timeout<O: ProtoMessage>(
        &mut self,
        interface: &str,
        signal: &str,
        timeout_millis: i32,
    ) -> Result<O, Box<Error>> {
        let rule = format!("interface='{}',member='{}'", interface, signal);
        self.connection.add_match(&rule)?;
        let mut result: Result<O, Box<Error>> = Err("timeout while waiting for signal".into());
        for item in self.connection.iter(timeout_millis) {
            match item {
                ConnectionItem::Signal(message) => {
                    if let (_, _, Some(msg_interface), Some(msg_signal)) = message.headers() {
                        if msg_interface == interface && signal == msg_signal {
                            result = dbus_message_to_proto(&message);
                            break;
                        }
                    }
                }
                ConnectionItem::Nothing => break,
                _ => {}
            };
        }
        self.connection.remove_match(&rule)?;
        result
    }

    /// Request that component updater load a named component.
    fn component_updater_load_component(&mut self, name: &str) -> Result<(), Box<Error>> {
        let method = Message::new_method_call(
            "org.chromium.ComponentUpdaterService",
            "/org/chromium/ComponentUpdaterService",
            "org.chromium.ComponentUpdaterService",
            "LoadComponent",
        )?
        .append1(name);
        let message = self
            .connection
            .send_with_reply_and_block(method, COMPONENT_UPDATER_TIMEOUT_MS)?;
        match message.get1() {
            Some("") | None => Err(FailedComponentUpdater(name.to_owned()).into()),
            _ => Ok(()),
        }
    }

    /// Request debugd to start vm_concierge.
    fn start_concierge(&mut self) -> Result<(), Box<Error>> {
        // Mount the termina component, waiting up to 2 minutes to download it. If this fails we
        // won't be able to start the concierge service below.
        self.component_updater_load_component("cros-termina")?;

        let method = Message::new_method_call(
            DEBUGD_SERVICE_NAME,
            DEBUGD_SERVICE_PATH,
            DEBUGD_INTERFACE,
            START_VM_CONCIERGE,
        )?;

        let message = self.connection.send_with_reply_and_block(method, 30000)?;
        match message.get1() {
            Some(true) => Ok(()),
            _ => Err(BadConciergeStatus.into()),
        }
    }

    /// Request that concierge create a disk image.
    fn create_disk_image(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
    ) -> Result<String, Box<Error>> {
        let mut request = CreateDiskImageRequest::new();
        request.disk_path = vm_name.to_owned();
        request.cryptohome_id = user_id_hash.to_owned();
        request.image_type = DiskImageType::DISK_IMAGE_AUTO;
        request.storage_location = StorageLocation::STORAGE_CRYPTOHOME_ROOT;

        let response: CreateDiskImageResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                CREATE_DISK_IMAGE_METHOD,
            )?,
            &request,
        )?;

        match response.status {
            DiskImageStatus::DISK_STATUS_EXISTS | DiskImageStatus::DISK_STATUS_CREATED => {
                Ok(response.disk_path)
            }
            _ => Err(BadDiskImageStatus(response.status, response.failure_reason).into()),
        }
    }

    /// Request that concierge create a disk image.
    fn destroy_disk_image(&mut self, vm_name: &str, user_id_hash: &str) -> Result<(), Box<Error>> {
        let mut request = DestroyDiskImageRequest::new();
        request.disk_path = vm_name.to_owned();
        request.cryptohome_id = user_id_hash.to_owned();

        let response: DestroyDiskImageResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                DESTROY_DISK_IMAGE_METHOD,
            )?,
            &request,
        )?;

        match response.status {
            DiskImageStatus::DISK_STATUS_DESTROYED
            | DiskImageStatus::DISK_STATUS_DOES_NOT_EXIST => Ok(()),
            _ => Err(BadDiskImageStatus(response.status, response.failure_reason).into()),
        }
    }

    /// Request that concierge export a VM's disk image.
    fn export_disk_image(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        export_name: &str,
        removable_media: Option<&str>,
    ) -> Result<(), Box<Error>> {
        let file_name = format!("{}{}", export_name, QCOW_IMAGE_EXTENSION);
        let export_path = match removable_media {
            Some(media_path) => Path::new(REMOVABLE_MEDIA_ROOT)
                .join(media_path)
                .join(file_name),
            None => Path::new(CRYPTOHOME_USER)
                .join(user_id_hash)
                .join(DOWNLOADS_DIR)
                .join(file_name),
        };

        if export_path.components().any(|c| c == Component::ParentDir) {
            return Err(InvalidExportPath.into());
        }

        if export_path.exists() {
            return Err(ExportPathExists.into());
        }

        // Exporting the disk should always create a new file, and only be accessible to the user
        // that creates it. The old version of this used `O_NOFOLLOW` in its open flags, but this
        // has no effect as `O_NOFOLLOW` only preempts symlinks for the final part of the path,
        // which is guaranteed to not exist by `create_new(true)`.
        let export_file = OpenOptions::new()
            .write(true)
            .read(true)
            .create_new(true)
            .mode(0o600)
            .open(export_path)?;
        let export_fd = OwnedFd::new(export_file.into_raw_fd());

        let mut request = ExportDiskImageRequest::new();
        request.disk_path = vm_name.to_owned();
        request.cryptohome_id = user_id_hash.to_owned();

        // We can't use sync_protobus because we need to append the file descriptor out of band from
        // the protobuf message.
        let method = Message::new_method_call(
            VM_CONCIERGE_SERVICE_NAME,
            VM_CONCIERGE_SERVICE_PATH,
            VM_CONCIERGE_INTERFACE,
            EXPORT_DISK_IMAGE_METHOD,
        )?
        .append1(request.write_to_bytes()?)
        .append1(export_fd);

        let message = self
            .connection
            .send_with_reply_and_block(method, EXPORT_DISK_TIMEOUT_MS)?;

        let response: ExportDiskImageResponse = dbus_message_to_proto(&message)?;
        match response.status {
            DiskImageStatus::DISK_STATUS_CREATED => Ok(()),
            _ => Err(BadDiskImageStatus(response.status, response.failure_reason).into()),
        }
    }

    /// Request a list of disk images from concierge.
    fn list_disk_images(
        &mut self,
        user_id_hash: &str,
    ) -> Result<(Vec<(String, u64)>, u64), Box<Error>> {
        let mut request = ListVmDisksRequest::new();
        request.cryptohome_id = user_id_hash.to_owned();
        request.all_locations = true;

        let response: ListVmDisksResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                LIST_VM_DISKS_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            let images: Vec<(String, u64)> = response
                .images
                .into_iter()
                .map(|e| (e.name, e.size))
                .collect();
            Ok((images, response.total_size))
        } else {
            Err(FailedListDiskImages(response.failure_reason).into())
        }
    }

    /// Request that concierge start a vm with the given disk image.
    fn start_vm_with_disk(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        features: VmFeatures,
        disk_image_path: String,
    ) -> Result<(), Box<Error>> {
        let mut request = StartVmRequest::new();
        request.start_termina = true;
        request.owner_id = user_id_hash.to_owned();
        request.enable_gpu = features.gpu;
        request.software_tpm = features.software_tpm;
        request.name = vm_name.to_owned();
        {
            let disk_image = request.mut_disks().push_default();
            disk_image.path = disk_image_path;
            disk_image.writable = true;
            disk_image.do_mount = false;
        }

        let response: StartVmResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                START_VM_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            return Ok(());
        }

        match response.status {
            VmStatus::VM_STATUS_RUNNING | VmStatus::VM_STATUS_STARTING => Ok(()),
            _ => Err(BadVmStatus(response.status, response.failure_reason).into()),
        }
    }

    /// Request that `concierge` stop a vm with the given disk image.
    fn stop_vm(&mut self, vm_name: &str, user_id_hash: &str) -> Result<(), Box<Error>> {
        let mut request = StopVmRequest::new();
        request.owner_id = user_id_hash.to_owned();
        request.name = vm_name.to_owned();

        let response: StopVmResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                STOP_VM_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            Ok(())
        } else {
            Err(FailedStopVm {
                vm_name: vm_name.to_owned(),
                reason: response.failure_reason,
            }
            .into())
        }
    }

    // Request `VmInfo` from concierge about given `vm_name`.
    fn get_vm_info(&mut self, vm_name: &str, user_id_hash: &str) -> Result<VmInfo, Box<Error>> {
        let mut request = GetVmInfoRequest::new();
        request.owner_id = user_id_hash.to_owned();
        request.name = vm_name.to_owned();

        let response: GetVmInfoResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                GET_VM_INFO_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            Ok(response.vm_info.unwrap_or_default())
        } else {
            Err(FailedGetVmInfo.into())
        }
    }

    // Request the given `path` be shared with the seneschal instance associated with the desired
    // vm, owned by `user_id_hash`.
    fn share_path_with_vm(
        &mut self,
        seneschal_handle: u32,
        user_id_hash: &str,
        path: &str,
    ) -> Result<String, Box<Error>> {
        let mut request = SharePathRequest::new();
        request.handle = seneschal_handle;
        request.shared_path.set_default().path = path.to_owned();
        request.storage_location = SharePathRequest_StorageLocation::MY_FILES;
        request.owner_id = user_id_hash.to_owned();

        let response: SharePathResponse = self.sync_protobus(
            Message::new_method_call(
                SENESCHAL_SERVICE_NAME,
                SENESCHAL_SERVICE_PATH,
                SENESCHAL_INTERFACE,
                SHARE_PATH_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            Ok(response.path)
        } else {
            Err(FailedSharePath(response.failure_reason).into())
        }
    }

    fn create_container(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
        image_server: &str,
        image_alias: &str,
    ) -> Result<(), Box<Error>> {
        let mut request = CreateLxdContainerRequest::new();
        request.vm_name = vm_name.to_owned();
        request.container_name = container_name.to_owned();
        request.owner_id = user_id_hash.to_owned();
        request.image_server = image_server.to_owned();
        request.image_alias = image_alias.to_owned();

        let response: CreateLxdContainerResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CICERONE_SERVICE_NAME,
                VM_CICERONE_SERVICE_PATH,
                VM_CICERONE_INTERFACE,
                CREATE_LXD_CONTAINER_METHOD,
            )?,
            &request,
        )?;

        use self::CreateLxdContainerResponse_Status::*;
        use self::LxdContainerCreatedSignal_Status::*;
        match response.status {
            CREATING => {
                let signal: LxdContainerCreatedSignal = self.protobus_wait_for_signal_timeout(
                    VM_CICERONE_INTERFACE,
                    LXD_CONTAINER_CREATED_SIGNAL,
                    DEFAULT_TIMEOUT_MS,
                )?;
                match signal.status {
                    CREATED => Ok(()),
                    _ => Err(
                        FailedCreateContainerSignal(signal.status, signal.failure_reason).into(),
                    ),
                }
            }
            EXISTS => Ok(()),
            _ => Err(FailedCreateContainer(response.status, response.failure_reason).into()),
        }
    }

    fn start_container(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
    ) -> Result<(), Box<Error>> {
        let mut request = StartLxdContainerRequest::new();
        request.vm_name = vm_name.to_owned();
        request.container_name = container_name.to_owned();
        request.owner_id = user_id_hash.to_owned();
        request.async = true;

        let response: StartLxdContainerResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CICERONE_SERVICE_NAME,
                VM_CICERONE_SERVICE_PATH,
                VM_CICERONE_INTERFACE,
                START_LXD_CONTAINER_METHOD,
            )?,
            &request,
        )?;

        use self::StartLxdContainerResponse_Status::*;
        match response.status {
            STARTING => {
                let signal: cicerone_service::LxdContainerStartingSignal = self
                    .protobus_wait_for_signal_timeout(
                        VM_CICERONE_INTERFACE,
                        LXD_CONTAINER_STARTING_SIGNAL,
                        DEFAULT_TIMEOUT_MS,
                    )?;
                match signal.status {
                    LxdContainerStartingSignal_Status::STARTED => Ok(()),
                    _ => {
                        Err(FailedStartContainerSignal(signal.status, signal.failure_reason).into())
                    }
                }
            }
            RUNNING => Ok(()),
            _ => Err(FailedStartContainerStatus(response.status, response.failure_reason).into()),
        }
    }

    fn setup_container_user(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
        username: &str,
    ) -> Result<(), Box<Error>> {
        let mut request = SetUpLxdContainerUserRequest::new();
        request.vm_name = vm_name.to_owned();
        request.owner_id = user_id_hash.to_owned();
        request.container_name = container_name.to_owned();
        request.container_username = username.to_owned();

        let response: SetUpLxdContainerUserResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CICERONE_SERVICE_NAME,
                VM_CICERONE_SERVICE_PATH,
                VM_CICERONE_INTERFACE,
                SET_UP_LXD_CONTAINER_USER_METHOD,
            )?,
            &request,
        )?;

        use self::SetUpLxdContainerUserResponse_Status::*;
        match response.status {
            SUCCESS | EXISTS => {
                // TODO: listen for signal before calling the D-Bus method.
                let _signal: cicerone_service::ContainerStartedSignal = self
                    .protobus_wait_for_signal_timeout(
                        VM_CICERONE_INTERFACE,
                        CONTAINER_STARTED_SIGNAL,
                        DEFAULT_TIMEOUT_MS,
                    )?;
                Ok(())
            }
            _ => Err(FailedSetupContainerUser(response.status, response.failure_reason).into()),
        }
    }

    fn attach_usb(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        bus: u8,
        device: u8,
        usb_fd: OwnedFd,
    ) -> Result<u8, Box<Error>> {
        let mut request = AttachUsbDeviceRequest::new();
        request.owner_id = user_id_hash.to_owned();
        request.vm_name = vm_name.to_owned();
        request.bus_number = bus as u32;
        request.port_number = device as u32;

        let response: AttachUsbDeviceResponse = self.sync_protobus_timeout(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                ATTACH_USB_DEVICE_METHOD,
            )?,
            &request,
            &[usb_fd],
            DEFAULT_TIMEOUT_MS,
        )?;

        if response.success {
            Ok(response.guest_port as u8)
        } else {
            Err(FailedAttachUsb(response.reason).into())
        }
    }

    fn detach_usb(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        port: u8,
    ) -> Result<(), Box<Error>> {
        let mut request = DetachUsbDeviceRequest::new();
        request.owner_id = user_id_hash.to_owned();
        request.vm_name = vm_name.to_owned();
        request.guest_port = port as u32;

        let response: DetachUsbDeviceResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                DETACH_USB_DEVICE_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            Ok(())
        } else {
            Err(FailedDetachUsb(response.reason).into())
        }
    }

    fn list_usb(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
    ) -> Result<Vec<UsbDeviceMessage>, Box<Error>> {
        let mut request = ListUsbDeviceRequest::new();
        request.owner_id = user_id_hash.to_owned();
        request.vm_name = vm_name.to_owned();

        let response: ListUsbDeviceResponse = self.sync_protobus(
            Message::new_method_call(
                VM_CONCIERGE_SERVICE_NAME,
                VM_CONCIERGE_SERVICE_PATH,
                VM_CONCIERGE_INTERFACE,
                LIST_USB_DEVICE_METHOD,
            )?,
            &request,
        )?;

        if response.success {
            Ok(response.usb_devices.into())
        } else {
            Err(FailedListUsb.into())
        }
    }

    fn permission_broker_open_path(&mut self, path: &Path) -> Result<OwnedFd, Box<Error>> {
        let path_str = path
            .to_str()
            .ok_or_else(|| FailedGetOpenPath(path.into()))?;
        let method = Message::new_method_call(
            PERMISSION_BROKER_SERVICE_NAME,
            PERMISSION_BROKER_SERVICE_PATH,
            PERMISSION_BROKER_INTERFACE,
            OPEN_PATH,
        )?
        .append1(path_str);

        let message = self
            .connection
            .send_with_reply_and_block(method, DEFAULT_TIMEOUT_MS)
            .map_err(FailedOpenPath)?;

        message
            .get1()
            .ok_or_else(|| FailedGetOpenPath(path.into()).into())
    }
}

impl Backend for ChromeOS {
    fn name(&self) -> &'static str {
        "ChromeOS"
    }

    fn metrics_send_sample(&mut self, name: &str) -> Result<(), Box<Error>> {
        let status = Command::new("metrics_client")
            .arg("-v")
            .arg(name)
            .status()?;
        if !status.success() {
            return Err(FailedMetricsSend {
                exit_code: status.code(),
            }
            .into());
        }
        Ok(())
    }

    fn sessions_list(&mut self) -> Result<Vec<(String, String)>, Box<Error>> {
        let method = Message::new_method_call(
            "org.chromium.SessionManager",
            "/org/chromium/SessionManager",
            "org.chromium.SessionManagerInterface",
            "RetrieveActiveSessions",
        )?;
        let message = self
            .connection
            .send_with_reply_and_block(method, DEFAULT_TIMEOUT_MS)?;
        match message.get1::<HashMap<String, String>>() {
            Some(sessions) => Ok(sessions.into_iter().collect()),
            _ => Err(RetrieveActiveSessions.into()),
        }
    }

    fn vm_start(
        &mut self,
        name: &str,
        user_id_hash: &str,
        features: VmFeatures,
    ) -> Result<(), Box<Error>> {
        let is_stable_channel = is_stable_channel();
        if features.gpu && is_stable_channel {
            return Err(EnableGpuOnStable.into());
        }
        if features.software_tpm && is_stable_channel {
            return Err(TpmOnStable.into());
        }
        self.start_concierge()?;
        let disk_image_path = self.create_disk_image(name, user_id_hash)?;
        self.start_vm_with_disk(name, user_id_hash, features, disk_image_path)
    }

    fn vm_stop(&mut self, name: &str, user_id_hash: &str) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.stop_vm(name, user_id_hash)
    }

    fn vm_export(
        &mut self,
        name: &str,
        user_id_hash: &str,
        file_name: &str,
        removable_media: Option<&str>,
    ) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.export_disk_image(name, user_id_hash, file_name, removable_media)
    }

    fn vm_share_path(
        &mut self,
        name: &str,
        user_id_hash: &str,
        path: &str,
    ) -> Result<String, Box<Error>> {
        self.start_concierge()?;
        let vm_info = self.get_vm_info(name, user_id_hash)?;
        // The VmInfo uses a u64 as the handle, but SharePathRequest uses a u32 for the handle.
        if vm_info.seneschal_server_handle > u64::from(u32::max_value()) {
            return Err(FailedGetVmInfo.into());
        }
        let seneschal_handle = vm_info.seneschal_server_handle as u32;
        let vm_path = self.share_path_with_vm(seneschal_handle, user_id_hash, path)?;
        Ok(format!("{}{}", MNT_SHARED_ROOT, vm_path))
    }

    fn vsh_exec(&mut self, vm_name: &str, user_id_hash: &str) -> Result<(), Box<Error>> {
        Command::new("vsh")
            .arg(format!("--vm_name={}", vm_name))
            .arg(format!("--owner_id={}", user_id_hash))
            .args(&[
                "--",
                "LXD_DIR=/mnt/stateful/lxd",
                "LXD_CONF=/mnt/stateful/lxd_conf",
            ])
            .status()?;

        Ok(())
    }

    fn vsh_exec_container(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
    ) -> Result<(), Box<Error>> {
        Command::new("vsh")
            .arg(format!("--vm_name={}", vm_name))
            .arg(format!("--owner_id={}", user_id_hash))
            .arg(format!("--target_container={}", container_name))
            .args(&[
                "--",
                "LXD_DIR=/mnt/stateful/lxd",
                "LXD_CONF=/mnt/stateful/lxd_conf",
            ])
            .status()?;

        Ok(())
    }

    fn disk_destroy(&mut self, vm_name: &str, user_id_hash: &str) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.destroy_disk_image(vm_name, user_id_hash)
    }

    fn disk_list(&mut self, user_id_hash: &str) -> Result<(Vec<(String, u64)>, u64), Box<Error>> {
        self.start_concierge()?;
        self.list_disk_images(user_id_hash)
    }

    fn container_create(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
        image_server: &str,
        image_alias: &str,
    ) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.create_container(
            vm_name,
            user_id_hash,
            container_name,
            image_server,
            image_alias,
        )
    }

    fn container_start(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
    ) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.start_container(vm_name, user_id_hash, container_name)
    }

    fn container_setup_user(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        container_name: &str,
        username: &str,
    ) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.setup_container_user(vm_name, user_id_hash, container_name, username)
    }

    fn usb_attach(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        bus: u8,
        device: u8,
    ) -> Result<u8, Box<Error>> {
        self.start_concierge()?;
        let usb_file_path = format!("/dev/bus/usb/{:03}/{:03}", bus, device);
        let usb_fd = self.permission_broker_open_path(Path::new(&usb_file_path))?;
        self.attach_usb(vm_name, user_id_hash, bus, device, usb_fd)
    }

    fn usb_detach(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
        port: u8,
    ) -> Result<(), Box<Error>> {
        self.start_concierge()?;
        self.detach_usb(vm_name, user_id_hash, port)
    }

    fn usb_list(
        &mut self,
        vm_name: &str,
        user_id_hash: &str,
    ) -> Result<Vec<(u8, u16, u16, String)>, Box<Error>> {
        self.start_concierge()?;
        let device_list = self
            .list_usb(vm_name, user_id_hash)?
            .into_iter()
            .map(|mut d| {
                (
                    d.get_guest_port() as u8,
                    d.get_vendor_id() as u16,
                    d.get_product_id() as u16,
                    d.take_device_name(),
                )
            })
            .collect();
        Ok(device_list)
    }
}

fn is_stable_channel() -> bool {
    match LsbRelease::gather() {
        Ok(lsb) => lsb.release_channel() == Some(ReleaseChannel::Stable),
        Err(_) => {
            // Weird /etc/lsb-release, do not enforce stable restrictions.
            false
        }
    }
}
