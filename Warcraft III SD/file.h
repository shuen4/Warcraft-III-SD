#pragma once
#include <vector>
#include <fstream>
#include "log.h"

bool read_file(char* filepath, std::vector<uint8_t>& data);
bool read_file(const char* filepath, std::vector<uint8_t>& data);
bool read_file(std::string filepath, std::vector<uint8_t>& data);
bool write_file(char* filepath, std::vector<uint8_t>& data);
bool write_file(const char* filepath, std::vector<uint8_t>& data);
bool write_file(std::string filepath, std::vector<uint8_t>& data);
bool file_exist(std::string file);