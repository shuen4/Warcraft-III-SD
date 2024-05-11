#pragma once
#include <vector>
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include "log.h"
#pragma comment(lib, "winhttp.lib")

bool HTTPGetFile(std::string url, std::vector<uint8_t>& data);
bool HTTPGetFile(const char* url, std::vector<uint8_t>& data);
bool HTTPGetFile(char* url, std::vector<uint8_t>& data);
bool HTTPGetFilePartial(std::string url, std::vector<uint8_t>& data, uint64_t offset, uint64_t size);
bool HTTPGetFilePartial(const char* url, std::vector<uint8_t>& data, uint64_t offset, uint64_t size);
bool HTTPGetFilePartial(char* url, std::vector<uint8_t>& data, uint64_t offset, uint64_t size);