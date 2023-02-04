/*
 * Copyright (c) 2023, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include "appender.hpp"

namespace easylog {
class logger {
 public:
  static logger &instance() {
    static logger instance;
    return instance;
  }

  void operator+=(const record_t &record) { write(record); }

  void write(const record_t &record) {
    if (record.get_severity() < min_severity_) {
      return;
    }

    append_format(record);

    if (record.get_severity() == Severity::CRITICAL) {
      flush();
      std::exit(EXIT_FAILURE);
    }
  }

  void flush() {
    if (appender_) {
      appender_->flush();
    }
  }

  void init(Severity min_severity, bool enable_console,
            const std::string &filename, size_t max_file_size, size_t max_files,
            bool flush_every_time) {
    static appender appender(filename, max_file_size, max_files,
                             flush_every_time);
    appender_ = &appender;
    min_severity_ = min_severity;
    enable_console_ = enable_console;
  }

  bool check_severity(Severity severity) { return severity >= min_severity_; }

 private:
  logger() = default;
  logger(const logger &) = default;

  template <size_t N>
  size_t get_time_str(char (&buf)[N], const auto &now) {
    const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count() %
                       1000;

    size_t endpos =
        std::strftime(buf, N, "%Y-%m-%d %H:%M:%S", std::localtime(&nowAsTimeT));
    snprintf(buf + endpos, N - endpos, ".%03d", (int)nowMs);
    return endpos + 4;
  }

  void append_format(const record_t &record) {
    char buf[32];
    size_t len = get_time_str(buf, record.get_time_point());

#if __has_include(<memory_resource>)
    char arr[1024];
    std::pmr::monotonic_buffer_resource resource(arr, 1024);
    std::pmr::string str{&resource};
#else
    std::string str;
#endif
    str.append(buf, len).append(" ");
    str.append(severity_str(record.get_severity())).append(" ");

    auto [ptr, ec] = std::to_chars(buf, buf + 32, record.get_tid());

    str.append("[").append(std::string_view(buf, ptr - buf)).append("] ");
    str.append(record.get_file_str());
    str.append(record.get_message()).append("\n");

    if (appender_) {
      appender_->write(str);
    }

    if (enable_console_) {
      std::cout << str;
      std::cout << std::flush;
    }
  }

  Severity min_severity_;
  bool enable_console_ = true;
  appender *appender_ = nullptr;
};

inline void init_log(Severity min_severity, const std::string &filename = "",
                     bool enable_console = true, size_t max_file_size = 0,
                     size_t max_files = 0, bool flush_every_time = false) {
  logger::instance().init(min_severity, enable_console, filename, max_file_size,
                          max_files, flush_every_time);
}

inline void flush() { logger::instance().flush(); }
}  // namespace easylog

#define ELOG_IMPL(severity)                                           \
  if (!easylog::logger::instance().check_severity(severity)) {        \
    ;                                                                 \
  }                                                                   \
  else                                                                \
    easylog::logger::instance() +=                                    \
        easylog::record_t(std::chrono::system_clock::now(), severity, \
                          GET_STRING(__FILE__, __LINE__))             \
            .ref()

#define ELOG(severity) ELOG_IMPL(Severity::severity)

#define ELOGV_IMPL(severity, fmt, ...)                                \
  if (!easylog::logger::instance().check_severity(severity)) {        \
    ;                                                                 \
  }                                                                   \
  else {                                                              \
    easylog::logger::instance() +=                                    \
        easylog::record_t(std::chrono::system_clock::now(), severity, \
                          GET_STRING(__FILE__, __LINE__))             \
            .sprintf(fmt, __VA_ARGS__);                               \
    if (severity == Severity::CRITICAL) {                             \
      easylog::flush();                                               \
      std::exit(EXIT_FAILURE);                                        \
    }                                                                 \
  }

#define ELOGV(severity, ...) ELOGV_IMPL(Severity::severity, __VA_ARGS__, "\n")

#define ELOG_TRACE ELOG(INFO)
#define ELOG_DEBUG ELOG(DEBUG)
#define ELOG_INFO ELOG(INFO)
#define ELOG_WARN ELOG(WARN)
#define ELOG_ERROR ELOG(ERROR)
#define ELOG_CRITICAL ELOG(CRITICAL)

#define ELOGT ELOG_TRACE
#define ELOGD ELOG_DEBUG
#define ELOGI ELOG_INFO
#define ELOGW ELOG_WARN
#define ELOGE ELOG_ERROR
#define ELOGC ELOG_CRITICAL