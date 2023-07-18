/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SASL_XOAUTH2_LOG_H
#define SASL_XOAUTH2_LOG_H

#include <memory>
#include <string>
#include <vector>

namespace sasl_xoauth2 {

void EnableLoggingForTesting();

// Log implementation interface, not for direct use.
class LogImpl {
 public:
  virtual ~LogImpl() = default;

  virtual void WriteLine(const std::string &line) = 0;
};

class Log {
 public:
  enum Options {
    OPTIONS_NONE = 0,
    OPTIONS_IMMEDIATE = 1,
    OPTIONS_FULL_TRACE_ON_FAILURE = 2,
    OPTIONS_FLUSH_ON_DESTROY = 4,
  };

  enum Target {
    TARGET_DEFAULT = 0,
    TARGET_NONE = 1,
    TARGET_SYSLOG = 2,
    TARGET_STDERR = 3,
  };

  static std::unique_ptr<Log> Create(Options options = OPTIONS_NONE,
                                     Target target = TARGET_DEFAULT);

  ~Log();

  void Write(const char *fmt, ...);
  void Flush();
  void SetFlushOnDestroy();

 protected:
  Log(std::unique_ptr<LogImpl> impl, Options options)
      : impl_(std::move(impl)), options_(options) {}

 private:
  const std::unique_ptr<LogImpl> impl_;

  Options options_;
  std::string summary_;
  std::vector<std::string> lines_;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_LOG_H
