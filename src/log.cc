#include "log.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace sasl_xoauth2 {

namespace {

std::string GenerateFileName() {
  timeval t = {};
  gettimeofday(&t, nullptr);
  const uint64_t time_ms = t.tv_sec * 1000 + t.tv_usec / 1000;

  char buf[128];
  snprintf(buf, sizeof(buf), "%d.%" PRIu64, getpid(), time_ms);

  return "/tmp/sasl_xoauth2." + std::string(buf) + ".log";
}

bool s_test_mode = false;

}  // namespace

void EnableLoggingForTesting() { s_test_mode = true; }

Log::~Log() {
  if (flush_on_destroy_) FlushToDisk();
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

void Log::FlushToDisk() {
  const std::string path = GenerateFileName();

  if (s_test_mode) {
    fprintf(stderr, "LOGGING: skipping write to %s\n", path.c_str());
    return;
  }

  FILE *f = fopen(path.c_str(), "w");
  if (!f) return;

  for (const auto &line : lines_) {
    fprintf(f, "%s\n", line.c_str());
  }

  fclose(f);
}

void Log::SetFlushOnDestroy() { flush_on_destroy_ = true; }

}  // namespace sasl_xoauth2
