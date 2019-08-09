#ifndef SASL_XOAUTH2_LOG_H
#define SASL_XOAUTH2_LOG_H

#include <string>
#include <vector>

namespace sasl_xoauth2 {

void EnableLoggingForTesting();

class Log {
 public:
  Log() = default;
  ~Log();

  void Write(const char *fmt, ...);
  void FlushToDisk();
  void SetFlushOnDestroy();

 private:
  bool flush_on_destroy_ = false;
  std::vector<std::string> lines_;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_LOG_H
