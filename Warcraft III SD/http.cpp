#include "http.h"
#include "util.h"

bool HTTPGetFile(std::string url, std::vector<uint8_t>& data) {
    // https://github.com/d07RiV/blizzget/blob/2764ef6a378a1a3e85bf002d946898b417d747d2/src/base/http.cpp#L14
    Log.info("getting file from url %s\n", url.c_str());

    std::wstring url16 = a2w(url);
    URL_COMPONENTS urlComp;
    memset(&urlComp, 0, sizeof urlComp);
    urlComp.dwStructSize = sizeof urlComp;
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;
    
    if (!WinHttpCrackUrl(url16.c_str(), (DWORD)url16.size(), 0, &urlComp)) {
        Log.error("WinHttpCrackUrl failed\n");
        return false;
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

    HINTERNET hSession = WinHttpOpen(L"ShrineTips", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) {
        Log.error("WinHttpOpen failed\n");
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        Log.error("WinHttpConnect failed\n");
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path.c_str(),
        L"HTTP/1.1",
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) | WINHTTP_FLAG_BYPASS_PROXY_CACHE
    );
    if (!hRequest) {
        Log.error("WinHttpOpenRequest failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)) {
        Log.error("WinHttpSendRequest failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        Log.error("WinHttpReceiveResponse failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    DWORD statusCode = 0;
    DWORD size = sizeof statusCode;
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX)) {
        Log.error("WinHttpQueryHeaders status_code failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (statusCode != HTTP_STATUS_OK) {
        Log.error("received HTTP status code %d\n", statusCode);
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    DWORD data_len = 0;
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &data_len, &size, WINHTTP_NO_HEADER_INDEX)) {
        Log.error("WinHttpQueryHeaders content_length failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }
    data.resize(data_len);

    if (!WinHttpReadData(hRequest, (void*)&data[0], (DWORD)data.size(), &data_len)) {
        Log.error("WinHttpReadData failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (data_len != data.size()) {
        Log.error("expected size %d, got %d\n", data.size(), data_len);
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    WinHttpCloseHandle(hSession);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hRequest);
    return true;
}
bool HTTPGetFile(const char* url, std::vector<uint8_t>& data) {
    return HTTPGetFile(std::string(url), data);
}
bool HTTPGetFile(char* url, std::vector<uint8_t>& data) {
    return HTTPGetFile(std::string(url), data);
}
bool HTTPGetFilePartial(std::string url, std::vector<uint8_t>& data, uint64_t offset, uint64_t size) {
    // copied from above with tweaks
    Log.info("getting partial file from url %s, offset %llu, size %llu\n", url.c_str(), offset, size);

    std::wstring url16 = a2w(url);
    URL_COMPONENTS urlComp;
    memset(&urlComp, 0, sizeof urlComp);
    urlComp.dwStructSize = sizeof urlComp;
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;

    if (!WinHttpCrackUrl(url16.c_str(), (DWORD)url16.size(), 0, &urlComp)) {
        Log.error("WinHttpCrackUrl failed\n");
        return false;
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

    HINTERNET hSession = WinHttpOpen(L"ShrineTips", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) {
        Log.error("WinHttpOpen failed\n");
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        Log.error("WinHttpConnect failed\n");
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path.c_str(),
        L"HTTP/1.1",
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) | WINHTTP_FLAG_BYPASS_PROXY_CACHE
    );
    if (!hRequest) {
        Log.error("WinHttpOpenRequest failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        return false;
    }

    std::wstring range_header = L"Range: bytes=";
    range_header += std::to_wstring(offset);
    range_header.append(1, L'-');
    range_header += std::to_wstring(offset + size - 1);
    if (!WinHttpSendRequest(hRequest, range_header.c_str(), (DWORD)range_header.size(), NULL, 0, 0, 0)) {
        Log.error("WinHttpSendRequest failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        Log.error("WinHttpReceiveResponse failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    DWORD statusCode = 0;
    DWORD DWORD_size = sizeof statusCode;
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &DWORD_size, WINHTTP_NO_HEADER_INDEX)) {
        Log.error("WinHttpQueryHeaders status_code failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (statusCode != HTTP_STATUS_PARTIAL_CONTENT) {
        Log.error("received HTTP status code %d\n", statusCode);
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    DWORD data_len = 0;
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &data_len, &DWORD_size, WINHTTP_NO_HEADER_INDEX)) {
        Log.error("WinHttpQueryHeaders content_length failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (data_len != size) {
        Log.error("expected size %d, got %d\n", size, data_len);
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }
    data.resize(data_len);

    if (!WinHttpReadData(hRequest, (void*)&data[0], (DWORD)data.size(), &data_len)) {
        Log.error("WinHttpReadData failed\n");
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    if (data_len != data.size()) {
        Log.error("expected size %d, got %d\n", data.size(), data_len);
        WinHttpCloseHandle(hSession);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hRequest);
        return false;
    }

    WinHttpCloseHandle(hSession);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hRequest);
    return true;
}
bool HTTPGetFilePartial(const char* url, std::vector<uint8_t>& data, uint64_t offset, uint64_t size) {
    return HTTPGetFilePartial(std::string(url), data, offset, size);
}
bool HTTPGetFilePartial(char* url, std::vector<uint8_t>& data, uint64_t offset, uint64_t size) {
    return HTTPGetFilePartial(std::string(url), data, offset, size);
}