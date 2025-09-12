#define BT_TRACKER_LOG_LEVEL bittorrent::logger::LogLevel::debug
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include "../logger.hpp"


using namespace bittorrent::logger;

// ---------------- Test Sink (in-memory capture) ----------------

class TestSink : public ILoggerSink 
{
public:
    void write(const LogRecord& rec) override {
        records.push_back(rec);

        // Also store a rendered approximation for convenience checks
        std::ostringstream oss;
        oss << '[' << static_cast<int>(rec.level) << "] "
            << (rec.logger.empty() ? "tracker" : rec.logger)
            << ": " << rec.msg;
        if (!rec.url.empty())      oss << " url="      << rec.url;
        if (!rec.tier.empty())     oss << " tier="     << rec.tier;
        if (!rec.endpoint.empty()) oss << " endpoint=" << rec.endpoint;
        if (!rec.event.empty())    oss << " event="    << rec.event;
        if (rec.httpStatus >= 0)   oss << " http="     << rec.httpStatus;
        if (rec.retries >= 0)      oss << " retries="  << rec.retries;
        if (rec.interval >= 0)     oss << " interval=" << rec.interval;
        lines.push_back(oss.str());
    }

    std::vector<LogRecord> records;
    std::vector<std::string> lines;
};

// ---------------- Helpers ----------------

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

// ---------------- Tests ----------------

TEST_CASE("Logger: level threshold filters messages") {
    auto sink = std::make_shared<TestSink>();
    Logger log{sink};

    log.setLevel(LogLevel::info);
    log.debug("debug should be filtered", "L1");
    log.info("info should pass", "L1");
    log.warn("warn should pass", "L1");
    log.error("error should pass", "L1");

    REQUIRE(sink->records.size() == 3);
    CHECK(contains(sink->lines[0], "info should pass"));
    CHECK(contains(sink->lines[1], "warn should pass"));
    CHECK(contains(sink->lines[2], "error should pass"));
}

TEST_CASE("Logger: redactor is applied to url and msg") {
    auto sink = std::make_shared<TestSink>();
    Logger log{sink};
    log.setLevel(LogLevel::debug);

    log.setRedactor([](std::string_view s) {
        std::string in{s};
        auto pos = in.find("secret");
        if (pos != std::string::npos) {
            in.replace(pos, 6, "******");
        }
        return in;
    });

    LogRecord rec;
    rec.level = LogLevel::info;
    rec.logger = "HttpTracker";
    rec.msg = "token=secret&ok=1";
    rec.url = "http://tracker/?pass=secret";
    log.log(std::move(rec));

    REQUIRE(sink->records.size() == 1);
    CHECK(contains(sink->lines[0], "token=******"));
    CHECK(contains(sink->lines[0], "url=http://tracker/?pass=******"));
}

TEST_CASE("Logger: BT_LOG macro routes message via LogStreamHelper") {
    auto sink = std::make_shared<TestSink>();
    Logger log{sink};
    log.setLevel(LogLevel::debug);

    // Compose via stream; destructor should dispatch
    BT_LOG(&log, LogLevel::debug) << "hello " << 42;

    REQUIRE(sink->records.size() == 1);
    // The macro prefixes "[func:line]" but we only check payload is present
    CHECK(contains(sink->records[0].msg, "hello 42"));
    CHECK(sink->records[0].level == LogLevel::debug);
    CHECK(sink->records[0].logger == "tracker"); // set by helper
}

TEST_CASE("StdoutSink: one line formatted and written to std::cout") {
    // Capture std::cout for this scope
    std::ostringstream cap;
    auto* old = std::cout.rdbuf(cap.rdbuf());

    StdoutSink sink;
    LogRecord rec;
    rec.level = LogLevel::warn;
    rec.logger = "TrackerManager";
    rec.msg = "re-announce scheduled";
    rec.url = "http://t/ann";
    rec.tier = "0/2";
    rec.endpoint = "1.2.3.4:80";
    rec.event = "started";
    rec.httpStatus = 200;
    rec.retries = 1;
    rec.interval = 1800;

    sink.write(rec);

    // Restore cout
    std::cout.rdbuf(old);

    const std::string out = cap.str();
    // Timestamp is variable; check stable parts
    CHECK(contains(out, " [WARN] "));
    CHECK(contains(out, "TrackerManager: re-announce scheduled"));
    CHECK(contains(out, "url=http://t/ann"));
    CHECK(contains(out, "tier=0/2"));
    CHECK(contains(out, "endpoint=1.2.3.4:80"));
    CHECK(contains(out, "event=started"));
    CHECK(contains(out, "http=200"));
    CHECK(contains(out, "retries=1"));
    CHECK(contains(out, "interval=1800"));
    // ends with newline
    REQUIRE(!out.empty());
    CHECK(out.back() == '\n');
}

TEST_CASE("FileSink: appends lines atomically under mutex") {
    // Create a temp file in the build/test dir
    auto tmp = std::filesystem::temp_directory_path() / ("tracker_log_" + std::to_string(::getpid()) + ".log");

    {
        FileSink sink(tmp.string());
        LogRecord a; a.level = LogLevel::info;  a.logger = "A"; a.msg = "first";
        LogRecord b; b.level = LogLevel::error; b.logger = "B"; b.msg = "second";
        sink.write(a);
        sink.write(b);
    }

    const std::string content = read_file(tmp);
    // We don't check timestamp, only the payload and formatting markers
    CHECK(contains(content, " [INFO] A: first"));
    CHECK(contains(content, " [ERROR] B: second"));

    // Clean up
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

TEST_CASE("Logger: structured fields pass through log(LogRecord)") {
    auto sink = std::make_shared<TestSink>();
    Logger log{sink};
    log.setLevel(LogLevel::debug);

    LogRecord rec;
    rec.level = LogLevel::debug;
    rec.logger = "HttpTracker";
    rec.msg = "announce ok";
    rec.url = "http://tracker";
    rec.tier = "1/2";
    rec.endpoint = "host:6969";
    rec.event = "none";
    rec.httpStatus = 200;
    rec.retries = 0;
    rec.interval = 900;

    log.log(std::move(rec));

    REQUIRE(sink->records.size() == 1);
    const auto& line = sink->lines[0];
    CHECK(contains(line, "HttpTracker: announce ok"));
    CHECK(contains(line, "url=http://tracker"));
    CHECK(contains(line, "tier=1/2"));
    CHECK(contains(line, "endpoint=host:6969"));
    CHECK(contains(line, "event=none"));
    CHECK(contains(line, "http=200"));
    CHECK(contains(line, "retries=0"));
    CHECK(contains(line, "interval=900"));
}
