#pragma once
#include <stdarg.h>
#include <iostream>
#include <fstream>

class CLog {
public:
    std::ofstream file;
    CLog();
    void error(const char* format, ...);
    void info(const char* format, ...);
    void warn(const char* format, ...);
    void debug(const char* format, ...);
    void write(const char* format, ...);
};
extern "C" CLog Log;