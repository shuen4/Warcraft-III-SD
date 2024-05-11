#include <iostream>
#include <signal.h>
#include <map>
#include <chrono>
#include <ctime>   
#include "file.h"
#include "reader.h"
#include "http.h"
#include "util.h"
#include "blte.h"

#define PATCH_HOST "us.patch.battle.net"
#define PATCH_HTTP "http://us.patch.battle.net:1119"
#define PRODUCT "w3"
#define CHECKSUM_SIZE 16
// parsable from PATCH_HTTP/PRODUCT/cdns
#define CDN_HOST "level3.blizzard.com"
#define CDN_HTTP "http://level3.blizzard.com"
#define PATH "tpr/war3"

class CArchiveIndex {
public:
    CArchiveIndex() :ekey(), archive(), offset(0), size(0), archive_index(-1) {};
    CArchiveIndex(std::vector<uint8_t> ekey, std::string archive, uint32_t offset, uint32_t size, uint32_t archive_index) :ekey(ekey), archive(archive), offset(offset), size(size), archive_index(archive_index) {}
    std::vector<uint8_t> ekey;
    std::string archive;
    uint32_t offset;
    uint32_t size;
    uint32_t archive_index;
};
class CDownloadEntry {
public:
    std::vector<uint8_t> encoded_key;
    std::vector<std::string> tag;
    uint64_t file_size;
    uint8_t priority;
    CDownloadEntry(std::vector<uint8_t> encoded_key, uint64_t file_size, uint8_t priority) :encoded_key(encoded_key), file_size(file_size), priority(priority), tag() {}
};
class CTag {
public:
    std::string name;
    uint16_t type;
    CTag(std::string name, uint16_t type) :name(name), type(type) {}
};
HANDLE hEvent;
void onSignal(int s) {
    SetEvent(hEvent);
}
int main() {
    if (file_exist("nginx\\html"))
        system("rmdir /s /q nginx\\html");
    if (!file_exist("backup"))
        (void)_wmkdir(L"backup");

    // startup winsock
    WSADATA wsaData{};
    int WSA_startup = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (WSA_startup) {
        Log.error("WSAStartup failed with %d\n", WSA_startup);
        return 1;
    }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        Log.error("WSAStartup failed with incompatible version\n");
        return 1;
    }

    // backup hosts file
    std::vector<uint8_t> hosts_data_bin;
    if (!read_file("C:\\Windows\\System32\\drivers\\etc\\hosts", hosts_data_bin)) {
        Log.error("failed to read hosts file for backup\n");
        return 1;
    }
    std::string hosts_data(hosts_data_bin.begin(), hosts_data_bin.end());
    if (!write_file(std::string("backup\\hosts." + std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))), hosts_data_bin)) {
        Log.error("failed to write hosts file for backup\n");
        return 1;
    }

    // get necessary info
    std::string build_config;
    std::string cdn_config;
    std::vector<uint8_t> version_info_bin;
    if (!HTTPGetFile(PATCH_HTTP "/" PRODUCT "/versions", version_info_bin)) {
        Log.error("failed to get " PATCH_HTTP "/" PRODUCT "/versions content\n");
        return 1;
    }
    std::istringstream iss(std::string(version_info_bin.begin(), version_info_bin.end()));
    std::string line;
    std::string entry;
    uint32_t build_config_pos = -1;
    uint32_t cdn_config_pos = -1;
    // parse header
    while (std::getline(iss, line)) {
        if (!line.empty() && !string_start_with(line, "#")) {
            uint32_t i = 0;
            std::istringstream iss1(line);
            while (std::getline(iss1, entry, '|')) {
                if (entry == "BuildConfig!HEX:16")
                    build_config_pos = i;
                else if(entry == "CDNConfig!HEX:16")
                    cdn_config_pos = i;
                i++;
            }
            break;
        }
    }
    if (build_config_pos == -1) {
        Log.error("failed to find build config pos\n");
        return 1;
    }
    if (cdn_config_pos == -1) {
        Log.error("failed to find cdn config pos\n");
        return 1;
    }
    // extract info
    while (std::getline(iss, line)) {
        if (!line.empty() && !string_start_with(line, "#")) {
            uint32_t i = 0;
            std::istringstream iss1(line);
            while (std::getline(iss1, entry, '|')) {
                if (i == build_config_pos)
                    build_config = entry;
                else if (i == cdn_config_pos)
                    cdn_config = entry;
                i++;
            }
            break;
        }
    }
    Log.debug("build %s, cdn %s\n", build_config.c_str(), cdn_config.c_str());
    if (build_config.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid build config hex length\n");
        return 1;
    }
    if (cdn_config.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid cdn config hex length\n");
        return 1;
    }

    // extract necessary info from build config
    std::string root_ckey;
    std::string download_ckey;
    std::string download_ekey; // might missing
    std::string encoding_ckey;
    std::string encoding_ekey; // should always present
    std::vector<uint8_t> build_config_bin;
    std::string build_config_url = CDN_HTTP "/" PATH "/config/";
    build_config_url += build_config.substr(0, 2);
    build_config_url.append(1, '/');
    build_config_url += build_config.substr(2, 2);
    build_config_url.append(1, '/');
    build_config_url += build_config;
    if (!HTTPGetFile(build_config_url, build_config_bin)) {
        Log.error("failed to get %s content\n", build_config_url.c_str());
        return 1;
    }
    iss.clear();
    iss.seekg(0, std::ios::beg);
    iss.str(std::string(build_config_bin.begin(), build_config_bin.end()));
    while (std::getline(iss, line)) {
        if (!line.empty() && !string_start_with(line, "#")) {
            if (string_start_with(line, "root = ")) {
                root_ckey = line.substr(7, CHECKSUM_SIZE * 2);
            }
            else if (string_start_with(line, "download = ")) {
                download_ckey = line.substr(11, CHECKSUM_SIZE * 2);
                if (line.size() > 43) // with ekey
                    download_ekey = line.substr(44, CHECKSUM_SIZE * 2);
            }
            else if (string_start_with(line, "encoding = ")) {
                encoding_ckey = line.substr(11, CHECKSUM_SIZE * 2);
                if (line.size() > 43) // with ekey
                    encoding_ekey = line.substr(44, CHECKSUM_SIZE * 2);
            }
        }
    }
    Log.debug("root %s\n", root_ckey.c_str());
    Log.debug("download %s, %s\n", download_ckey.c_str(), download_ekey.size() > 0 ? download_ekey.c_str() : "<missing>");
    Log.debug("encoding %s, %s\n", encoding_ckey.c_str(), encoding_ekey.c_str());
    if (root_ckey.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid root ckey hex length\n");
        return 1;
    }
    if (download_ckey.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid download ckey hex length\n");
        return 1;
    }
    if (!download_ekey.empty() && download_ekey.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid download ekey hex length\n");
        return 1;
    }
    if (encoding_ckey.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid encoding ckey hex length\n");
        return 1;
    }
    if (encoding_ekey.size() != CHECKSUM_SIZE * 2) {
        Log.error("invalid encoding ekey hex length\n");
        return 1;
    }

    // extract necessary info from cdn config
    std::vector<std::string> archive_list;
    std::string archive_group_hash;
    std::vector<uint8_t> cdn_config_bin;
    std::string file_index;
    std::string cdn_config_url = CDN_HTTP "/" PATH "/config/";
    cdn_config_url += cdn_config.substr(0, 2);
    cdn_config_url.append(1, '/');
    cdn_config_url += cdn_config.substr(2, 2);
    cdn_config_url.append(1, '/');
    cdn_config_url += cdn_config;
    if (!HTTPGetFile(cdn_config_url, cdn_config_bin)) {
        Log.error("failed to get %s content\n", cdn_config_url.c_str());
        return 1;
    }
    iss.clear();
    iss.seekg(0, std::ios::beg);
    iss.str(std::string(cdn_config_bin.begin(), cdn_config_bin.end()));
    while (std::getline(iss, line)) {
        if (!line.empty() && !string_start_with(line, "#")) {
            if (string_start_with(line, "archives = ")) {
                line.erase(line.begin(), line.begin() + 11);
                while (line.size() >= CHECKSUM_SIZE * 2) {
                    archive_list.push_back(line.substr(0, CHECKSUM_SIZE * 2));
                    line.erase(line.begin(), line.begin() + CHECKSUM_SIZE * 2);
                    if (line.size() > 0)
                        line.erase(line.begin());
                }
            }
            else if (string_start_with(line, "file-index = ")) {
                file_index = line.substr(13, CHECKSUM_SIZE * 2);
            }
            else if (string_start_with(line, "archive-group = ")) {
                archive_group_hash = line.substr(16, CHECKSUM_SIZE * 2);
            }
        }
    }
    if (file_index.empty()) {
        Log.error("could not find file index from cdn config");
        return 1;
    }
    if (archive_group_hash.empty()) {
        Log.error("could not find archive group from cdn config");
        return 1;
    }

    // read archive index
    std::map<std::string, CArchiveIndex> archive_index;
    for (size_t i = 0; i < archive_list.size(); i++) {
        std::string index_url = CDN_HTTP "/" PATH "/data/";
        index_url += archive_list[i].substr(0, 2);
        index_url.append(1, '/');
        index_url += archive_list[i].substr(2, 2);
        index_url.append(1, '/');
        index_url += archive_list[i];
        index_url += ".index";
        std::vector<uint8_t> index_data;
        if (!HTTPGetFile(index_url, index_data)) {
            Log.error("failed to get %s content\n", index_url.c_str());
            return 1;
        }
        // usually keySizeInBytes = 16, checksumSize = 8, blockSizeKb = 4, sizeBytes = 4, offsetBytes = 4
        // guess it by using above value;
        uint32_t page_count = 0;
        uint32_t guess_size = /* other fixed size */12 + /* checksumSize */ 8 + /* checksumSize */ 8;
        while (guess_size != index_data.size()) {
            if (guess_size > index_data.size()) {
                Log.error("failed to guess page size\n");
                return 1;
            }
            page_count++;
            guess_size += /* blockSizeKb */4 * 1024 + /* keySizeInBytes */16 + /* checksumSize */ 8;
        }
        CReader reader(index_data);
        for (uint32_t i1 = 0; i1 < page_count; i1++) {
            reader.curPos = i1 * /* blockSizeKb */4 * 1024;
            while (true) {
                std::vector<uint8_t> ekey = reader.ReadVector<uint8_t>(CHECKSUM_SIZE);
                if (
                    ekey[0] == 0 &&
                    ekey[1] == 0 &&
                    ekey[2] == 0 &&
                    ekey[3] == 0 &&
                    ekey[4] == 0 &&
                    ekey[5] == 0 &&
                    ekey[6] == 0 &&
                    ekey[7] == 0 &&
                    ekey[8] == 0 &&
                    ekey[9] == 0 &&
                    ekey[10] == 0 &&
                    ekey[11] == 0 &&
                    ekey[12] == 0 &&
                    ekey[13] == 0 &&
                    ekey[14] == 0 &&
                    ekey[15] == 0
                )
                    break;
                uint32_t size = reader.Read<uint32_t>();
                reverse(size);
                uint32_t offset = reader.Read<uint32_t>();
                reverse(offset);
                archive_index[vector_to_hex_string(ekey)] = CArchiveIndex(ekey, archive_list[i], offset, size, (uint32_t)i);
            }
        }
    }
    // repeat for file index
    std::string file_index_url = CDN_HTTP "/" PATH "/data/";
    file_index_url += file_index.substr(0, 2);
    file_index_url.append(1, '/');
    file_index_url += file_index.substr(2, 2);
    file_index_url.append(1, '/');
    file_index_url += file_index;
    file_index_url += ".index";
    std::vector<uint8_t> file_index_data;
    if (!HTTPGetFile(file_index_url, file_index_data)) {
        Log.error("failed to get %s content\n", file_index_url.c_str());
        return 1;
    }
    uint32_t page_count = 0;
    uint32_t guess_size = /* other fixed size */12 + /* checksumSize */ 8 + /* checksumSize */ 8;
    while (guess_size != file_index_data.size()) {
        if (guess_size > file_index_data.size()) {
            Log.error("failed to guess page size\n");
            return 1;
        }
        page_count++;
        guess_size += /* blockSizeKb */4 * 1024 + /* keySizeInBytes */16 + /* checksumSize */ 8;
    }
    CReader reader(file_index_data);
    for (uint32_t i1 = 0; i1 < page_count; i1++) {
        reader.curPos = i1 * /* blockSizeKb */4 * 1024;
        while (true) {
            std::vector<uint8_t> ekey = reader.ReadVector<uint8_t>(CHECKSUM_SIZE);
            if (
                ekey[0] == 0 &&
                ekey[1] == 0 &&
                ekey[2] == 0 &&
                ekey[3] == 0 &&
                ekey[4] == 0 &&
                ekey[5] == 0 &&
                ekey[6] == 0 &&
                ekey[7] == 0 &&
                ekey[8] == 0 &&
                ekey[9] == 0 &&
                ekey[10] == 0 &&
                ekey[11] == 0 &&
                ekey[12] == 0 &&
                ekey[13] == 0 &&
                ekey[14] == 0 &&
                ekey[15] == 0
            )
                break;
            uint32_t size = reader.Read<uint32_t>();
            reverse(size);
            archive_index[vector_to_hex_string(ekey)] = CArchiveIndex(ekey, vector_to_hex_string(ekey), 0, size, -1);
        }
    }

    // read encoding
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> ekey_to_ckey;
    std::vector<uint8_t> root_ckey_vec = md5_string_to_vector(root_ckey);
    std::vector<uint8_t> download_ckey_vec = md5_string_to_vector(download_ckey);
    std::string root_ekey;
    if (archive_index.find(encoding_ekey) == archive_index.end()) {
        Log.error("cannot find encoding ekey at archive\n");
        return 1;
    }
    CArchiveIndex& archive_info = archive_index[encoding_ekey];
    Log.debug("ENCODING %s at archive %s, offset %d, size %d\n", encoding_ekey.c_str(), archive_info.archive.c_str(), archive_info.offset, archive_info.size);
    std::string archive_url = CDN_HTTP "/" PATH "/data/";
    archive_url += archive_info.archive.substr(0, 2);
    archive_url.append(1, '/');
    archive_url += archive_info.archive.substr(2, 2);
    archive_url.append(1, '/');
    archive_url += archive_info.archive;
    std::vector<uint8_t> encoding_data_encoded;
    if (!HTTPGetFilePartial(archive_url, encoding_data_encoded, archive_info.offset, archive_info.size)) {
        Log.error("failed to get %s content\n", archive_url.c_str());
        return 1;
    }
    std::vector<uint8_t> encoding_data;
    if (!BLTE_decode(encoding_data_encoded, encoding_data)) {
        Log.error("failed to decode ENCODING %s\n", encoding_ekey.c_str());
        return 1;
    }
    reader.curPos = 0;
    reader.data = encoding_data;
    if (reader.Read<uint16_t>() != 0x4E45) { // EN
        Log.error("invalid ENCODING file header\n");
        return 1;
    }
    if (reader.Read<uint8_t>() != 0x01) {
        Log.error("expected ENCODING header version 1\n");
        return 1;
    }
    if (reader.Read<uint8_t>() != CHECKSUM_SIZE) {
        Log.error("expected ENCODING content hash key size %d\n", CHECKSUM_SIZE);
        return 1;
    }
    if (reader.Read<uint8_t>() != CHECKSUM_SIZE) {
        Log.error("expected ENCODING encoded hash key size %d\n", CHECKSUM_SIZE);
        return 1;
    }
    uint16_t CEKey_page_size = reader.Read<uint16_t>();
    reverse(CEKey_page_size);
    CEKey_page_size *= 1024;
    reader.curPos += 2; // EKeySpecPageTable_page_size_kb
    uint32_t CEKey_page_count = reader.Read<uint32_t>();
    reverse(CEKey_page_count);
    reader.curPos += 4; // EKeySpec_count
    reader.curPos += 1; // asserted 0 by agent
    uint32_t ESpec_block_size = reader.Read<uint32_t>();
    reverse(ESpec_block_size);
    reader.curPos += ESpec_block_size;
    reader.curPos += 0x20 * CEKey_page_count;
    size_t page_start = reader.curPos;
    for (uint32_t i = 0; i < CEKey_page_count; i++) {
        while (true) {
            uint8_t ekey_count = reader.Read<uint8_t>();
            if (ekey_count == 0)
                break;

            reader.curPos += 5; // file_size
            std::vector<uint8_t> content_key = reader.ReadVector<uint8_t>(16);
            for (uint32_t i1 = 0; i1 < ekey_count; i1++) {
                std::vector<uint8_t> encoded_key = reader.ReadVector<uint8_t>(16);
                if (content_key == root_ckey_vec)
                    root_ekey = vector_to_hex_string(encoded_key);
                if (content_key == download_ckey_vec)
                    download_ekey = vector_to_hex_string(encoded_key);
                ekey_to_ckey[encoded_key] = content_key;
            }
        }
        page_start += CEKey_page_size;
        reader.curPos = page_start;
    }

    // read root
    std::map<std::vector<uint8_t>, std::string> ckey_to_archive_path;
    if (root_ekey.empty()) {
        Log.error("cannot find root ckey at encoding\n");
        return 1;
    }
    if (archive_index.find(root_ekey) == archive_index.end()) {
        Log.error("cannot find root ekey at archive\n");
        return 1;
    }
    archive_info = archive_index[root_ekey];
    Log.debug("ROOT %s at archive %s, offset %d, size %d\n", root_ekey.c_str(), archive_info.archive.c_str(), archive_info.offset, archive_info.size);
    archive_url = CDN_HTTP "/" PATH "/data/";
    archive_url += archive_info.archive.substr(0, 2);
    archive_url.append(1, '/');
    archive_url += archive_info.archive.substr(2, 2);
    archive_url.append(1, '/');
    archive_url += archive_info.archive;
    std::vector<uint8_t> root_data_encoded;
    if (!HTTPGetFilePartial(archive_url, root_data_encoded, archive_info.offset, archive_info.size)) {
        Log.error("failed to get %s content\n", archive_url.c_str());
        return 1;
    }
    std::vector<uint8_t> root_data;
    if (!BLTE_decode(root_data_encoded, root_data)) {
        Log.error("failed to decode ROOT %s\n", root_ekey.c_str());
        return 1;
    }
    iss.clear();
    iss.seekg(0, std::ios::beg);
    iss.str(std::string(root_data.begin(), root_data.end()));
    // archive_path|content_key|tag|file_system_path
    while (std::getline(iss, line)) {
        size_t i = 0;
        std::string archive_path = line.substr(i, line.find('|', i + 1) - i);
        i = line.find('|', i + 1);
        if (i == std::string::npos) {
            Log.warn("invalid format at ROOT\n", line);
            continue;
        }
        std::string content_key_string = line.substr(i + 1, line.find('|', i + 1) - i - 1);
        ckey_to_archive_path[md5_string_to_vector(content_key_string)] = archive_path;
        // ignore other part
    }

    // read download
    std::vector<CDownloadEntry> download_entry;
    std::vector<CTag> download_entry_tag;
    if (download_ekey.empty()) {
        Log.error("cannot find download ekey at encoding\n");
        return 1;
    }
    if (archive_index.find(download_ekey) == archive_index.end()) {
        Log.error("cannot find download ekey at archive\n");
        return 1;
    }
    archive_info = archive_index[download_ekey];
    Log.debug("DOWNLOAD %s at archive %s, offset %d, size %d\n", download_ekey.c_str(), archive_info.archive.c_str(), archive_info.offset, archive_info.size);
    archive_url = CDN_HTTP "/" PATH "/data/";
    archive_url += archive_info.archive.substr(0, 2);
    archive_url.append(1, '/');
    archive_url += archive_info.archive.substr(2, 2);
    archive_url.append(1, '/');
    archive_url += archive_info.archive;
    std::vector<uint8_t> download_data_encoded;
    if (!HTTPGetFilePartial(archive_url, download_data_encoded, archive_info.offset, archive_info.size)) {
        Log.error("failed to get %s content\n", archive_url.c_str());
        return 1;
    }
    std::vector<uint8_t> download_data;
    if (!BLTE_decode(download_data_encoded, download_data)) {
        Log.error("failed to decode DOWNLOAD %s\n", download_ekey.c_str());
        return 1;
    }
    reader.curPos = 0;
    reader.data = download_data;
    if (reader.Read<uint16_t>() != 'LD') { // DL
        Log.error("invalid DOWNLOAD file header\n");
        return 1;
    }
    if (reader.Read<uint8_t>() != 0x01) {
        Log.error("expected DOWNLOAD header version 1\n");
        return 1;
    }
    if (reader.Read<uint8_t>() != CHECKSUM_SIZE) {
        Log.error("expected DOWNLOAD checksum size %d\n", CHECKSUM_SIZE);
        return 1;
    }
    uint8_t unk1 = reader.Read<uint8_t>();
    uint32_t entry_count = reader.Read<uint32_t>();
    reverse(entry_count);
    uint16_t tag_count = reader.Read<uint16_t>();
    reverse(tag_count);
    for (uint32_t i = 0; i < entry_count; i++) {
        std::vector<uint8_t> encoded_key = reader.ReadVector<uint8_t>(CHECKSUM_SIZE);
        uint8_t file_size_hi = reader.Read<uint8_t>();
        uint32_t file_size_lo = reader.Read<uint32_t>();
        reverse(file_size_lo);
        uint64_t file_size = (uint64_t)file_size_lo | (uint64_t)file_size_hi << 32;
        uint8_t priority = reader.Read<uint8_t>();
        download_entry.push_back(CDownloadEntry(encoded_key, file_size, priority));
    }
    uint32_t bit_size = entry_count / 8 + (entry_count % 8 > 0 ? 1 : 0);
    for (uint32_t i = 0; i < tag_count; i++) {
        std::string name = reader.ReadString();
        uint16_t type = reader.Read<uint16_t>(); // 1 = locale, 2 = platform, 3 = release channel
        reverse(type);
        download_entry_tag.push_back(CTag(name, type));
        std::vector<uint8_t> tag_bit;
        for (uint32_t i1 = 0; i1 < bit_size; i1++) {
            uint8_t bit = reader.Read<uint8_t>();
            tag_bit.push_back(bit);
            for (uint32_t i2 = 0; i2 < 8; i2++)
                if (bit & 1 << i2)
                    if (i1 * 8 + i2 < download_entry.size())
                        download_entry[i1 * 8 + i2].tag.push_back(name);
        }
    }

    // delete archive path starting with "War3.w3mod:_HD.w3mod:" from download entry
    download_entry.erase(std::remove_if(download_entry.begin(), download_entry.end(), [&ekey_to_ckey, &ckey_to_archive_path](CDownloadEntry entry) {
        std::vector<uint8_t> encoded_key = entry.encoded_key;
        if (ekey_to_ckey.find(encoded_key) == ekey_to_ckey.end()) {
            Log.warn("ekey_to_ckey failed.\n");
            return false;
        }

        if (ckey_to_archive_path.find(ekey_to_ckey[encoded_key]) == ckey_to_archive_path.end()) {
            Log.warn("ckey_to_archive_path failed.\n");
            return false;
        }
        std::string archive_path = ckey_to_archive_path[ekey_to_ckey[encoded_key]];
        bool remove = string_start_with(archive_path, "War3.w3mod:_HD.w3mod:");
        if (remove)
            Log.info("removing %s\n", archive_path.data());
        return remove;
    }), download_entry.end());

    // write modified download
    std::vector<uint8_t> new_download_data;
    // header
    new_download_data.push_back('D');
    new_download_data.push_back('L');
    new_download_data.push_back(1);
    new_download_data.push_back(CHECKSUM_SIZE);
    new_download_data.push_back(unk1);
    new_download_data.push_back((uint8_t)(download_entry.size() >> 24));
    new_download_data.push_back((uint8_t)(download_entry.size() >> 16));
    new_download_data.push_back((uint8_t)(download_entry.size() >>  8));
    new_download_data.push_back((uint8_t)(download_entry.size() >>  0));
    new_download_data.push_back((uint8_t)(download_entry_tag.size() >> 8));
    new_download_data.push_back((uint8_t)(download_entry_tag.size() >> 0));
    for (uint32_t i = 0; i < download_entry.size(); i++) {
        new_download_data.insert(new_download_data.end(), download_entry[i].encoded_key.begin(), download_entry[i].encoded_key.end());
        new_download_data.push_back((uint8_t)(download_entry[i].file_size >> 32));
        new_download_data.push_back((uint8_t)(download_entry[i].file_size >> 24));
        new_download_data.push_back((uint8_t)(download_entry[i].file_size >> 16));
        new_download_data.push_back((uint8_t)(download_entry[i].file_size >> 8));
        new_download_data.push_back((uint8_t)(download_entry[i].file_size >> 0));
        new_download_data.push_back((uint8_t)(download_entry[i].priority));
    }
    uint32_t bit_count = (uint32_t)(download_entry.size() / 8 + (download_entry.size() % 8 > 0 ? 1 : 0));
    for (uint32_t i = 0; i < download_entry_tag.size(); i++) {
        new_download_data.insert(new_download_data.end(), download_entry_tag[i].name.begin(), download_entry_tag[i].name.end());
        new_download_data.push_back(0); // null terminator
        new_download_data.push_back((uint8_t)(download_entry_tag[i].type >> 8));
        new_download_data.push_back((uint8_t)(download_entry_tag[i].type >> 0));
        uint8_t* tag_bit = new uint8_t[bit_count];
        memset(tag_bit, 0, bit_count);
        for (uint32_t i1 = 0; i1 < download_entry.size(); i1++) {
            for (uint32_t i2 = 0; i2 < download_entry[i1].tag.size(); i2++) {
                if (download_entry[i1].tag[i2] == download_entry_tag[i].name) {
                    uint32_t array_index = i1 / 8;
                    uint32_t bit_index = i1 % 8;
                    tag_bit[array_index] |= 1 << bit_index;
                    break;
                }
            }
        }
        // unused bit
        for (uint32_t i1 = (uint32_t)download_entry.size(); i1 < bit_count * 8; i1++) {
            uint32_t array_index = i1 / 8;
            uint32_t bit_index = i1 % 8;
            tag_bit[array_index] |= 1 << bit_index;
        }
        new_download_data.insert(new_download_data.end(), tag_bit, tag_bit + bit_count);
        delete[] tag_bit;
    }

    // calc modified download ckey and ekey
    std::string new_download_ckey = md5((char*)&new_download_data[0], (long)new_download_data.size());
    std::vector<uint8_t> new_download_data_encoded;
    BLTE_encode(new_download_data, new_download_data_encoded); // never fail
    std::string new_download_ekey;
    if (!BLTE_md5(new_download_data_encoded, new_download_ekey)) {
        Log.error("Failed to calc BLTE checksum\n");
        return 1;
    }

    // add modified download to archive
    // add is simple than modify current archive so ...
    std::vector<uint8_t> new_archive_index_data;
    std::vector<uint8_t> new_download_ekey_vec = md5_string_to_vector(new_download_ekey);
    new_archive_index_data.insert(new_archive_index_data.end(), new_download_ekey_vec.begin(), new_download_ekey_vec.end());
    new_archive_index_data.push_back((uint8_t)(new_download_data_encoded.size() >> 24));
    new_archive_index_data.push_back((uint8_t)(new_download_data_encoded.size() >> 16));
    new_archive_index_data.push_back((uint8_t)(new_download_data_encoded.size() >>  8));
    new_archive_index_data.push_back((uint8_t)(new_download_data_encoded.size() >>  0));
    new_archive_index_data.push_back(0); // size
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.resize(/* blockSizeKb */4 * 1024);
    new_archive_index_data.insert(new_archive_index_data.end(), new_download_ekey_vec.begin(), new_download_ekey_vec.end()); // last ekey
    std::string last_block_md5_string = md5((char*)&new_archive_index_data[0], /* blockSizeKb */4 * 1024);
    std::vector<uint8_t> last_block_md5 = md5_string_to_vector(last_block_md5_string);
    last_block_md5.resize(/* checksumSize */ 8);
    new_archive_index_data.insert(new_archive_index_data.end(), last_block_md5.begin(), last_block_md5.end());
    std::string toc_md5_string = md5((char*)&new_archive_index_data[/* blockSizeKb */4 * 1024], CHECKSUM_SIZE + /* checksumSize */ 8);
    std::vector<uint8_t> toc_md5 = md5_string_to_vector(toc_md5_string);
    toc_md5.resize(/* checksumSize */ 8);
    new_archive_index_data.insert(new_archive_index_data.end(), toc_md5.begin(), toc_md5.end());
    new_archive_index_data.push_back(1);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(4);
    new_archive_index_data.push_back(4);
    new_archive_index_data.push_back(4);
    new_archive_index_data.push_back(16);
    new_archive_index_data.push_back(8);
    new_archive_index_data.push_back(1); // entry count
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0); // footer md5
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    new_archive_index_data.push_back(0);
    std::string footer_md5_string = md5((char*)&new_archive_index_data[/* blockSizeKb */4 * 1024 + CHECKSUM_SIZE + /* checksumSize */ 8 + /* checksumSize */ 8], 12 + /* checksumSize */ 8);
    std::vector<uint8_t> footer_md5 = md5_string_to_vector(footer_md5_string);
    footer_md5.resize(/* checksumSize */ 8);
    new_archive_index_data.resize(new_archive_index_data.size() - /* checksumSize */ 8);
    new_archive_index_data.insert(new_archive_index_data.end(), footer_md5.begin(), footer_md5.end());
    std::string new_archive_index = md5((char*)&new_archive_index_data[/* blockSizeKb */4 * 1024 + CHECKSUM_SIZE + /* checksumSize */ 8], 12 + /* checksumSize */ 8 + /* checksumSize */ 8);
    archive_index[new_download_ekey] = CArchiveIndex(md5_string_to_vector(new_download_ekey), new_archive_index, 0, (uint32_t)new_download_data_encoded.size(), (uint32_t)archive_list.size());
    archive_list.push_back(new_archive_index);

    // calc archive group hash
    std::vector<uint8_t> archive_group_data;
    uint32_t numElements = 0;
    std::vector<std::vector<uint8_t>> archive_group_last_ekey;
    std::vector<std::vector<uint8_t>> archive_group_block_md5;
    for (auto i = archive_index.begin(); i != archive_index.end(); i++) {
        if (i->second.archive_index == -1) // from file index
            continue;
        numElements++;
        archive_group_data.insert(archive_group_data.end(), i->second.ekey.begin(), i->second.ekey.end());
        archive_group_data.push_back((uint8_t)(i->second.size >> 24));
        archive_group_data.push_back((uint8_t)(i->second.size >> 16));
        archive_group_data.push_back((uint8_t)(i->second.size >> 8));
        archive_group_data.push_back((uint8_t)(i->second.size >> 0));
        archive_group_data.push_back((uint8_t)(i->second.archive_index >> 8));
        archive_group_data.push_back((uint8_t)(i->second.archive_index >> 0));
        archive_group_data.push_back((uint8_t)(i->second.offset >> 24));
        archive_group_data.push_back((uint8_t)(i->second.offset >> 16));
        archive_group_data.push_back((uint8_t)(i->second.offset >> 8));
        archive_group_data.push_back((uint8_t)(i->second.offset >> 0));
        if (archive_group_data.size() % (/* blockSizeKb */4 * 1024) == 0xFF2) {
            archive_group_data.push_back(0); // padding
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_data.push_back(0);
            archive_group_last_ekey.push_back(i->second.ekey);
            size_t page_start = (archive_group_data.size() / (/* blockSizeKb */4 * 1024) - 1) * /* blockSizeKb */4 * 1024;
            std::string page_md5_string = md5((char*)&archive_group_data[page_start], /* blockSizeKb */4 * 1024);
            std::vector<uint8_t> page_md5 = md5_string_to_vector(page_md5_string);
            page_md5.resize(/* checksumSize */ 8);
            archive_group_block_md5.push_back(page_md5);
        }
    }
    size_t page_end = ((archive_group_data.size() + (/* blockSizeKb */4 * 1024) - 1) / (/* blockSizeKb */4 * 1024)) * (/* blockSizeKb */4 * 1024);
    if (page_end != archive_group_data.size()) {
        archive_group_data.resize(page_end, 0); // padding
        for (auto i = archive_index.rbegin(); i != archive_index.rend(); i++) {
            if (i->second.archive_index == -1) // from file index
                continue;
            archive_group_last_ekey.push_back(i->second.ekey);
            break;
        }
        size_t page_start = (archive_group_data.size() / (/* blockSizeKb */4 * 1024) - 1) * /* blockSizeKb */4 * 1024;
        std::string page_md5_string = md5((char*)&archive_group_data[page_start], /* blockSizeKb */4 * 1024);
        std::vector<uint8_t> page_md5 = md5_string_to_vector(page_md5_string);
        page_md5.resize(/* checksumSize */ 8);
        archive_group_block_md5.push_back(page_md5);
    }
    std::vector<uint8_t> toc_data;
    for (size_t i = 0; i < archive_group_last_ekey.size(); i++)
        toc_data.insert(toc_data.end(), archive_group_last_ekey[i].begin(), archive_group_last_ekey[i].end());
    for (size_t i = 0; i < archive_group_block_md5.size(); i++)
        toc_data.insert(toc_data.end(), archive_group_block_md5[i].begin(), archive_group_block_md5[i].end());
    archive_group_data.insert(archive_group_data.end(), toc_data.begin(), toc_data.end());
    toc_md5_string = md5((char*)&toc_data[0], (long)toc_data.size());
    toc_md5 = md5_string_to_vector(toc_md5_string);
    toc_md5.resize(/* checksumSize */ 8);
    std::vector<uint8_t> footer_data;
    footer_data.insert(footer_data.end(), toc_md5.begin(), toc_md5.end());
    footer_data.push_back(1);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(4);
    footer_data.push_back(6); // offsetBytes archive group index is 6
    footer_data.push_back(4);
    footer_data.push_back(16);
    footer_data.push_back(8);
    footer_data.push_back((uint8_t)(numElements >> 0));
    footer_data.push_back((uint8_t)(numElements >> 8));
    footer_data.push_back((uint8_t)(numElements >> 16));
    footer_data.push_back((uint8_t)(numElements >> 24));
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_data.push_back(0);
    footer_md5_string = md5((char*)&footer_data[/* checksumSize */ 8], (long)footer_data.size() - /* checksumSize */ 8);
    footer_md5 = md5_string_to_vector(footer_md5_string);
    footer_md5.resize(/* checksumSize */ 8);
    footer_data.resize(footer_data.size() - /* checksumSize */ 8);
    footer_data.insert(footer_data.end(), footer_md5.begin(), footer_md5.end());
    archive_group_data.insert(archive_group_data.end(), footer_data.begin(), footer_data.end());
    std::string new_archive_group_hash = md5((char*)&footer_data[0], (long)footer_data.size());
    Log.debug("new_archive_group_hash %s\n", new_archive_group_hash.c_str());

    // write build config
    std::string new_build_config_data;
    iss.clear();
    iss.seekg(0, std::ios::beg);
    iss.str(std::string(build_config_bin.begin(), build_config_bin.end()));
    while (std::getline(iss, line)) {
        if (string_start_with(line, "download = ")) {
            new_build_config_data += "download = " + new_download_ckey + " " + new_download_ekey + "\n";
        }
        else if (string_start_with(line, "download-size = ")) {
            new_build_config_data += "download-size = " + std::to_string(new_download_data.size()) + " " + std::to_string(new_download_data_encoded.size()) + "\n";
        }
        else
            new_build_config_data += line + "\n";
    }
    std::string new_build_config = md5((char*)&new_build_config_data[0], (long)new_build_config_data.size());
    std::vector<uint8_t> new_build_config_data_bin(new_build_config_data.begin(), new_build_config_data.end());

    // write cdn config
    std::string new_cdn_config_data;
    iss.clear();
    iss.seekg(0, std::ios::beg);
    iss.str(std::string(cdn_config_bin.begin(), cdn_config_bin.end()));
    while (std::getline(iss, line)) {
        if (string_start_with(line, "archives = ")) {
            new_cdn_config_data += "archives = ";
            for (size_t i = 0; i < archive_list.size(); i++)
                new_cdn_config_data += archive_list[i] + " ";
            new_cdn_config_data.resize(new_cdn_config_data.size() - 1); // delete last space
            new_cdn_config_data += "\n";
        }
        else if (string_start_with(line, "archive-group = "))
            new_cdn_config_data += "archive-group = " + new_archive_group_hash + "\n";
        else if (string_start_with(line, "archives-index-size = "))
            new_cdn_config_data += line + " " + std::to_string(new_archive_index_data.size()) + "\n";
        else
            new_cdn_config_data += line + "\n";
    }
    std::string new_cdn_config = md5((char*)&new_cdn_config_data[0], (long)new_cdn_config_data.size());
    std::vector<uint8_t> new_cdn_config_data_bin(new_cdn_config_data.begin(), new_cdn_config_data.end());

    // write versions
    std::string new_versions_data(version_info_bin.begin(), version_info_bin.end());
    string_replace(new_versions_data, build_config, new_build_config);
    string_replace(new_versions_data, cdn_config, new_cdn_config);
    std::vector<uint8_t> new_versions_data_bin(new_versions_data.begin(), new_versions_data.end());

    // write hosts
    // some hostname doesnt exist but whatever
    std::string new_hosts_data = hosts_data;
    new_hosts_data += "\n"; // incase file not end with newline
    new_hosts_data += "127.0.0.1 blizzard.gcdn.cloudn.co.kr\n";
    new_hosts_data += "127.0.0.1 blzddist1-a.akamaihd.net\n";
    new_hosts_data += "127.0.0.1 blzddistkr1-a.akamaihd.net\n";
    new_hosts_data += "127.0.0.1 level3.ssl.blizzard.com\n";
    new_hosts_data += "127.0.0.1 level3.blizzard.com\n";
    new_hosts_data += "127.0.0.1 us.cdn.blizzard.com\n";
    new_hosts_data += "127.0.0.1 eu.cdn.blizzard.com\n";
    new_hosts_data += "127.0.0.1 cn.cdn.blizzard.com\n";
    new_hosts_data += "127.0.0.1 tw.cdn.blizzard.com\n";
    new_hosts_data += "127.0.0.1 kr.cdn.blizzard.com\n";
    new_hosts_data += "127.0.0.1 sg.cdn.blizzard.com\n";
    new_hosts_data += "127.0.0.1 us.patch.battle.net\n";
    new_hosts_data += "127.0.0.1 eu.patch.battle.net\n";
    new_hosts_data += "127.0.0.1 cn.patch.battle.net\n";
    new_hosts_data += "127.0.0.1 tw.patch.battle.net\n";
    new_hosts_data += "127.0.0.1 kr.patch.battle.net\n";
    new_hosts_data += "127.0.0.1 sg.patch.battle.net\n";
    new_hosts_data += "127.0.0.1 us.version.battle.net\n";
    new_hosts_data += "127.0.0.1 eu.version.battle.net\n";
    new_hosts_data += "127.0.0.1 cn.version.battle.net\n";
    new_hosts_data += "127.0.0.1 tw.version.battle.net\n";
    new_hosts_data += "127.0.0.1 kr.version.battle.net\n";
    new_hosts_data += "127.0.0.1 sg.version.battle.net\n";
    std::vector<uint8_t> new_hosts_data_bin(new_hosts_data.begin(), new_hosts_data.end());

    // write nginx proxy config
    std::string cdn_ip = nslookup(CDN_HOST);
    std::string patch_ip = nslookup(PATCH_HOST);
    if (cdn_ip.empty()) {
        Log.error("failed to get " CDN_HOST " ip");
        return 1;
    }
    if (patch_ip.empty()) {
        Log.error("failed to get " PATCH_HOST " ip");
        return 1;
    }
    std::string nginx_config_data = "worker_processes  1;\n";
    nginx_config_data += "\n";
    nginx_config_data += "events {\n";
    nginx_config_data += "    worker_connections  1024;\n";
    nginx_config_data += "}\n";
    nginx_config_data += "\n";
    nginx_config_data += "http {\n";
    nginx_config_data += "    server {\n";
    nginx_config_data += "        location / {\n";
    nginx_config_data += "            proxy_set_header Host level3.blizzard.com;\n";
    nginx_config_data += "            proxy_pass http://" + cdn_ip +"/;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "        location /" PATH "/config/" + new_build_config.substr(0, 2) + "/" + new_build_config.substr(2, 2) + "/" + new_build_config + " {\n";
    nginx_config_data += "            root html;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "        location /" PATH "/config/" + new_cdn_config.substr(0, 2) + "/" + new_cdn_config.substr(2, 2) + "/" + new_cdn_config + " {\n";
    nginx_config_data += "            root html;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "        location /" PATH "/data/" + new_archive_index.substr(0, 2) + "/" + new_archive_index.substr(2, 2) + "/" + new_archive_index + " {\n";
    nginx_config_data += "            root html;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "        location /" PATH "/data/" + new_archive_index.substr(0, 2) + "/" + new_archive_index.substr(2, 2) + "/" + new_archive_index + ".index {\n";
    nginx_config_data += "            root html;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "    }\n";
    nginx_config_data += "    server {\n";
    nginx_config_data += "        listen 1119;\n";
    nginx_config_data += "        location / {\n";
    nginx_config_data += "            proxy_pass http://" + patch_ip + ":1119/;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "        location /" PRODUCT "/versions {\n";
    nginx_config_data += "            root html;\n";
    nginx_config_data += "        }\n";
    nginx_config_data += "    }\n";
    nginx_config_data += "}\n";
    std::vector<uint8_t> nginx_config_data_bin(nginx_config_data.begin(), nginx_config_data.end());

    // save generated data
    // build config
    std::string file_path = "nginx/html/" PATH "/config/";
    file_path += new_build_config.substr(0, 2);
    file_path.append(1, '/');
    file_path += new_build_config.substr(2, 2);
    system(std::string("mkdir \"" + file_path + "\"").c_str());
    file_path.append(1, '/');
    file_path += new_build_config;
    if (!write_file(file_path, new_build_config_data_bin)) {
        Log.error("failed to save new build config %s\n", file_path.c_str());
        return 1;
    }
    // cdn config
    file_path = "nginx/html/" PATH "/config/";
    file_path += new_cdn_config.substr(0, 2);
    file_path.append(1, '/');
    file_path += new_cdn_config.substr(2, 2);
    system(std::string("mkdir \"" + file_path + "\"").c_str());
    file_path.append(1, '/');
    file_path += new_cdn_config;
    if (!write_file(file_path, new_cdn_config_data_bin)) {
        Log.error("failed to save new cdn config %s\n", file_path.c_str());
        return 1;
    }
    // archive
    file_path = "nginx/html/" PATH "/data/";
    file_path += new_archive_index.substr(0, 2);
    file_path.append(1, '/');
    file_path += new_archive_index.substr(2, 2);
    system(std::string("mkdir \"" + file_path + "\"").c_str());
    file_path.append(1, '/');
    file_path += new_archive_index;
    if (!write_file(file_path, new_download_data_encoded)) {
        Log.error("failed to save new archive %s\n", file_path.c_str());
        return 1;
    }
    // archive index
    file_path += ".index";
    if (!write_file(file_path, new_archive_index_data)) {
        Log.error("failed to save new archive index %s\n", file_path.c_str());
        return 1;
    }
    // versions
    file_path = "nginx/html/" PRODUCT;
    system(std::string("mkdir \"" + file_path + "\"").c_str());
    file_path.append(1, '/');
    file_path += "versions";
    if (!write_file(file_path, new_versions_data_bin)) {
        Log.error("failed to save new versions %s\n", file_path.c_str());
        return 1;
    }
    // hosts file
    if (!write_file("C:\\Windows\\System32\\drivers\\etc\\hosts", new_hosts_data_bin)) {
        Log.error("failed to save new system hosts C:\\Windows\\System32\\drivers\\etc\\hosts\n");
        return 1;
    }
    // nginx config
    if (!write_file("nginx/conf/nginx.conf", nginx_config_data_bin)) {
        Log.error("failed to save nginx config nginx/conf/nginx.conf\n");
        return 1;
    }

    // start nginx server
    system("start /d nginx nginx.exe");

    // wait for user singal
    Log.info("proxy server running, CTRL + C to exit\n");
    hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    signal(SIGINT, onSignal);
    if (WaitForSingleObject(hEvent, INFINITE) != WAIT_OBJECT_0)
        Log.warn("wait for singal failed");
    else
        Log.info("received exit signal, exiting\n");

    // shutdown nginx server
    system("start /wait /d nginx nginx.exe -s stop");
    // revert hosts file
    if (!write_file("C:\\Windows\\System32\\drivers\\etc\\hosts", hosts_data_bin)) {
        Log.error("failed to revert system hosts C:\\Windows\\System32\\drivers\\etc\\hosts\n", file_path.c_str());
        return 1;
    }
    // delete generated files
    system("rmdir /s /q nginx\\html");
    return 0;
}