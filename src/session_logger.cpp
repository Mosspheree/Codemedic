#include "session_logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

std::string SessionLogger::now_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

SessionLogger::SessionLogger(const Config& cfg)
    : cfg_(cfg), session_start_(now_iso8601()) {}

void SessionLogger::record_fix(const FixRecord& record) {
    records_.push_back(record);
}

void SessionLogger::set_session_end() {
    session_end_ = now_iso8601();
}

int SessionLogger::total_applied() const {
    int n = 0;
    for (auto& r : records_) if (r.applied) ++n;
    return n;
}

int SessionLogger::total_verified() const {
    int n = 0;
    for (auto& r : records_) if (r.verified) ++n;
    return n;
}

double SessionLogger::total_duration_ms() const {
    double total = 0;
    for (auto& r : records_) total += r.duration_ms;
    return total;
}

bool SessionLogger::write_log() const {
    if (cfg_.log_file.empty()) return false;

    json log;

    // Session metadata
    log["session"] = {
        {"start",    session_start_},
        {"end",      session_end_.empty() ? now_iso8601() : session_end_},
        {"provider", cfg_.provider_name()},
        {"model",    cfg_.resolved_model()},
        {"compiler", cfg_.compiler}
    };

    // Fix records
    json fixes = json::array();
    for (auto& r : records_) {
        fixes.push_back({
            {"file",          r.file},
            {"line",          r.line},
            {"error",         r.error_message},
            {"explanation",   r.explanation},
            {"patch_summary", r.patch_summary},
            {"applied",       r.applied},
            {"verified",      r.verified},
            {"duration_ms",   r.duration_ms},
            {"timestamp",     r.timestamp}
        });
    }
    log["fixes"] = fixes;

    // Summary
    log["summary"] = {
        {"total_attempts",  total_attempts()},
        {"total_applied",   total_applied()},
        {"total_verified",  total_verified()},
        {"total_duration_ms", total_duration_ms()}
    };

    std::ofstream f(cfg_.log_file);
    if (!f) return false;
    f << log.dump(2) << "\n";
    return true;
}
