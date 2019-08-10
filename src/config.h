#ifndef SASL_XOAUTH2_CONFIG_H
#define SASL_XOAUTH2_CONFIG_H

#include <json/json.h>
#include <string>

namespace sasl_xoauth2 {

class Config {
 public:
  static int Init();
  static int InitForTesting(const Json::Value &root);

  static Config *Get();

  std::string client_id() const { return client_id_; }
  std::string client_secret() const { return client_secret_; }
  bool log_to_syslog_on_failure() const { return log_to_syslog_on_failure_; }
  bool log_full_trace_on_failure() const { return log_full_trace_on_failure_; }

 private:
  Config() = default;

  int Init(const Json::Value &root);

  std::string client_id_;
  std::string client_secret_;
  bool log_to_syslog_on_failure_ = false;
  bool log_full_trace_on_failure_ = false;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_CONFIG_H
