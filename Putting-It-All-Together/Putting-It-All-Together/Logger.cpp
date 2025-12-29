#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static std::string TimestampForFilename()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

static std::string TimestampNow()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    // Ensure logs directory exists
    MKDIR("logs");

    std::string runName = "logs/run_" + TimestampForFilename() + ".log";
    ofsRun_.open(runName, std::ios::out | std::ios::trunc);
    ofsLatest_.open("logs/latest.log", std::ios::out | std::ios::trunc);

    if (!ofsRun_.is_open())
    {
        // If file creation failed, fallback silently to latest.log only
        ofsRun_.setstate(std::ios::badbit);
    }
    if (!ofsLatest_.is_open())
    {
        ofsLatest_.setstate(std::ios::badbit);
    }

    Log("Log started");
}

Logger::~Logger()
{
    Log("Log ended");
    if (ofsRun_.is_open()) ofsRun_.close();
    if (ofsLatest_.is_open()) ofsLatest_.close();
}

void Logger::Log(const std::string& message)
{
    const std::string line = TimestampNow() + " - " + message + "\n";
    std::lock_guard<std::mutex> lk(mtx_);
    if (ofsRun_.good()) ofsRun_ << line << std::flush;
    if (ofsLatest_.good()) ofsLatest_ << line << std::flush;
}
