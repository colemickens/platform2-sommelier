// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/libvda/gpu/gpu_vda_impl.h"

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/single_thread_task_runner.h>
#include <base/synchronization/waitable_event.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <sys/eventfd.h>

namespace arc {
namespace {

// Minimum required version of VideoAcceleratorFactory interface.
// Set to 6 which is when CreateDecodeAccelerator was introduced.
constexpr uint32_t kRequiredVideoAcceleratorFactoryMojoVersion = 6;

// Global GpuVdaImpl object.
GpuVdaImpl* active_impl = nullptr;

void RunTaskOnThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::OnceClosure task) {
  if (task_runner->BelongsToCurrentThread()) {
    LOG(WARNING) << "RunTaskOnThread called on target thread.";
    std::move(task).Run();
    return;
  }

  base::WaitableEvent task_complete_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure task, base::WaitableEvent* task_complete_event) {
            std::move(task).Run();
            task_complete_event->Signal();
          },
          std::move(task), &task_complete_event));
  task_complete_event.Wait();
}

inline vda_result_t ConvertResult(
    arc::mojom::VideoDecodeAccelerator::Result error) {
  switch (error) {
    case arc::mojom::VideoDecodeAccelerator::Result::SUCCESS:
      return SUCCESS;
    case arc::mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE:
      return ILLEGAL_STATE;
    case arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT:
      return INVALID_ARGUMENT;
    case arc::mojom::VideoDecodeAccelerator::Result::UNREADABLE_INPUT:
      return UNREADABLE_INPUT;
    case arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE:
      return PLATFORM_FAILURE;
    case arc::mojom::VideoDecodeAccelerator::Result::INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_RESOURCES;
    case arc::mojom::VideoDecodeAccelerator::Result::CANCELLED:
      return CANCELLED;
    default:
      DLOG(ERROR) << "Unknown error code: " << error;
      return PLATFORM_FAILURE;
  }
}

// GpuVdaContext is the GPU decode session context created by GpuVdaImpl which
// handles all mojo VideoDecodeClient invocations and callbacks.
class GpuVdaContext : public VdaContext, arc::mojom::VideoDecodeClient {
 public:
  GpuVdaContext(
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      arc::mojom::VideoDecodeAcceleratorPtr vda_ptr);
  ~GpuVdaContext();

  using InitializeCallback = base::OnceCallback<void(vda_result_t)>;

  // Initializes the VDA context object. When complete, callback is called with
  // the result.
  void Initialize(InitializeCallback callback);

  // VdaContext overrides.
  vda_result_t Decode(int32_t bitstream_id,
                      int fd,
                      uint32_t offset,
                      uint32_t bytes_used) override;
  vda_result_t SetOutputBufferCount(size_t num_output_buffers) override;
  vda_result_t UseOutputBuffer(int32_t picture_buffer_id,
                               vda_pixel_format_t format,
                               int fd,
                               size_t num_planes,
                               video_frame_plane_t* planes) override;
  vda_result_t Reset() override;
  vda_result_t Flush() override;

  // arc::mojom::VideoDecodeClient overrides.
  void ProvidePictureBuffers(
      arc::mojom::PictureBufferFormatPtr format_ptr) override;
  void PictureReady(arc::mojom::PicturePtr) override;
  void NotifyError(arc::mojom::VideoDecodeAccelerator::Result error) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_id) override;

 private:
  void OnInitialized(InitializeCallback callback,
                     arc::mojom::VideoDecodeAccelerator::Result result);
  void OnVdaError(uint32_t custom_reason, const std::string& description);
  void OnVdaClientError(uint32_t custom_reason, const std::string& description);
  // Looks up a bitstream ID and returns the associated FD.
  int GetFdFromBitstreamId(int32_t bitstream_id);

  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  // TODO(alexlau): Use THREAD_CHECKER macro after libchrome uprev
  // (crbug.com/909719).
  base::ThreadChecker ipc_thread_checker_;
  arc::mojom::VideoDecodeAcceleratorPtr vda_ptr_;
  mojo::Binding<arc::mojom::VideoDecodeClient> binding_;
};

GpuVdaContext::GpuVdaContext(
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    arc::mojom::VideoDecodeAcceleratorPtr vda_ptr)
    : ipc_task_runner_(ipc_task_runner),
      vda_ptr_(std::move(vda_ptr)),
      binding_(this) {
  // Since ipc_thread_checker_ binds to whichever thread it's created on, check
  // that we're on the correct thread first using BelongsToCurrentThread.
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  vda_ptr_.set_connection_error_with_reason_handler(
      base::Bind(&GpuVdaContext::OnVdaError, base::Unretained(this)));

  DLOG(INFO) << "Created new GPU context";
}

void GpuVdaContext::Initialize(InitializeCallback callback) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  arc::mojom::VideoDecodeClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));
  binding_.set_connection_error_with_reason_handler(
      base::Bind(&GpuVdaContext::OnVdaClientError, base::Unretained(this)));

  arc::mojom::VideoDecodeAcceleratorConfigPtr config =
      arc::mojom::VideoDecodeAcceleratorConfig::New();
  config->secure_mode = false;
  config->profile = arc::mojom::VideoCodecProfile::H264PROFILE_MAIN;

  vda_ptr_->Initialize(
      std::move(config), std::move(client_ptr),
      base::Bind(&GpuVdaContext::OnInitialized, base::Unretained(this),
                 base::Passed(std::move(callback))));
}

void GpuVdaContext::OnInitialized(
    InitializeCallback callback,
    arc::mojom::VideoDecodeAccelerator::Result result) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  std::move(callback).Run(ConvertResult(result));
}

GpuVdaContext::~GpuVdaContext() {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
}

void GpuVdaContext::OnVdaError(uint32_t custom_reason,
                               const std::string& description) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DLOG(ERROR) << "VideoDecodeAccelerator mojo connection error. custom_reason="
              << custom_reason << " description=" << description;
}

void GpuVdaContext::OnVdaClientError(uint32_t custom_reason,
                                     const std::string& description) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DLOG(ERROR) << "VideoDecodeClient mojo connection error. custom_reason="
              << custom_reason << " description=" << description;
}

vda_result_t GpuVdaContext::Decode(int32_t bitstream_id,
                                   int fd,
                                   uint32_t offset,
                                   uint32_t bytes_used) {
  // TODO(alexlau): Implement this.
  DLOG(INFO) << "TODO: GpuVdaContext::Decode";
  NOTIMPLEMENTED();
  return PLATFORM_FAILURE;
}

vda_result_t GpuVdaContext::SetOutputBufferCount(size_t num_output_buffers) {
  // TODO(alexlau): Implement this.
  DLOG(INFO) << "TODO: GpuVdaContext::SetOutputBufferCount";
  NOTIMPLEMENTED();
  return PLATFORM_FAILURE;
}

vda_result_t GpuVdaContext::UseOutputBuffer(int32_t picture_buffer_id,
                                            vda_pixel_format_t format,
                                            int fd,
                                            size_t num_planes,
                                            video_frame_plane_t* planes) {
  // TODO(alexlau): Implement this.
  DLOG(INFO) << "TODO: GpuVdaContext::UseOutputBuffer";
  NOTIMPLEMENTED();
  return PLATFORM_FAILURE;
}

vda_result GpuVdaContext::Reset() {
  // TODO(alexlau): Implement this.
  DLOG(INFO) << "TODO: GpuVdaContext::Reset";
  NOTIMPLEMENTED();
  DispatchResetResponse(PLATFORM_FAILURE);
  return PLATFORM_FAILURE;
}

vda_result GpuVdaContext::Flush() {
  // TODO(alexlau): Implement this.
  DLOG(INFO) << "TODO: GpuVdaContext::Flush";
  NOTIMPLEMENTED();
  DispatchFlushResponse(PLATFORM_FAILURE);
  return PLATFORM_FAILURE;
}

// VideoDecodeClient implementation function.
void GpuVdaContext::ProvidePictureBuffers(
    arc::mojom::PictureBufferFormatPtr format_ptr) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchProvidePictureBuffers(format_ptr->min_num_buffers,
                                format_ptr->coded_size->width,
                                format_ptr->coded_size->height);
}

// VideoDecodeClient implementation function.
void GpuVdaContext::PictureReady(arc::mojom::PicturePtr picture_ptr) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchPictureReady(
      picture_ptr->picture_buffer_id, picture_ptr->bitstream_id,
      picture_ptr->crop_rect->left, picture_ptr->crop_rect->top,
      picture_ptr->crop_rect->right, picture_ptr->crop_rect->bottom);
}

// VideoDecodeClient implementation function.
void GpuVdaContext::NotifyError(
    arc::mojom::VideoDecodeAccelerator::Result error) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchNotifyError(ConvertResult(error));
}

// VideoDecodeClient implementation function.
void GpuVdaContext::NotifyEndOfBitstreamBuffer(int32_t bitstream_id) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchNotifyEndOfBitstreamBuffer(bitstream_id);
}

}  // namespace

// static
GpuVdaImpl* GpuVdaImpl::Create() {
  if (active_impl) {
    // A instantiated GpuVdaImpl object already exists, return nullptr.
    return nullptr;
  }

  auto impl = std::make_unique<GpuVdaImpl>();
  if (!impl->Initialize()) {
    LOG(ERROR) << "Could not initialize GpuVdaImpl.";
    return nullptr;
  }

  return impl.release();
}

GpuVdaImpl::GpuVdaImpl() : ipc_thread_("MojoIpcThread") {
  CHECK_EQ(active_impl, nullptr);
  active_impl = this;

  mojo::edk::Init();
  CHECK(ipc_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  mojo::edk::InitIPCSupport(ipc_thread_.task_runner());
}

GpuVdaImpl::~GpuVdaImpl() {
  RunTaskOnThread(
      ipc_thread_.task_runner(),
      base::BindOnce(&GpuVdaImpl::CleanupOnIpcThread, base::Unretained(this)));
  mojo::edk::ShutdownIPCSupport(base::Bind(&base::DoNothing));

  CHECK_EQ(active_impl, this);
  active_impl = nullptr;
}

bool GpuVdaImpl::Initialize() {
  bool init_success = false;
  RunTaskOnThread(ipc_thread_.task_runner(),
                  base::Bind(&GpuVdaImpl::InitializeOnIpcThread,
                             base::Unretained(this), &init_success));
  // TODO(alexlau): Populate capabilities_.
  return init_success;
}

void GpuVdaImpl::InitializeOnIpcThread(bool* init_success) {
  // Since ipc_thread_checker_ binds to whichever thread it's created on, check
  // that we're on the correct thread first using BelongsToCurrentThread.
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  // TODO(alexlau): Use DETACH_FROM_THREAD macro after libchrome uprev
  // (crbug.com/909719).
  ipc_thread_checker_.DetachFromThread();
  // TODO(alexlau): Use DCHECK_CALLED_ON_VALID_THREAD macro after libchrome
  // uprev (crbug.com/909719).
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(std::move(opts));
  if (!bus->Connect()) {
    DLOG(ERROR) << "Failed to connect to system bus";
    return;
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      libvda::kLibvdaServiceName, dbus::ObjectPath(libvda::kLibvdaServicePath));
  if (!proxy) {
    // TODO(alexlau): Would this ever start before Chrome such that we should
    //                call WaitForServiceToBeAvailable here?
    DLOG(ERROR) << "Unable to get dbus proxy for "
                << libvda::kLibvdaServiceName;
    return;
  }

  dbus::MethodCall method_call(libvda::kLibvdaServiceInterface,
                               libvda::kProvideMojoConnectionMethod);
  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response.get()) {
    DLOG(ERROR) << "Unable to get response from method call "
                << libvda::kProvideMojoConnectionMethod;
    return;
  }

  dbus::MessageReader reader(response.get());

  // Read the mojo pipe FD.
  base::ScopedFD fd;
  if (!reader.PopFileDescriptor(&fd)) {
    DLOG(ERROR) << "Unable to read mojo pipe fd";
    return;
  }
  if (!fd.is_valid()) {
    DLOG(ERROR) << "Received invalid mojo pipe fd";
    return;
  }

  std::string pipe_name;
  if (!reader.PopString(&pipe_name)) {
    DLOG(ERROR) << "Unable to read mojo pipe name.";
    return;
  }

  // Setup the mojo pipe.
  mojo::edk::SetParentPipeHandle(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(fd.release())));
  mojo::ScopedMessagePipeHandle scoped_message_pipe_handle =
      mojo::edk::CreateChildMessagePipe(pipe_name);
  mojo::InterfacePtrInfo<arc::mojom::VideoAcceleratorFactory>
      interface_ptr_info(std::move(scoped_message_pipe_handle),
                         kRequiredVideoAcceleratorFactoryMojoVersion);
  vda_factory_ptr_ = mojo::MakeProxy(std::move(interface_ptr_info));
  vda_factory_ptr_.set_connection_error_with_reason_handler(
      base::Bind(&GpuVdaImpl::OnVdaFactoryError, base::Unretained(this)));

  *init_success = true;
}

void GpuVdaImpl::OnVdaFactoryError(uint32_t custom_reason,
                                   const std::string& description) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DLOG(ERROR)
      << "VideoDecodeAcceleratorFactory mojo connection error. custom_reason="
      << custom_reason << " description=" << description;
}

VdaContext* GpuVdaImpl::InitDecodeSession(vda_profile_t profile) {
  DCHECK(!ipc_thread_checker_.CalledOnValidThread());

  DLOG(INFO) << "Initializing decode session with profile " << profile;

  base::WaitableEvent init_complete_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  VdaContext* context = nullptr;
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuVdaImpl::InitDecodeSessionOnIpcThread,
                                base::Unretained(this), profile,
                                &init_complete_event, &context));

  init_complete_event.Wait();

  return context;
}

void GpuVdaImpl::InitDecodeSessionOnIpcThread(
    vda_profile_t profile,
    base::WaitableEvent* init_complete_event,
    VdaContext** out_context) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  arc::mojom::VideoDecodeAcceleratorPtr vda_ptr;
  vda_factory_ptr_->CreateDecodeAccelerator(mojo::MakeRequest(&vda_ptr));

  std::unique_ptr<GpuVdaContext> context = std::make_unique<GpuVdaContext>(
      ipc_thread_.task_runner(), std::move(vda_ptr));
  GpuVdaContext* context_ptr = context.get();
  context_ptr->Initialize(base::Bind(
      &GpuVdaImpl::InitDecodeSessionAfterContextInitializedOnIpcThread,
      base::Unretained(this), init_complete_event, out_context,
      base::Passed(std::move(context))));
}

void GpuVdaImpl::InitDecodeSessionAfterContextInitializedOnIpcThread(
    base::WaitableEvent* init_complete_event,
    VdaContext** out_context,
    std::unique_ptr<VdaContext> context,
    vda_result_t result) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  if (result == SUCCESS) {
    *out_context = context.release();
  } else {
    DLOG(ERROR) << "Failed to initialize decode session.";
  }
  init_complete_event->Signal();
}

void GpuVdaImpl::CloseDecodeSession(VdaContext* context) {
  DLOG(INFO) << "Closing decode session";
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuVdaImpl::CloseDecodeSessionOnIpcThread,
                                base::Unretained(this), context));
}

void GpuVdaImpl::CloseDecodeSessionOnIpcThread(VdaContext* context) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  delete context;
}

void GpuVdaImpl::CleanupOnIpcThread() {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  if (vda_factory_ptr_.is_bound())
    vda_factory_ptr_.reset();
}

}  // namespace arc
