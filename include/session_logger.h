#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <chrono>

/**
 * SessionLogger
 *
 * Records every fix attempt to a structured JSON log file.
 * Useful for auditing what was changed, tracking success rates,
 * and integrating with CI/CD pipelines.
 *
 * Log format:
 * {
 *   "session": { "start": "...", "end": "...", "provider": "...", "model": "..." },
 *   "fixes": [ { "file": "...", "line": N, ... }, ... ],
 *   "summary": { "total": N, "applied": N, "verified": N }
 * }
 */
class SessionLogger {
public:
    explicit SessionLogger(const Config& cfg);

    void record_fix(const FixRecord& record);
    void set_session_end();
    bool write_log() const;

    // Accessors for summary
    int total_attempts() const { return (int)records_.size(); }
    int total_applied() const;
    int total_verified() const;
    double total_duration_ms() const;

    // Public utility — used for timestamping individual records
    static std::string now_iso8601();

private:
    const Config& cfg_;
    std::vector<FixRecord> records_;
    std::string session_start_;
    std::string session_end_;
};
