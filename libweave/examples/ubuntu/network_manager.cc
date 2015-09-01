// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/examples/ubuntu/network_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/wireless.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <cstdlib>
#include <fstream>

#include <base/bind.h>
#include <weave/task_runner.h>

namespace weave {
namespace examples {

namespace {

int ForkCmd(const std::string& path, const std::vector<std::string>& args) {
  int pid = fork();
  if (pid != 0)
    return pid;

  std::vector<const char*> args_vector;
  args_vector.push_back(path.c_str());
  for (auto& i : args)
    args_vector.push_back(i.c_str());
  args_vector.push_back(nullptr);

  execvp(path.c_str(), const_cast<char**>(args_vector.data()));
  NOTREACHED();
}

class SocketStream : public Stream {
 public:
  explicit SocketStream(TaskRunner* task_runner) : task_runner_{task_runner} {}

  ~SocketStream() { CloseBlocking(nullptr); }

  void RunDelayedTask(const base::Closure& success_callback) {
    success_callback.Run();
  }

  bool ReadAsync(void* buffer,
                 size_t size_to_read,
                 const base::Callback<void(size_t)>& success_callback,
                 const base::Callback<void(const Error*)>& error_callback,
                 ErrorPtr* error) {
    if (socket_fd_ < 0) {
      Error::AddTo(error, FROM_HERE, "socket", "invalid_socket",
                   strerror(errno));
      return false;
    }
    int size_read = recv(socket_fd_, buffer, size_to_read, MSG_DONTWAIT);
    if (size_read > 0) {
      task_runner_->PostDelayedTask(
          FROM_HERE, base::Bind(&SocketStream::RunDelayedTask,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::Bind(success_callback, size_read)),
          {});
      return true;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&SocketStream::ReadAsync),
                     weak_ptr_factory_.GetWeakPtr(), buffer, size_to_read,
                     success_callback, error_callback, nullptr),
          base::TimeDelta::FromMilliseconds(200));
      return true;
    }

    ErrorPtr recv_error;
    Error::AddTo(&recv_error, FROM_HERE, "socket", "socket_recv_failed",
                 strerror(errno));
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(error_callback, base::Owned(recv_error.release())), {});
    return true;
  }

  bool WriteAllAsync(const void* buffer,
                     size_t size_to_write,
                     const base::Closure& success_callback,
                     const base::Callback<void(const Error*)>& error_callback,
                     ErrorPtr* error) {
    if (socket_fd_ < 0) {
      Error::AddTo(error, FROM_HERE, "socket", "invalid_socket",
                   strerror(errno));
      return false;
    }
    const char* buffer_ptr = static_cast<const char*>(buffer);
    do {
      int size_sent = send(socket_fd_, buffer_ptr, size_to_write, 0);
      if (size_sent <= 0) {
        ErrorPtr send_error;
        Error::AddTo(&send_error, FROM_HERE, "socket", "socket_send_failed",
                     strerror(errno));
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(error_callback, base::Owned(send_error.release())), {});
        // Still true as we return error with callback.
        return true;
      }
      size_to_write -= size_sent;
      buffer_ptr += size_sent;
    } while (size_to_write > 0);

    task_runner_->PostDelayedTask(FROM_HERE, success_callback, {});
    return true;
  }
  bool FlushBlocking(ErrorPtr* error) { return true; }

  bool CloseBlocking(ErrorPtr* error) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (socket_fd_ >= 0) {
      close(socket_fd_);
      socket_fd_ = -1;
    }
  }

  void CancelPendingAsyncOperations() {
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  bool Connect(const std::string& host, uint16_t port) {
    std::string service = std::to_string(port);
    addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM};
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result)) {
      LOG(ERROR) << "Failed to resolve host name: " << host;
      return false;
    }
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result_deleter{
        result, &freeaddrinfo};

    for (const addrinfo* info = result; info != nullptr; info = info->ai_next) {
      socket_fd_ =
          socket(info->ai_family, info->ai_socktype, info->ai_protocol);
      if (socket_fd_ < 0)
        continue;

      int flags = fcntl(socket_fd_, F_GETFL, 0);
      if (flags == -1)
        flags = 0;
      fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

      LOG(INFO) << "Connecting...";
      if (connect(socket_fd_, info->ai_addr, info->ai_addrlen) == 0)
        break;  // Success.

      if (errno == EINPROGRESS) {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd_, &write_fds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int select_ret = select(socket_fd_ + 1, NULL, &write_fds, NULL, &tv);
        if (select_ret != -1 && select_ret != 0) {
          break;
        }
      }

      LOG(ERROR) << "Failed to connect";
      CloseBlocking(nullptr);
    }

    return socket_fd_ >= 0;
  }

  int GetFd() const { return socket_fd_; }

 private:
  TaskRunner* task_runner_{nullptr};
  int socket_fd_{-1};

  base::WeakPtrFactory<SocketStream> weak_ptr_factory_{this};
};

class SSLStream : public Stream {
 public:
  explicit SSLStream(TaskRunner* task_runner) : task_runner_{task_runner} {}

  ~SSLStream() { weak_ptr_factory_.InvalidateWeakPtrs(); }

  void RunDelayedTask(const base::Closure& success_callback) {
    success_callback.Run();
  }

  bool ReadAsync(void* buffer,
                 size_t size_to_read,
                 const base::Callback<void(size_t)>& success_callback,
                 const base::Callback<void(const Error*)>& error_callback,
                 ErrorPtr* error) {
    int res = SSL_read(ssl_.get(), buffer, size_to_read);
    if (res > 0) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&SSLStream::RunDelayedTask, weak_ptr_factory_.GetWeakPtr(),
                     base::Bind(success_callback, res)),
          {});
      return true;
    }

    int err = SSL_get_error(ssl_.get(), res);

    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&SSLStream::ReadAsync),
                     weak_ptr_factory_.GetWeakPtr(), buffer, size_to_read,
                     success_callback, error_callback, nullptr),
          base::TimeDelta::FromSeconds(1));
      return true;
    }

    ErrorPtr weave_error;
    Error::AddTo(&weave_error, FROM_HERE, "ssl", "socket_read_failed",
                 "SSL error");
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &SSLStream::RunDelayedTask, weak_ptr_factory_.GetWeakPtr(),
            base::Bind(error_callback, base::Owned(weave_error.release()))),
        {});
    return true;
  }

  bool WriteAllAsync(const void* buffer,
                     size_t size_to_write,
                     const base::Closure& success_callback,
                     const base::Callback<void(const Error*)>& error_callback,
                     ErrorPtr* error) {
    int res = SSL_write(ssl_.get(), buffer, size_to_write);
    if (res > 0) {
      buffer = static_cast<const char*>(buffer) + res;
      size_to_write -= res;
      if (size_to_write == 0) {
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(&SSLStream::RunDelayedTask,
                       weak_ptr_factory_.GetWeakPtr(), success_callback),
            {});
        return true;
      }

      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&SSLStream::WriteAllAsync),
                     weak_ptr_factory_.GetWeakPtr(), buffer, size_to_write,
                     success_callback, error_callback, nullptr),
          base::TimeDelta::FromSeconds(1));

      return true;
    }

    int err = SSL_get_error(ssl_.get(), res);

    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&SSLStream::WriteAllAsync),
                     weak_ptr_factory_.GetWeakPtr(), buffer, size_to_write,
                     success_callback, error_callback, nullptr),
          base::TimeDelta::FromSeconds(1));
      return true;
    }

    ErrorPtr weave_error;
    Error::AddTo(&weave_error, FROM_HERE, "ssl", "socket_write_failed",
                 "SSL error");
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &SSLStream::RunDelayedTask, weak_ptr_factory_.GetWeakPtr(),
            base::Bind(error_callback, base::Owned(weave_error.release()))),
        {});
    return true;
  }

  bool FlushBlocking(ErrorPtr* error) { return true; }

  bool CloseBlocking(ErrorPtr* error) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    return true;
  }

  void CancelPendingAsyncOperations() {
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  bool Init() {
    ctx_.reset(SSL_CTX_new(TLSv1_2_client_method()));
    CHECK(ctx_);
    ssl_.reset(SSL_new(ctx_.get()));

    char endpoint[] = "talk.google.com:5223";
    stream_bio_ = BIO_new_connect(endpoint);
    CHECK(stream_bio_);
    BIO_set_nbio(stream_bio_, 1);

    while (BIO_do_connect(stream_bio_) != 1) {
      CHECK(BIO_should_retry(stream_bio_));
      sleep(1);
    }

    SSL_set_bio(ssl_.get(), stream_bio_, stream_bio_);
    SSL_set_connect_state(ssl_.get());

    for (;;) {
      int res = SSL_do_handshake(ssl_.get());
      if (res) {
        return true;
      }

      res = SSL_get_error(ssl_.get(), res);

      if (res != SSL_ERROR_WANT_READ || res != SSL_ERROR_WANT_WRITE) {
        return false;
      }

      sleep(1);
    }
    return false;
  }

 private:
  TaskRunner* task_runner_{nullptr};
  std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ctx_{nullptr, SSL_CTX_free};
  std::unique_ptr<SSL, decltype(&SSL_free)> ssl_{nullptr, SSL_free};
  BIO* stream_bio_{nullptr};

  base::WeakPtrFactory<SSLStream> weak_ptr_factory_{this};
};

}  // namespace

NetworkImpl::NetworkImpl(TaskRunner* task_runner) : task_runner_{task_runner} {
  SSL_load_error_strings();
  SSL_library_init();

  DisableAccessPoint();
}
NetworkImpl::~NetworkImpl() {
  DisableAccessPoint();
}

void NetworkImpl::AddOnConnectionChangedCallback(
    const OnConnectionChangedCallback& listener) {
  callbacks_.push_back(listener);
}

void NetworkImpl::TryToConnect(const std::string& ssid,
                               const std::string& passphrase,
                               int pid,
                               base::Time until,
                               const base::Closure& on_success) {
  if (pid) {
    int status = 0;
    if (pid == waitpid(pid, &status, WNOWAIT)) {
      int sockf_d = socket(AF_INET, SOCK_DGRAM, 0);
      CHECK_GE(sockf_d, 0) << strerror(errno);

      iwreq wreq = {};
      snprintf(wreq.ifr_name, sizeof(wreq.ifr_name), "wlan0");
      std::string essid(' ', IW_ESSID_MAX_SIZE + 1);
      wreq.u.essid.pointer = &essid[0];
      wreq.u.essid.length = essid.size();
      CHECK_GE(ioctl(sockf_d, SIOCGIWESSID, &wreq), 0) << strerror(errno);
      essid.resize(wreq.u.essid.length);
      close(sockf_d);

      if (ssid == essid) {
        task_runner_->PostDelayedTask(
            FROM_HERE, base::Bind(&NetworkImpl::NotifyNetworkChanged,
                                  weak_ptr_factory_.GetWeakPtr()),
            {});
        return task_runner_->PostDelayedTask(FROM_HERE, on_success, {});
      }
      pid = 0;  // Try again.
    }
  }

  if (pid == 0) {
    pid = ForkCmd("nmcli",
                  {"dev", "wifi", "connect", ssid, "password", passphrase});
  }

  if (base::Time::Now() >= until) {
    task_runner_->PostDelayedTask(FROM_HERE,
                                  base::Bind(&NetworkImpl::NotifyNetworkChanged,
                                             weak_ptr_factory_.GetWeakPtr()),
                                  {});
    return;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NetworkImpl::TryToConnect, weak_ptr_factory_.GetWeakPtr(),
                 ssid, passphrase, pid, until, on_success),
      base::TimeDelta::FromSeconds(1));
}

bool NetworkImpl::ConnectToService(const std::string& ssid,
                                   const std::string& passphrase,
                                   const base::Closure& on_success,
                                   ErrorPtr* error) {
  CHECK(!hostapd_started_);
  if (hostapd_started_) {
    Error::AddTo(error, FROM_HERE, "wifi", "busy", "Running Access Point.");
    return false;
  }

  TryToConnect(ssid, passphrase, 0,
               base::Time::Now() + base::TimeDelta::FromMinutes(1), on_success);
}

NetworkState NetworkImpl::GetConnectionState() const {
  // Forced soft AP.
  return NetworkState::kOffline;

  if (std::system("ping talk.google.com -c 1") == 0)
    return NetworkState::kConnected;

  if (std::system("nmcli dev"))
    return NetworkState::kFailure;

  if (std::system("nmcli dev | grep connecting") == 0)
    return NetworkState::kConnecting;

  return NetworkState::kOffline;
}

void NetworkImpl::EnableAccessPoint(const std::string& ssid) {
  if (hostapd_started_)
    return;

  // Release wlan0 interface.
  CHECK_EQ(0, std::system("nmcli nm wifi off"));
  CHECK_EQ(0, std::system("rfkill unblock wlan"));
  sleep(1);

  std::string hostapd_conf = "/tmp/weave_hostapd.conf";
  {
    std::ofstream ofs(hostapd_conf);
    ofs << "interface=wlan0" << std::endl;
    ofs << "channel=1" << std::endl;
    ofs << "ssid=" << ssid << std::endl;
  }

  CHECK_EQ(0, std::system(("hostapd -B -K " + hostapd_conf).c_str()));
  hostapd_started_ = true;

  for (size_t i = 0; i < 10; ++i) {
    if (0 == std::system("ifconfig wlan0 192.168.76.1/24"))
      break;
    sleep(1);
  }

  std::string dnsmasq_conf = "/tmp/weave_dnsmasq.conf";
  {
    std::ofstream ofs(dnsmasq_conf.c_str());
    ofs << "port=0" << std::endl;
    ofs << "bind-interfaces" << std::endl;
    ofs << "log-dhcp" << std::endl;
    ofs << "dhcp-range=192.168.76.10,192.168.76.100" << std::endl;
    ofs << "interface=wlan0" << std::endl;
    ofs << "dhcp-leasefile=" << dnsmasq_conf << ".leases" << std::endl;
  }

  CHECK_EQ(0, std::system(("dnsmasq --conf-file=" + dnsmasq_conf).c_str()));
  task_runner_->PostDelayedTask(FROM_HERE,
                                base::Bind(&NetworkImpl::NotifyNetworkChanged,
                                           weak_ptr_factory_.GetWeakPtr()),
                                {});
}

void NetworkImpl::DisableAccessPoint() {
  int res = std::system("pkill -f dnsmasq.*/tmp/weave");
  res = std::system("pkill -f hostapd.*/tmp/weave");
  CHECK_EQ(0, std::system("nmcli nm wifi on"));
  hostapd_started_ = false;

  task_runner_->PostDelayedTask(FROM_HERE,
                                base::Bind(&NetworkImpl::NotifyNetworkChanged,
                                           weak_ptr_factory_.GetWeakPtr()),
                                {});
}

void NetworkImpl::NotifyNetworkChanged() {
  bool online = GetConnectionState() == NetworkState::kConnected;
  for (const auto& i : callbacks_)
    i.Run(online);
}

std::unique_ptr<Stream> NetworkImpl::OpenSocketBlocking(const std::string& host,
                                                        uint16_t port) {
  std::unique_ptr<SocketStream> stream{new SocketStream{task_runner_}};
  if (!stream->Connect(host, port))
    return nullptr;
  return std::move(stream);
}

void NetworkImpl::CreateTlsStream(
    std::unique_ptr<Stream> stream,
    const std::string& host,
    const base::Callback<void(std::unique_ptr<Stream>)>& success_callback,
    const base::Callback<void(const Error*)>& error_callback) {
  // Connect to SSL port instead of upgrading to TLS.
  std::unique_ptr<SSLStream> tls_stream{new SSLStream{task_runner_}};

  if (tls_stream->Init()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(success_callback, base::Passed(&tls_stream)), {});
  } else {
    ErrorPtr error;
    Error::AddTo(&error, FROM_HERE, "tls", "tls_init_failed",
                 "Failed to initialize TLS stream.");
  }
}

}  // namespace examples
}  // namespace weave
