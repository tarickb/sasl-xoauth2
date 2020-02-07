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

#include <string>
#include <vector>

namespace sasl_xoauth2 {

void EnableLoggingForTesting();

class Log {
 public:
  Log() = default;
  ~Log();

  void Write(const char *fmt, ...);
  void Flush();
  void SetFlushOnDestroy();

 private:
  bool flush_on_destroy_ = false;
  std::string summary_;
  std::vector<std::string> lines_;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_LOG_H
