#include <WS2tcpip.h>
#include "util.h"
#pragma comment(lib, "ws2_32.lib")

std::wstring a2w(const std::string_view& str) {
    // https://github.com/actboy168/bee.lua/blob/4fef6bf84cd2d311bfc09a753bc992df91f2aa55/bee/win/unicode.cpp#L31
    std::wstring result;
    if (str.empty())
        return result;

    const int wlen = MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
    if (wlen <= 0)
        return result;

    result.resize(wlen);
    MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), result.data(), static_cast<int>(result.size()));
    return result;
}
std::string vector_to_hex_string(std::vector<uint8_t>& data) {
    std::string ret;
    ret.resize(data.size() * 2 + 1); // include null
    for (uint32_t i = 0; i < data.size(); i++)
        sprintf_s(&ret[i * 2], 3, "%02hhx", data[i]);
    ret.resize(ret.size() - 1); // exclue null
    return ret;
}
void print_vector_as_hex(std::vector<uint8_t>& data) {
    Log.write(vector_to_hex_string(data).c_str());
}
std::vector<uint8_t> md5_string_to_vector(std::string& str) {
    std::vector<uint8_t> md5;
    md5.resize(16);
    sscanf_s(str.c_str(), "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx", &md5[0], &md5[1], &md5[2], &md5[3], &md5[4], &md5[5], &md5[6], &md5[7], &md5[8], &md5[9], &md5[10], &md5[11], &md5[12], &md5[13], &md5[14], &md5[15]);
    return md5;
}
bool string_start_with(std::string& s, std::string start) {
    return s.rfind(start, 0) == 0;
}
void string_replace(std::string& Text, std::string Key, std::string Value) {
    // https://github.com/uakfdotb/ghostpp/blob/cf397544d08d05f8536272e3ab1b22cf03309d9f/ghost/util.cpp#L615
    // don't allow any infinite loops

    if (Value.find(Key) != std::string::npos)
        return;

    std::string::size_type KeyStart = Text.find(Key);

    while (KeyStart != std::string::npos) {
        Text.replace(KeyStart, Key.size(), Value);
        KeyStart = Text.find(Key);
    }
}
std::string nslookup(std::string address) {
    std::string ip;
    addrinfo* result = NULL;
    addrinfo* ptr = NULL;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    int ret = getaddrinfo(address.c_str(), NULL, &hints, &result);
    if (ret) {
        Log.error("getaddrinfo failed with %d, %d\n", ret, WSAGetLastError());
        return ip;
    }
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET) {
            ((sockaddr_in*)ptr->ai_addr)->sin_addr;
            ip += std::to_string((int)((sockaddr_in*)ptr->ai_addr)->sin_addr.S_un.S_un_b.s_b1);
            ip += ".";
            ip += std::to_string((int)((sockaddr_in*)ptr->ai_addr)->sin_addr.S_un.S_un_b.s_b2);
            ip += ".";
            ip += std::to_string((int)((sockaddr_in*)ptr->ai_addr)->sin_addr.S_un.S_un_b.s_b3);
            ip += ".";
            ip += std::to_string((int)((sockaddr_in*)ptr->ai_addr)->sin_addr.S_un.S_un_b.s_b4);
            break;
        }
    }
    return ip;
}