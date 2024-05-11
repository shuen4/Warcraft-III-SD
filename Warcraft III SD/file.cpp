#include "file.h"

bool read_file(char* filepath, std::vector<uint8_t>& data) {
    std::ifstream in(filepath, std::ios::binary);
    if (in.fail()) {
        Log.error("failed to read %s\n", filepath);
        return false;
    }
    in.seekg(0, std::ios::end);
    data.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read((char*)&data[0], data.size());
    in.close();
    return true;
}
bool read_file(const char* filepath, std::vector<uint8_t>& data) {
    return read_file((char*)filepath, data);
}
bool read_file(std::string filepath, std::vector<uint8_t>& data) {
    return read_file(filepath.c_str(), data);
}
bool write_file(char* filepath, std::vector<uint8_t>& data) {
    std::ofstream out(filepath, std::ios::out | std::ios::binary);
    if (out.fail()) {
        Log.error("failed to write %s\n", filepath);
        return false;
    }
    out.write((char*)&data[0], data.size());
    return true;
}
bool write_file(const char* filepath, std::vector<uint8_t>& data) {
    return write_file((char*)filepath, data);
}
bool write_file(std::string filepath, std::vector<uint8_t>& data) {
    return write_file(filepath.c_str(), data);
}
bool file_exist(std::string file) {
    struct stat fileinfo;
    return stat(file.c_str(), &fileinfo) == 0;
}