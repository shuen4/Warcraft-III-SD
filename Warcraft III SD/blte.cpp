#include "blte.h"

bool BLTE_encode(std::vector<uint8_t>& data, std::vector<uint8_t>& out) {
    data.insert(data.begin(), 'N');
    std::string md5_string = md5((char*)&data[0], (long)data.size());
    std::vector<uint8_t> md5 = md5_string_to_vector(md5_string);
    // BLTE
    out.clear();
    out.push_back('B');
    out.push_back('L');
    out.push_back('T');
    out.push_back('E');
    // header size
    out.push_back('\0');
    out.push_back('\0');
    out.push_back('\0');
    out.push_back('\x24');
    // flag
    out.push_back('\xF');
    // chunck count
    out.push_back('\0');
    out.push_back('\0');
    out.push_back('\1');
    // compressed size
    uint32_t size = (uint32_t)data.size();
    reverse(size);
    out.push_back((uint8_t)(size >> 0));
    out.push_back((uint8_t)(size >> 8));
    out.push_back((uint8_t)(size >> 16));
    out.push_back((uint8_t)(size >> 24));
    // decompressed size
    reverse(size);
    size -= 1;
    reverse(size);
    out.push_back((uint8_t)(size >> 0));
    out.push_back((uint8_t)(size >> 8));
    out.push_back((uint8_t)(size >> 16));
    out.push_back((uint8_t)(size >> 24));
    // md5
    out.insert(out.end(), md5.begin(), md5.end());
    out.insert(out.end(), data.begin(), data.end());
    data.erase(data.begin());
    return true;
}
bool BLTE_decode(std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    CReader reader(in);
    if (reader.Read<uint32_t>() != 'ETLB') { // BLTE
        Log.error("invalid BLTE header\n");
        return false;
    }
    uint32_t header_size = reader.Read<uint32_t>();
    reverse(header_size);
    if (reader.Read<uint8_t>() != 0xF) {
        reader.curPos--;
        Log.error("expected flag 0x0F, got 0x%02hhX\n", reader.Read<uint8_t>());
        return false;
    }
    uint8_t chunk_count_hi = reader.Read<uint8_t>();
    uint16_t chunk_count_lo = reader.Read<uint16_t>();
    reverse(chunk_count_lo);
    uint32_t chunk_count = chunk_count_lo | chunk_count_hi << 16;
    std::vector<CChunkInfoEntry> chunk_info_entry;
    for (uint32_t i = 0; i < chunk_count; i++) {
        uint32_t compressed_size = reader.Read<uint32_t>();
        reverse(compressed_size);
        uint32_t decompressed_size = reader.Read<uint32_t>();
        reverse(decompressed_size);
        std::vector<uint8_t> checksum;
        checksum.resize(16);
        checksum[0] = reader.Read<uint8_t>();
        checksum[1] = reader.Read<uint8_t>();
        checksum[2] = reader.Read<uint8_t>();
        checksum[3] = reader.Read<uint8_t>();
        checksum[4] = reader.Read<uint8_t>();
        checksum[5] = reader.Read<uint8_t>();
        checksum[6] = reader.Read<uint8_t>();
        checksum[7] = reader.Read<uint8_t>();
        checksum[8] = reader.Read<uint8_t>();
        checksum[9] = reader.Read<uint8_t>();
        checksum[10] = reader.Read<uint8_t>();
        checksum[11] = reader.Read<uint8_t>();
        checksum[12] = reader.Read<uint8_t>();
        checksum[13] = reader.Read<uint8_t>();
        checksum[14] = reader.Read<uint8_t>();
        checksum[15] = reader.Read<uint8_t>();
        chunk_info_entry.push_back(CChunkInfoEntry(compressed_size, decompressed_size, checksum));
    }
    if (reader.curPos != header_size) {
        Log.error("expected pos %d, got %d\n", header_size, reader.curPos);
        return false;
    }
    for (size_t i = 0; i < chunk_info_entry.size(); i++) {
        std::string md5_string = md5((char*)&reader.data[reader.curPos], chunk_info_entry[i].compressed_size);
        if (chunk_info_entry[i].checksum != md5_string_to_vector(md5_string)) {
            Log.error("checksum verify failed\n");
            return false;
        }
        uint8_t encoding_mode = reader.Read<uint8_t>();
        uint32_t encoded_data_size = chunk_info_entry[i].compressed_size - 1;
        std::vector<uint8_t> encoded_data(reader.data.begin() + reader.curPos, reader.data.begin() + reader.curPos + encoded_data_size);
        reader.curPos += encoded_data_size;
        std::vector<uint8_t> decoded_data;
        switch (encoding_mode) {
        case 'N':
            // plain data
            decoded_data.insert(decoded_data.end(), encoded_data.begin(), encoded_data.end());
            break;
        case 'Z':
            // zlib
            // https://github.com/ladislav-zezula/CascLib/blob/d9ee87443dbba33ba02858bcdff44d57784a79fb/src/CascDecompress.cpp#L18
            {
                decoded_data.resize(chunk_info_entry[i].decompressed_size);
                zng_stream z;
                z.next_in = &encoded_data[0];
                z.avail_in = encoded_data_size;
                z.total_in = encoded_data_size;
                z.next_out = &decoded_data[0];
                z.avail_out = (uint32_t)decoded_data.size();
                z.total_out = 0;
                z.zalloc = NULL;
                z.zfree = NULL;
                if (zng_inflateInit(&z) != Z_OK) {
                    Log.error("zlib init failed\n");
                    return false;
                }
                int32_t ret = zng_inflate(&z, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    Log.error("zlib decompress failed\n");
                    return false;
                }
                if (z.total_out != decoded_data.size()) {
                    Log.error("unexpected zlib decompressed size\n");
                    return false;
                }
                zng_inflateEnd(&z);
                break;
            }
        case '4':
        case 'E':
        case 'F':
        default:
            Log.error("unsupported encoding mode %c\n", encoding_mode);
            return false;
        }
        if (chunk_info_entry[i].decompressed_size != decoded_data.size()) {
            Log.error("expected decompressed size %d, got %d\n", chunk_info_entry[i].decompressed_size, decoded_data.size());
            return false;
        }
        out.insert(out.end(), decoded_data.begin(), decoded_data.end());
    }
    if (reader.curPos != reader.data.size()) {
        Log.warn("read %d bytes, %d bytes left\n", reader.curPos, reader.data.size() - reader.curPos);
    }
    return true;
}
bool BLTE_md5(std::vector<uint8_t>& in, std::string& out) {
    CReader reader(in);
    if (reader.Read<uint32_t>() != 'ETLB') { // BLTE
        Log.error("invalid BLTE header\n");
        return false;
    }
    uint32_t header_size = reader.Read<uint32_t>();
    reverse(header_size);
    if (in.size() < header_size) {
        Log.error("invalid BLTE header size\n");
        return false;
    }
    out = md5((char*)&in[0], header_size);
    return true;
}