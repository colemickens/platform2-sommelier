#include <glog/logging.h>

#define SHILL_LOG_DAEMON 1
#define SHILL_LOG_CONFIG 2
#define SHILL_LOG_EVENT 4
#define SHILL_LOG_DBUS 8
#define SHILL_LOG_MANAGER 16
#define SHILL_LOG_SERVICE 32
#define SHILL_LOG_DEVICE 64
#define SHILL_LOG_ALL ~0

#define SHILL_LOG(level, flags) LOG_IF(level, shill::Log::IsEnabled(flags))

#define SHILL_LOG_FILE "/tmp/shill_daemon.log"

namespace shill {
class Log {
 public:
  static void Enable(uint32_t flags) { flags_ |= flags; }
  static void Disable(uint32_t flags) { flags_ &= ~flags; }
  static inline bool IsEnabled(uint32_t flags) { return (flags_ & flags) != 0; }

 private:
  static uint32_t flags_;
};
}  // namespace shill
