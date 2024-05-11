#include "log.h"

CLog Log;
CLog::CLog() :file("log.txt", std::ios::out | std::ios::binary) {}
void CLog::error(const char* format, ...) {
    char buffer[0x400];
    strcpy_s(buffer, 0x400, "[ERROR] ");
    va_list args;
    va_start(args, format);
    uint32_t count = vsprintf_s(buffer + 8, 0x400 - 8, format, args);
    va_end(args);
    file.write(buffer, count + 8);
    std::cout.write(buffer, count + 8);
}
void CLog::info(const char* format, ...) {
    char buffer[0x400];
    strcpy_s(buffer, 0x400, "[INFO ] ");
    va_list args;
    va_start(args, format);
    uint32_t count = vsprintf_s(buffer + 8, 0x400 - 8, format, args);
    va_end(args);
    file.write(buffer, count + 8);
    std::cout.write(buffer, count + 8);
}
void CLog::warn(const char* format, ...) {
    char buffer[0x400];
    strcpy_s(buffer, 0x400, "[WARN ] ");
    va_list args;
    va_start(args, format);
    uint32_t count = vsprintf_s(buffer + 8, 0x400 - 8, format, args);
    va_end(args);
    file.write(buffer, count + 8);
    std::cout.write(buffer, count + 8);
}
void CLog::debug(const char* format, ...) {
    char buffer[0x400];
    strcpy_s(buffer, 0x400, "[DEBUG] ");
    va_list args;
    va_start(args, format);
    uint32_t count = vsprintf_s(buffer + 8, 0x400 - 8, format, args);
    va_end(args);
    file.write(buffer, count + 8);
    std::cout.write(buffer, count + 8);
}
void CLog::write(const char* format, ...) {
    char buffer[0x400];
    va_list args;
    va_start(args, format);
    uint32_t count = vsprintf_s(buffer, 0x400, format, args);
    va_end(args);
    file.write(buffer, count);
}