#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <Windows.h>
#include <algorithm>
#include <sstream>
#include "log.h"

template<class t = uint64_t>
t ReadMemory(uint64_t addr) {
    return *(t*)addr;
}
std::wstring a2w(const std::string_view& str);
template<class t>
void reverse(t& value) {
    std::reverse<uint8_t*>((uint8_t*)&value, (uint8_t*)&value + sizeof(t));
}
std::string vector_to_hex_string(std::vector<uint8_t>& data);
void print_vector_as_hex(std::vector<uint8_t>& data);
std::vector<uint8_t> md5_string_to_vector(std::string& str);
bool string_start_with(std::string& s, std::string start);
void string_replace(std::string& Text, std::string Key, std::string Value);
std::string nslookup(std::string);