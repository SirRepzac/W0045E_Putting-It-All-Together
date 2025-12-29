#pragma once
#include <string>
#include <mutex>
#include <fstream>

class Logger
{
public:
    static Logger& Instance();

    // Thread-safe append with newline
    void Log(const std::string& message);

private:
    Logger();
    ~Logger();

    // non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mtx_;
    std::ofstream ofsRun_;
    std::ofstream ofsLatest_;
};