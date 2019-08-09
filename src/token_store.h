#ifndef SASL_XOAUTH2_TOKEN_STORE_H
#define SASL_XOAUTH2_TOKEN_STORE_H

#include <time.h>

#include <memory>
#include <string>

namespace sasl_xoauth2 {

class Log;

class TokenStore {
 public:
  static std::unique_ptr<TokenStore> Create(Log *log, const std::string &path);

  int GetAccessToken(std::string *token);
  int Refresh();

 private:
  TokenStore(Log *log, const std::string &path);

  int Read();
  int Write();

  Log *const log_ = nullptr;
  const std::string path_;

  std::string access_;
  std::string refresh_;
  time_t expiry_ = 0;

  int refresh_attempts_ = 0;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_TOKEN_STORE_H
