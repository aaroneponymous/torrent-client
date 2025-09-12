#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace bittorrent::logger {

    enum class LogLevel : uint8_t { trace=0, debug=1, info=2, warn=3, error=4, none=255 };

    struct LogRecord 
    {
        LogLevel level{LogLevel::info};
        std::chrono::system_clock::time_point ts{};
        std::string logger;        // e.g. "TrackerManager", "HttpTracker"
        std::string msg;           // rendered text
        
        // Optional structured fields:
        std::string url;           // tracker URL (redacted)
        std::string tier;          // e.g. "0/2"
        std::string endpoint;      // ip:port or host:port
        std::string event;         // "started|completed|stopped|none"
        int         httpStatus{-1};
        int         retries{-1};
        int         interval{-1};
    };

    class ILoggerSink 
    {
    public:
        virtual ~ILoggerSink() = default;
        virtual void write(const LogRecord& rec) = 0;
    };

    class StdoutSink : public ILoggerSink 
    {
    public:
        void write(const LogRecord& rec) override;
    };

    class FileSink : public ILoggerSink 
    {
    public:
        explicit FileSink(const std::string& path);
        void write(const LogRecord& rec) override;
    private:
        std::mutex mu_;
        std::ofstream out_;
    };

    class Logger 
    {
    public:
        using RedactorFn = std::function<std::string(std::string_view)>;

        explicit Logger(std::shared_ptr<ILoggerSink> sink = std::make_shared<StdoutSink>())
        : sink_(std::move(sink)) {}

        void setLevel(LogLevel lvl) { level_.store(lvl, std::memory_order_relaxed); }
        LogLevel level() const { return level_.load(std::memory_order_relaxed); }

        void setRedactor(RedactorFn r) { std::scoped_lock lk(mu_); redactor_ = std::move(r); }

        // Core log function (printf-free; build string up front)

        void log(LogRecord rec) {
            if (static_cast<unsigned>(rec.level) < static_cast<unsigned>(level())) return;
            rec.ts = std::chrono::system_clock::now();
            if (redactor_) {
                rec.url = redactor_(rec.url);
                rec.msg = redactor_(rec.msg);
            }
            sink_->write(rec);
        }

        void trace(std::string msg, std::string logger = {}) { emit(LogLevel::trace, std::move(msg), std::move(logger)); }
        void debug(std::string msg, std::string logger = {}) { emit(LogLevel::debug, std::move(msg), std::move(logger)); }
        void info (std::string msg, std::string logger = {}) { emit(LogLevel::info , std::move(msg), std::move(logger)); }
        void warn (std::string msg, std::string logger = {}) { emit(LogLevel::warn , std::move(msg), std::move(logger)); }
        void error(std::string msg, std::string logger = {}) { emit(LogLevel::error, std::move(msg), std::move(logger)); }

    private:
        void emit(LogLevel lvl, std::string msg, std::string logger) {
            LogRecord rec;
            rec.level = lvl;
            rec.msg   = std::move(msg);
            rec.logger= std::move(logger);
            log(std::move(rec));
        }

        std::mutex mu_;
        std::shared_ptr<ILoggerSink> sink_;
        std::atomic<LogLevel> level_{LogLevel::info};
        RedactorFn redactor_;
    };

    // Optional macros to capture file:line without fmtlib
    #ifndef BT_TRACKER_LOG_LEVEL
    #define BT_TRACKER_LOG_LEVEL bittorrent::logger::LogLevel::info
    #endif

    #define BT_LOG_ENABLED(lvl) (static_cast<unsigned>(lvl) >= static_cast<unsigned>(BT_TRACKER_LOG_LEVEL))

    // Usage: BT_LOG(loggerPtr, LogLevel::debug) << "message " << x;
    #define BT_LOG(LOGGER_PTR, LVL) \
        if (!(LOGGER_PTR) || !BT_LOG_ENABLED(LVL)) ; \
        else ::bittorrent::logger::detail::LogStreamHelper(*(LOGGER_PTR), (LVL), __FILE__, __LINE__, __func__).stream()

    namespace detail {
        class LogStreamHelper 
        {
        public:
            LogStreamHelper(Logger& lg, LogLevel lvl, const char* file, int line, const char* fn)
            : lg_(lg) { ss_ << "[" << fn << ":" << line << "] "; rec_.level = lvl; }
            ~LogStreamHelper() {
                rec_.msg = ss_.str();
                rec_.logger = "tracker";
                lg_.log(std::move(rec_));
            }
            std::ostream& stream() { return ss_; }
            LogRecord rec_;
        private:
            Logger& lg_;
            std::ostringstream ss_;
        };
    }

} // namespace bittorrent::logger
