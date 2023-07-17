// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>

#include <memory>

namespace sasl_xoauth2 {

namespace {

Log::Options s_default_options = Log::OPTIONS_NONE;
Log::Target s_default_target = Log::TARGET_SYSLOG;

std::string Now() {
  time_t t = time(nullptr);
  char time_str[32];
  tm local_time = {};
  localtime_r(&t, &local_time);
  strftime(time_str, sizeof(time_str), "%F %T", &local_time);
  return std::string(time_str);
}

class NoOpLogger : public Log {
 public:
  NoOpLogger(Options options) : Log(options) {}
  ~NoOpLogger() override = default;

 protected:
  void WriteLine(const std::string &line) override {}
};

class SysLogLogger : public Log {
 public:
  SysLogLogger(Options options) : Log(options) {
    openlog("sasl-xoauth2", 0, 0);
  }

  ~SysLogLogger() override { closelog(); }

 protected:
  void WriteLine(const std::string &line) override {
    syslog(LOG_WARNING, "%s\n", line.c_str());
  }
};

class StdErrLogger : public Log {
 public:
  StdErrLogger(Options options) : Log(options) {}
  ~StdErrLogger() override = default;

 protected:
  void WriteLine(const std::string &line) override {
    fprintf(stderr, "%s\n", line.c_str());
  }
};

}  // namespace

void EnableLoggingForTesting() {
  s_default_options = Log::OPTIONS_IMMEDIATE;
  s_default_target = Log::TARGET_STDERR;
}

std::unique_ptr<Log> Log::Create(Options options, Target target) {
  options = static_cast<Options>(options | s_default_options);
  if (target == TARGET_DEFAULT) target = s_default_target;
  switch (target) {
    case TARGET_NONE:
      return std::make_unique<NoOpLogger>(options);
    case TARGET_SYSLOG:
      return std::make_unique<SysLogLogger>(options);
    case TARGET_STDERR:
      return std::make_unique<StdErrLogger>(options);
    default:
      return {};
  };
}

Log::~Log() {
  if (options_ & OPTIONS_FLUSH_ON_DESTROY) Flush();
}

void Log::Write(const char *fmt, ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  const std::string line = buf;
  if (options_ & OPTIONS_IMMEDIATE) {
    WriteLine(line);
  } else {
    lines_.push_back(Now() + ": " + line);
  }
}

void Log::Flush() {
  if (lines_.empty()) return;

  if (options_ & OPTIONS_FULL_TRACE_ON_FAILURE) {
    WriteLine("auth failed:");
    for (const auto &line : lines_) WriteLine("  " + line);
  } else {
    if (summary_.empty()) summary_ = lines_.back();
    WriteLine("auth failed: " + summary_);
    if (lines_.size() > 1) {
      WriteLine("set log_full_trace_on_failure to see full " +
                std::to_string(lines_.size()) + " line(s) of tracing.");
    }
  }
}

void Log::SetFlushOnDestroy() {
  options_ = static_cast<Options>(options_ | OPTIONS_FLUSH_ON_DESTROY);
  if (!lines_.empty()) summary_ = lines_.back();
}

}  // namespace sasl_xoauth2
