// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/service.h"

#include <arpa/inet.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread_task_runner_handle.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_proxy.h>
#include <vm_applications/proto_bindings/apps.pb.h>
#include <vm_cicerone/proto_bindings/cicerone_service.pb.h>

#include "vm_tools/common/constants.h"

using std::string;

namespace vm_tools {
namespace cicerone {

namespace {

// Default name for a virtual machine.
constexpr char kDefaultVmName[] = "termina";

// Default name to use for a container.
constexpr char kDefaultContainerName[] = "penguin";

// Hostname for the default VM/container.
constexpr char kDefaultContainerHostname[] = "linuxhost";

// Delimiter for the end of a URL scheme.
constexpr char kUrlSchemeDelimiter[] = "://";

// Hostnames we replace with the container IP if they are sent over in URLs to
// be opened by the host.
const char* const kLocalhostReplaceNames[] = {"localhost", "127.0.0.1"};

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
}

// Posted to a grpc thread to startup a listener service. Puts a copy of
// the pointer to the grpc server in |server_copy| and then signals |event|.
// It will listen on the address specified in |listener_address|.
void RunListenerService(grpc::Service* listener,
                        const std::string& listener_address,
                        base::WaitableEvent* event,
                        std::shared_ptr<grpc::Server>* server_copy) {
  // We are not interested in getting SIGCHLD or SIGTERM on this thread.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Build the grpc server.
  grpc::ServerBuilder builder;
  builder.AddListeningPort(listener_address, grpc::InsecureServerCredentials());
  builder.RegisterService(listener);

  std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());

  *server_copy = server;
  event->Signal();

  if (server) {
    server->Wait();
  }
}

// Sets up a gRPC listener service by starting the |grpc_thread| and posting the
// main task to run for the thread. |listener_address| should be the address the
// gRPC server is listening on. A copy of the pointer to the server is put in
// |server_copy|. Returns true if setup & started successfully, false otherwise.
bool SetupListenerService(base::Thread* grpc_thread,
                          grpc::Service* listener_impl,
                          const std::string& listener_address,
                          std::shared_ptr<grpc::Server>* server_copy) {
  // Start the grpc thread.
  if (!grpc_thread->Start()) {
    LOG(ERROR) << "Failed to start grpc thread";
    return false;
  }

  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool ret = grpc_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&RunListenerService, listener_impl,
                            listener_address, &event, server_copy));
  if (!ret) {
    LOG(ERROR) << "Failed to post server startup task to grpc thread";
    return false;
  }

  // Wait for the VM grpc server to start.
  event.Wait();

  if (!server_copy) {
    LOG(ERROR) << "grpc server failed to start";
    return false;
  }

  return true;
}

// Converts an IPv4 address to a string. The result will be stored in |str|
// on success.
bool IPv4AddressToString(const uint32_t address, std::string* str) {
  CHECK(str);

  char result[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &address, result, sizeof(result)) != result) {
    return false;
  }
  *str = std::string(result);
  return true;
}

// Replaces either localhost or 127.0.0.1 in the hostname part of a URL with the
// IP address of the container itself.
std::string ReplaceLocalhostInUrl(const std::string& url,
                                  const std::string& alt_host) {
  // We don't have any URL parsing libraries at our disposal here without
  // integrating something new, so just do some basic URL parsing ourselves.
  // First find where the scheme ends, which'll be after the first :// string.
  // Then search for the next / char, which will start the path for the URL, the
  // hostname will be in the string between those two.
  // Also check for an @ symbol, which may have a user/pass before the hostname
  // and then check for a : at the end for an optional port.
  // scheme://[user:pass@]hostname[:port]/path
  auto front = url.find(kUrlSchemeDelimiter);
  if (front == std::string::npos) {
    return url;
  }
  front += sizeof(kUrlSchemeDelimiter) - 1;
  auto back = url.find('/', front);
  if (back == std::string::npos) {
    // This isn't invalid, such as http://google.com.
    back = url.length();
  }
  auto at_check = url.find('@', front);
  if (at_check != std::string::npos && at_check < back) {
    front = at_check + 1;
  }
  auto port_check = url.find(':', front);
  if (port_check != std::string::npos && port_check < back) {
    back = port_check;
  }
  // We don't care about URL validity, but our logic should ensure that front
  // is less than back at this point and this checks that.
  CHECK_LE(front, back);
  std::string hostname = url.substr(front, back - front);
  for (const auto host_check : kLocalhostReplaceNames) {
    if (hostname == host_check) {
      // Replace the hostname with the alternate hostname which will be the
      // container's IP address.
      return url.substr(0, front) + alt_host + url.substr(back);
    }
  }
  return url;
}

}  // namespace

std::unique_ptr<Service> Service::Create(base::Closure quit_closure) {
  auto service = base::WrapUnique(new Service(std::move(quit_closure)));

  if (!service->Init()) {
    service.reset();
  }

  return service;
}

Service::Service(base::Closure quit_closure)
    : watcher_(FROM_HERE),
      quit_closure_(std::move(quit_closure)),
      weak_ptr_factory_(this) {
  container_listener_ =
      std::make_unique<ContainerListenerImpl>(weak_ptr_factory_.GetWeakPtr());
}

Service::~Service() {
  if (grpc_server_container_) {
    grpc_server_container_->Shutdown();
  }
}

void Service::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(signal_fd_.get(), fd);

  struct signalfd_siginfo siginfo;
  if (read(signal_fd_.get(), &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
    PLOG(ERROR) << "Failed to read from signalfd";
    return;
  }

  if (siginfo.ssi_signo == SIGTERM) {
    HandleSigterm();
  } else {
    LOG(ERROR) << "Received unknown signal from signal fd: "
               << strsignal(siginfo.ssi_signo);
  }
}

void Service::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void Service::ContainerStartupCompleted(const std::string& container_token,
                                        const uint32_t container_ip,
                                        bool* result,
                                        base::WaitableEvent* event) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(result);
  CHECK(event);
  *result = false;
  VirtualMachine* vm;
  std::string vm_name;
  std::string owner_id;
  if (!GetVirtualMachineForContainerIp(container_ip, &vm, &owner_id,
                                       &vm_name)) {
    event->Signal();
    return;
  }

  // Found the VM with a matching container subnet, register the IP address
  // for the container with that VM object.
  std::string string_ip;
  if (!IPv4AddressToString(container_ip, &string_ip)) {
    LOG(ERROR) << "Failed converting IP address to string: " << container_ip;
    event->Signal();
    return;
  }
  if (!vm->RegisterContainer(container_token, string_ip)) {
    LOG(ERROR) << "Invalid container token passed back from VM " << vm_name
               << " of " << container_token;
    event->Signal();
    return;
  }
  std::string container_name = vm->GetContainerNameForToken(container_token);
  LOG(INFO) << "Startup of container " << container_name << " at IP "
            << string_ip << " for VM " << vm_name << " completed.";

  if (owner_id == primary_owner_id_) {
    // Register this with the hostname resolver.
    RegisterHostname(base::StringPrintf("%s-%s-local", container_name.c_str(),
                                        vm_name.c_str()),
                     string_ip);
    if (vm_name == kDefaultVmName && container_name == kDefaultContainerName) {
      RegisterHostname(kDefaultContainerHostname, string_ip);
    }
  }

  // Send the D-Bus signal out to indicate the container is ready.
  dbus::Signal signal(kVmCiceroneInterface, kContainerStartedSignal);
  vm_tools::cicerone::ContainerStartedSignal proto;
  proto.set_vm_name(vm_name);
  proto.set_container_name(container_name);
  proto.set_owner_id(owner_id);
  dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
  exported_object_->SendSignal(&signal);
  *result = true;
  event->Signal();
}

void Service::ContainerShutdown(const std::string& container_token,
                                const uint32_t container_ip,
                                bool* result,
                                base::WaitableEvent* event) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(result);
  CHECK(event);
  *result = false;
  VirtualMachine* vm;
  std::string owner_id;
  std::string vm_name;

  if (!GetVirtualMachineForContainerIp(container_ip, &vm, &owner_id,
                                       &vm_name)) {
    event->Signal();
    return;
  }
  std::string container_name = vm->GetContainerNameForToken(container_token);
  if (!vm->UnregisterContainer(container_token)) {
    LOG(ERROR) << "Invalid container token passed back from VM " << vm_name
               << " of " << container_token;
    event->Signal();
    return;
  }
  // Unregister this with the hostname resolver.
  UnregisterHostname(base::StringPrintf("%s-%s-local", container_name.c_str(),
                                        vm_name.c_str()));
  if (vm_name == kDefaultVmName && container_name == kDefaultContainerName) {
    UnregisterHostname(kDefaultContainerHostname);
  }

  LOG(INFO) << "Shutdown of container " << container_name << " for VM "
            << vm_name;

  // Send the D-Bus signal out to indicate the container has shutdown.
  dbus::Signal signal(kVmCiceroneInterface, kContainerShutdownSignal);
  ContainerShutdownSignal proto;
  proto.set_vm_name(vm_name);
  proto.set_container_name(container_name);
  proto.set_owner_id(owner_id);
  dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
  exported_object_->SendSignal(&signal);
  *result = true;
  event->Signal();
}

void Service::UpdateApplicationList(const std::string& container_token,
                                    const uint32_t container_ip,
                                    vm_tools::apps::ApplicationList* app_list,
                                    bool* result,
                                    base::WaitableEvent* event) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(app_list);
  CHECK(result);
  CHECK(event);
  *result = false;
  std::string owner_id;
  std::string vm_name;
  VirtualMachine* vm;
  if (!GetVirtualMachineForContainerIp(container_ip, &vm, &owner_id,
                                       &vm_name)) {
    event->Signal();
    return;
  }
  std::string container_name = vm->GetContainerNameForToken(container_token);
  if (container_name.empty()) {
    event->Signal();
    return;
  }
  app_list->set_vm_name(vm_name);
  app_list->set_container_name(container_name);
  app_list->set_owner_id(owner_id);
  dbus::MethodCall method_call(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceUpdateApplicationListMethod);
  dbus::MessageWriter writer(&method_call);

  if (!writer.AppendProtoAsArrayOfBytes(*app_list)) {
    LOG(ERROR) << "Failed to encode ApplicationList protobuf";
    event->Signal();
    return;
  }

  std::unique_ptr<dbus::Response> dbus_response =
      vm_applications_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to crostini app registry";
  } else {
    *result = true;
  }
  event->Signal();
}

void Service::OpenUrl(const std::string& url,
                      uint32_t container_ip,
                      bool* result,
                      base::WaitableEvent* event) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(result);
  CHECK(event);
  *result = false;
  dbus::MethodCall method_call(chromeos::kUrlHandlerServiceInterface,
                               chromeos::kUrlHandlerServiceOpenUrlMethod);
  dbus::MessageWriter writer(&method_call);
  std::string container_ip_str;
  if (!IPv4AddressToString(container_ip, &container_ip_str)) {
    LOG(ERROR) << "Failed converting IP address to string: " << container_ip;
    event->Signal();
    return;
  }
  if (container_ip_str == linuxhost_ip_) {
    container_ip_str = kDefaultContainerHostname;
  }
  writer.AppendString(ReplaceLocalhostInUrl(url, container_ip_str));
  std::unique_ptr<dbus::Response> dbus_response =
      url_handler_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to Chrome for OpenUrl";
  } else {
    *result = true;
  }
  event->Signal();
}

bool Service::Init() {
  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(std::move(opts));

  if (!bus_->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }

  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kVmCiceroneServicePath));
  if (!exported_object_) {
    LOG(ERROR) << "Failed to export " << kVmCiceroneServicePath << " object";
    return false;
  }

  using ServiceMethod =
      std::unique_ptr<dbus::Response> (Service::*)(dbus::MethodCall*);
  const std::map<const char*, ServiceMethod> kServiceMethods = {
      {kNotifyVmStartedMethod, &Service::NotifyVmStarted},
      {kNotifyVmStoppedMethod, &Service::NotifyVmStopped},
      {kGetContainerTokenMethod, &Service::GetContainerToken},
      {kIsContainerRunningMethod, &Service::IsContainerRunning},
      {kLaunchContainerApplicationMethod, &Service::LaunchContainerApplication},
      {kGetContainerAppIconMethod, &Service::GetContainerAppIcon},
      {kLaunchVshdMethod, &Service::LaunchVshd},
  };

  for (const auto& iter : kServiceMethods) {
    bool ret = exported_object_->ExportMethodAndBlock(
        kVmCiceroneInterface, iter.first,
        base::Bind(&HandleSynchronousDBusMethodCall,
                   base::Bind(iter.second, base::Unretained(this))));
    if (!ret) {
      LOG(ERROR) << "Failed to export method " << iter.first;
      return false;
    }
  }

  if (!bus_->RequestOwnershipAndBlock(kVmCiceroneServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to take ownership of " << kVmCiceroneServiceName;
    return false;
  }

  // Get the D-Bus proxy for communicating with the crostini registry in Chrome
  // and for the URL handler service.
  vm_applications_service_proxy_ = bus_->GetObjectProxy(
      vm_tools::apps::kVmApplicationsServiceName,
      dbus::ObjectPath(vm_tools::apps::kVmApplicationsServicePath));
  if (!vm_applications_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::apps::kVmApplicationsServiceName;
    return false;
  }
  url_handler_service_proxy_ =
      bus_->GetObjectProxy(chromeos::kUrlHandlerServiceName,
                           dbus::ObjectPath(chromeos::kUrlHandlerServicePath));
  if (!url_handler_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << chromeos::kUrlHandlerServiceName;
    return false;
  }
  crosdns_service_proxy_ =
      bus_->GetObjectProxy(crosdns::kCrosDnsServiceName,
                           dbus::ObjectPath(crosdns::kCrosDnsServicePath));
  if (!crosdns_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << crosdns::kCrosDnsServiceName;
    return false;
  }
  crosdns_service_proxy_->WaitForServiceToBeAvailable(base::Bind(
      &Service::OnCrosDnsServiceAvailable, weak_ptr_factory_.GetWeakPtr()));

  // Setup & start the gRPC listener services.
  if (!SetupListenerService(
          &grpc_thread_container_, container_listener_.get(),
          base::StringPrintf("[::]:%u", vm_tools::kGarconPort),
          &grpc_server_container_)) {
    LOG(ERROR) << "Failed to setup/startup the container grpc server";
    return false;
  }

  // Set up the signalfd for receiving SIGTERM.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);

  signal_fd_.reset(signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC));
  if (!signal_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to create signalfd";
    return false;
  }

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      signal_fd_.get(), true /*persistent*/, base::MessageLoopForIO::WATCH_READ,
      &watcher_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch signalfd";
    return false;
  }

  // Now block signals from the normal signal handling path so that we will get
  // them via the signalfd.
  if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0) {
    PLOG(ERROR) << "Failed to block signals via sigprocmask";
    return false;
  }

  return true;
}

void Service::HandleSigterm() {
  LOG(INFO) << "Shutting down due to SIGTERM";

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
}

std::unique_ptr<dbus::Response> Service::NotifyVmStarted(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received NotifyVmStarted request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  NotifyVmStartedRequest request;
  EmptyMessage response;
  writer.AppendProtoAsArrayOfBytes(response);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse NotifyVmStartedRequest from message";
    return dbus_response;
  }

  vms_[std::make_pair(request.owner_id(), std::move(request.vm_name()))] =
      std::make_unique<VirtualMachine>(request.container_ipv4_subnet(),
                                       request.container_ipv4_netmask(),
                                       request.ipv4_address());
  if (primary_owner_id_.empty() || vms_.empty()) {
    primary_owner_id_ = request.owner_id();
  }
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::NotifyVmStopped(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received NotifyVmStopped request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  NotifyVmStoppedRequest request;
  EmptyMessage response;
  writer.AppendProtoAsArrayOfBytes(response);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse NotifyVmStoppedRequest from message";
    return dbus_response;
  }

  VmKey vm_key =
      std::make_pair(std::move(request.owner_id()), request.vm_name());
  auto iter = vms_.find(vm_key);
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist: " << request.vm_name();
    return dbus_response;
  }

  UnregisterVmContainers(iter->second.get(), iter->first.first,
                         iter->first.second);

  vms_.erase(iter);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::GetContainerToken(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received GetContainerToken request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ContainerTokenRequest request;
  ContainerTokenResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ContainerTokenRequest from message";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  VirtualMachine* vm = FindVm(request.owner_id(), request.vm_name());
  if (!vm) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  response.set_container_token(
      vm->GenerateContainerToken(std::move(request.container_name())));
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::IsContainerRunning(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received IsContainerRunning request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  IsContainerRunningRequest request;
  IsContainerRunningResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse IsContainerRunningRequest from message";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  VirtualMachine* vm = FindVm(request.owner_id(), request.vm_name());
  if (!vm) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  response.set_container_running(
      vm->IsContainerRunning(std::move(request.container_name())));
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::LaunchContainerApplication(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received LaunchContainerApplication request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  LaunchContainerApplicationRequest request;
  LaunchContainerApplicationResponse response;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse LaunchContainerApplicationRequest from "
               << "message";
    response.set_success(false);
    response.set_failure_reason(
        "Unable to parse LaunchContainerApplicationRequest");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  VirtualMachine* vm = FindVm(request.owner_id(), request.vm_name());
  if (!vm) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    response.set_success(false);
    response.set_failure_reason("Requested VM does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.desktop_file_id().empty()) {
    LOG(ERROR) << "LaunchContainerApplicationRequest had an empty "
               << "desktop_file_id";
    response.set_success(false);
    response.set_failure_reason("Empty desktop_file_id in request");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::string error_msg;
  response.set_success(vm->LaunchContainerApplication(
      request.container_name().empty() ? kDefaultContainerName
                                       : request.container_name(),
      request.desktop_file_id(),
      std::vector<string>(
          std::make_move_iterator(request.mutable_files()->begin()),
          std::make_move_iterator(request.mutable_files()->end())),
      &error_msg));
  response.set_failure_reason(error_msg);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::GetContainerAppIcon(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received GetContainerAppIcon request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ContainerAppIconRequest request;
  ContainerAppIconResponse response;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ContainerAppIconRequest from message";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  VirtualMachine* vm = FindVm(request.owner_id(), request.vm_name());
  if (!vm) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.desktop_file_ids().size() == 0) {
    LOG(ERROR) << "ContainerAppIconRequest had an empty desktop_file_ids";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::vector<std::string> desktop_file_ids;
  for (std::string& id : *request.mutable_desktop_file_ids()) {
    desktop_file_ids.emplace_back(std::move(id));
  }

  std::vector<VirtualMachine::Icon> icons;
  icons.reserve(desktop_file_ids.size());

  if (!vm->GetContainerAppIcon(request.container_name().empty()
                                   ? kDefaultContainerName
                                   : request.container_name(),
                               std::move(desktop_file_ids), request.size(),
                               request.scale(), &icons)) {
    LOG(ERROR) << "GetContainerAppIcon failed";
  }

  for (auto& container_icon : icons) {
    auto* icon = response.add_icons();
    *icon->mutable_desktop_file_id() =
        std::move(container_icon.desktop_file_id);
    *icon->mutable_icon() = std::move(container_icon.content);
  }

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::LaunchVshd(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received LaunchVshd request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  LaunchVshdRequest request;
  LaunchVshdResponse response;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse LaunchVshdRequest from message";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.port() == 0) {
    LOG(ERROR) << "Port is not set in LaunchVshdRequest";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // TODO(jkardatzke): Remove the empty string check once Chrome is updated
  // to put the owner_id in this request.
  std::string owner_id = request.owner_id().empty()
                             ? primary_owner_id_
                             : std::move(request.owner_id());
  VirtualMachine* vm = FindVm(request.owner_id(), request.vm_name());
  if (!vm) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::string error_msg;
  vm->LaunchVshd(request.container_name().empty() ? kDefaultContainerName
                                                  : request.container_name(),
                 request.port(), &error_msg);

  response.set_success(true);
  response.set_failure_reason(error_msg);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

bool Service::GetVirtualMachineForContainerIp(uint32_t container_ip,
                                              VirtualMachine** vm_out,
                                              std::string* owner_id_out,
                                              std::string* name_out) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(vm_out);
  CHECK(owner_id_out);
  CHECK(name_out);
  for (const auto& vm : vms_) {
    const uint32_t netmask = vm.second->container_netmask();
    if ((vm.second->container_subnet() & netmask) != (container_ip & netmask)) {
      continue;
    }
    *owner_id_out = vm.first.first;
    *name_out = vm.first.second;
    *vm_out = vm.second.get();
    return true;
  }
  return false;
}

bool Service::GetVirtualMachineForVmIp(uint32_t vm_ip,
                                       VirtualMachine** vm_out,
                                       std::string* owner_id_out,
                                       std::string* name_out) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  CHECK(vm_out);
  CHECK(owner_id_out);
  CHECK(name_out);
  for (const auto& vm : vms_) {
    if (vm.second->ipv4_address() != vm_ip) {
      continue;
    }
    *owner_id_out = vm.first.first;
    *name_out = vm.first.second;
    *vm_out = vm.second.get();
    return true;
  }
  return false;
}

void Service::RegisterHostname(const std::string& hostname,
                               const std::string& ip) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  dbus::MethodCall method_call(crosdns::kCrosDnsInterfaceName,
                               crosdns::kSetHostnameIpMappingMethod);
  dbus::MessageWriter writer(&method_call);
  // Params are hostname, IPv4, IPv6 (but we don't have IPv6 yet).
  writer.AppendString(hostname);
  writer.AppendString(ip);
  writer.AppendString("");
  std::unique_ptr<dbus::Response> dbus_response =
      crosdns_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    // If there's some issue with the resolver service, don't make that
    // propagate to a higher level failure and just log it. We have logic for
    // setting this up again if that service restarts.
    LOG(WARNING)
        << "Failed to send dbus message to crosdns to register hostname";
  } else {
    hostname_mappings_[hostname] = ip;
    if (hostname == kDefaultContainerHostname)
      linuxhost_ip_ = ip;
  }
}

void Service::UnregisterVmContainers(VirtualMachine* vm,
                                     const std::string& owner_id,
                                     const std::string& vm_name) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!vm)
    return;
  // When we were in concierge, this method was important because we shared a
  // D-Bus thread with concierge who was stopping the VM. Now that we are in a
  // separate process, we should receive the gRPC call from the container for
  // container shutdown before we receive the D-Bus call from concierge for the
  // VM stopping. It is entirely possible that they come in out of order, so we
  // still need this in case that happens.
  std::vector<std::string> containers = vm->GetContainerNames();
  for (auto& container_name : containers) {
    LOG(WARNING) << "Latent container left in VM " << vm_name << " of "
                 << container_name;
    if (owner_id == primary_owner_id_) {
      UnregisterHostname(base::StringPrintf(
          "%s-%s-local", container_name.c_str(), vm_name.c_str()));
      if (vm_name == kDefaultVmName &&
          container_name == kDefaultContainerName) {
        UnregisterHostname(kDefaultContainerHostname);
      }
    }

    // Send the D-Bus signal to indicate the container has shutdown.
    dbus::Signal signal(kVmCiceroneInterface, kContainerShutdownSignal);
    ContainerShutdownSignal proto;
    proto.set_vm_name(vm_name);
    proto.set_container_name(container_name);
    proto.set_owner_id(owner_id);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
    exported_object_->SendSignal(&signal);
  }
}

void Service::UnregisterHostname(const std::string& hostname) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  dbus::MethodCall method_call(crosdns::kCrosDnsInterfaceName,
                               crosdns::kRemoveHostnameIpMappingMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(hostname);
  std::unique_ptr<dbus::Response> dbus_response =
      crosdns_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    // If there's some issue with the resolver service, don't make that
    // propagate to a higher level failure and just log it. We have logic for
    // setting this up again if that service restarts.
    LOG(WARNING) << "Failed to send dbus message to crosdns to unregister "
                 << "hostname";
  }
  hostname_mappings_.erase(hostname);
  if (hostname == kDefaultContainerHostname)
    linuxhost_ip_ = "";
}

void Service::OnCrosDnsNameOwnerChanged(const std::string& old_owner,
                                        const std::string& new_owner) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!new_owner.empty()) {
    // Re-register everything in our map.
    for (auto& pair : hostname_mappings_) {
      RegisterHostname(pair.first, pair.second);
    }
  }
}

void Service::OnCrosDnsServiceAvailable(bool service_is_available) {
  if (service_is_available) {
    crosdns_service_proxy_->SetNameOwnerChangedCallback(base::Bind(
        &Service::OnCrosDnsNameOwnerChanged, weak_ptr_factory_.GetWeakPtr()));
  }
}

VirtualMachine* Service::FindVm(const std::string& owner_id,
                                const std::string& vm_name) {
  VmKey vm_key = std::make_pair(owner_id, vm_name);
  auto iter = vms_.find(vm_key);
  if (iter != vms_.end())
    return iter->second.get();
  if (!owner_id.empty()) {
    // TODO(jkardatzke): Remove this empty owner check once the other CLs land
    // for setting this everywhere.
    vm_key = std::make_pair("", vm_name);
    auto iter = vms_.find(vm_key);
    if (iter != vms_.end())
      return iter->second.get();
  }
  return nullptr;
}

}  // namespace cicerone
}  // namespace vm_tools
