#pragma once
#include <vector>
#include <fstream>
#include <zlib-ng.h>
#include "md5.h"
#include "reader.h"
#include "util.h"

class CChunkInfoEntry {
public:
    uint32_t compressed_size;
    uint32_t decompressed_size;
    std::vector<uint8_t> checksum;
    CChunkInfoEntry(uint32_t compressed_size, uint32_t decompressed_size, std::vector<uint8_t> checksum) :compressed_size(compressed_size), decompressed_size(decompressed_size), checksum(checksum) {}
};
bool BLTE_encode(std::vector<uint8_t>& data, std::vector<uint8_t>& out);
bool BLTE_decode(std::vector<uint8_t>& in, std::vector<uint8_t>& out);
bool BLTE_md5(std::vector<uint8_t>& in, std::string& out);