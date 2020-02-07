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
bool s_test_mode = false;
}  // namespace

void EnableLoggingForTesting() { s_test_mode = true; }

Log::~Log() {
  if (flush_on_destroy_ && !lines_.empty()) Flush();
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

  if (s_test_mode) {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
}

void Log::Flush() {
  if (s_test_mode) {
    fprintf(stderr, "LOGGING: skipping write of %zu line(s).\n", lines_.size());
    return;
  }

  if (!Config::Get()->log_to_syslog_on_failure()) return;

  openlog("sasl-xoauth2", 0, 0);
  if (Config::Get()->log_full_trace_on_failure()) {
    syslog(LOG_WARNING, "auth failed:\n");
    for (const auto &line : lines_) syslog(LOG_WARNING, "  %s\n", line.c_str());
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
}

void Log::SetFlushOnDestroy() {
  flush_on_destroy_ = true;
  if (!lines_.empty()) summary_ = lines_.back();
}

}  // namespace sasl_xoauth2
