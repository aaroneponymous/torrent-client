// logger.cpp
#include "logger.hpp"
#include <iostream>
#include <syncstream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace bittorrent::logger {

static const char* level_name(LogLevel l) {
        switch (l) {
            case LogLevel::trace: return "TRACE";
            case LogLevel::debug: return "DEBUG";
            case LogLevel::info:  return "INFO";
            case LogLevel::warn:  return "WARN";
            case LogLevel::error: return "ERROR";
            default:              return "NONE";
        }
    }

    static std::string ts_iso8601(std::chrono::system_clock::time_point tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
        gmtime_r(&t, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    // -------- StdoutSink: atomic per-line emission --------
    void StdoutSink::write(const LogRecord& rec) {
        std::osyncstream out(std::cout);  // per-call buffered; flushes on destruction
        out << ts_iso8601(rec.ts) << " [" << level_name(rec.level) << "] "
            << (rec.logger.empty() ? "tracker" : rec.logger) << ": "
            << rec.msg;

        // Optionally emit selected structured fields when present:
        if (!rec.url.empty())      out << " url="      << rec.url;
        if (!rec.tier.empty())     out << " tier="     << rec.tier;
        if (!rec.endpoint.empty()) out << " endpoint=" << rec.endpoint;
        if (!rec.event.empty())    out << " event="    << rec.event;
        if (rec.httpStatus >= 0)   out << " http="     << rec.httpStatus;
        if (rec.retries >= 0)      out << " retries="  << rec.retries;
        if (rec.interval >= 0)     out << " interval=" << rec.interval;

        out << '\n';
    }

    // -------- FileSink: mutex-serialized writes --------
    FileSink::FileSink(const std::string& path) : out_(path, std::ios::app) {}

    void FileSink::write(const LogRecord& rec) {
        if (!out_) return;
        std::scoped_lock lk(mu_);
        out_ << ts_iso8601(rec.ts) << " [" << level_name(rec.level) << "] "
            << (rec.logger.empty() ? "tracker" : rec.logger) << ": "
            << rec.msg;
        if (!rec.url.empty())      out_ << " url="      << rec.url;
        if (!rec.tier.empty())     out_ << " tier="     << rec.tier;
        if (!rec.endpoint.empty()) out_ << " endpoint=" << rec.endpoint;
        if (!rec.event.empty())    out_ << " event="    << rec.event;
        if (rec.httpStatus >= 0)   out_ << " http="     << rec.httpStatus;
        if (rec.retries >= 0)      out_ << " retries="  << rec.retries;
        if (rec.interval >= 0)     out_ << " interval=" << rec.interval;
        out_ << '\n';
        out_.flush();
    }

} // namespace bittorrent::logger
