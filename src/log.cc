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

#include "config.h"

namespace sasl_xoauth2 {

namespace {
Log::Options s_default_options = Log::OPTIONS_NONE;
Log::Target s_default_target = Log::TARGET_SYSLOG;
}  // namespace

void EnableLoggingForTesting() {
  s_default_options = Log::OPTIONS_IMMEDIATE;
  s_default_target = Log::TARGET_STDERR;
}

std::unique_ptr<Log> Log::Create(Options options, Target target) {
  options = static_cast<Options>(options | s_default_options);
  if (target == TARGET_DEFAULT) target = s_default_target;
  return std::unique_ptr<Log>(new Log(options, target));
}

Log::~Log() {
  if (options_ & OPTIONS_FLUSH_ON_DESTROY && !lines_.empty()) Flush();
}

void Log::Write(const char *fmt, ...) {
  time_t t = time(nullptr);
  char time_str[32];
  tm local_time = {};
  localtime_r(&t, &local_time);
  strftime(time_str, sizeof(time_str), "%F %T", &local_time);

  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  lines_.push_back(std::string(time_str) + ": " + buf);

  if (options_ & OPTIONS_IMMEDIATE && target_ == TARGET_STDERR) {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
}

void Log::Flush() {
  if (target_ == TARGET_SYSLOG) {
    openlog("sasl-xoauth2", 0, 0);
    if (options_ & OPTIONS_FULL_TRACE_ON_FAILURE) {
      syslog(LOG_WARNING, "auth failed:\n");
      for (const auto &line : lines_)
        syslog(LOG_WARNING, "  %s\n", line.c_str());
    } else {
      if (summary_.empty()) summary_ = lines_.back();
      syslog(LOG_WARNING, "auth failed: %s\n", summary_.c_str());
      if (lines_.size() > 1) {
        syslog(LOG_WARNING,
               "set log_full_trace_on_failure to see full %zu "
               "line(s) of tracing.\n",
               lines_.size());
      }
    }
    closelog();
  } else if (target_ == TARGET_STDERR) {
    if (options_ & OPTIONS_IMMEDIATE) {
      fprintf(stderr, "LOGGING: skipping write of %zu line(s).\n",
              lines_.size());
    } else {
      for (const auto &line : lines_) fprintf(stderr, "%s\n", line.c_str());
    }
  }
}

void Log::SetFlushOnDestroy() {
  options_ = static_cast<Options>(options_ | OPTIONS_FLUSH_ON_DESTROY);
  if (!lines_.empty()) summary_ = lines_.back();
}

}  // namespace sasl_xoauth2
