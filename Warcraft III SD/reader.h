#pragma once
#include <vector>
#include <string>
#include "util.h"

class CReader {
public:
    CReader(std::vector<uint8_t> data, int curPos = 0) :data(data), curPos(curPos) {}
    template <class t>
    t Read() {
        t ret = ReadMemory<t>((uint64_t)&data[0] + curPos);
        curPos += sizeof(t);
        return ret;
    }
    template<class t>
    std::vector<t> ReadVector(uint32_t count) {
        std::vector<t> ret;
        ret.reserve(count);
        ret.insert(ret.end(), &data[0] + curPos, &data[0] + curPos + count);
        curPos += count;
        return ret;
    }
    std::string ReadString(int length = -1) {
        std::string ret;
        if (length == -1)
            ret = std::string((char*)&data[0] + curPos);
        else {
            ret.reserve(length);
            ret.append((char*)&data[0] + curPos, length);
        }
        curPos += ret.size() + 1;
        return ret;
    }
    std::vector<uint8_t> data;
    size_t curPos;
};