#include <curl/curl.h>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <new>

struct Mp1CurlHandle { long response_code = 0; };

extern "C" {
CURLcode curl_global_init(long) { return CURLE_OK; }
void curl_global_cleanup(void) {}
CURL* curl_easy_init(void) {
    return reinterpret_cast<CURL*>(new (std::nothrow) Mp1CurlHandle());
}
CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) {
    if (!curl) return CURLE_FAILED_INIT;
    va_list args;
    va_start(args, option);
    switch (option) {
        case CURLOPT_URL:
        case CURLOPT_USERAGENT:
        case CURLOPT_WRITEDATA:
        case CURLOPT_WRITEFUNCTION:
        case CURLOPT_HTTPHEADER:
            (void)va_arg(args, void*);
            break;
        case CURLOPT_TIMEOUT:
        case CURLOPT_FOLLOWLOCATION:
        case CURLOPT_SSL_VERIFYPEER:
        case CURLOPT_SSL_VERIFYHOST:
            (void)va_arg(args, long);
            break;
        default:
            va_end(args);
            return CURLE_UNSUPPORTED_PROTOCOL;
    }
    va_end(args);
    return CURLE_OK;
}
CURLcode curl_easy_perform(CURL*) {
    // The initial MP1 Android port is offline/local-only. RecompFrontend still
    // links its optional online mod-store client, so satisfy the ABI without
    // silently performing network requests.
    return CURLE_UNSUPPORTED_PROTOCOL;
}
CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) {
    if (!curl) return CURLE_FAILED_INIT;
    va_list args;
    va_start(args, info);
    CURLcode ret = CURLE_UNSUPPORTED_PROTOCOL;
    if (info == CURLINFO_RESPONSE_CODE) {
        long* out = va_arg(args, long*);
        if (out) *out = reinterpret_cast<Mp1CurlHandle*>(curl)->response_code;
        ret = CURLE_OK;
    }
    va_end(args);
    return ret;
}
void curl_easy_cleanup(CURL* curl) { delete reinterpret_cast<Mp1CurlHandle*>(curl); }
const char* curl_easy_strerror(CURLcode code) {
    return code == CURLE_OK ? "No error" : "Networking disabled in MP1 Android build";
}
struct curl_slist* curl_slist_append(struct curl_slist* list, const char* data) {
    auto* item = static_cast<curl_slist*>(std::malloc(sizeof(curl_slist)));
    if (!item) return list;
    const char* src = data ? data : "";
    const size_t len = std::strlen(src);
    item->data = static_cast<char*>(std::malloc(len + 1));
    if (!item->data) { std::free(item); return list; }
    std::memcpy(item->data, src, len + 1);
    item->next = nullptr;
    if (!list) return item;
    curl_slist* end = list;
    while (end->next) end = end->next;
    end->next = item;
    return list;
}
void curl_slist_free_all(struct curl_slist* list) {
    while (list) {
        curl_slist* next = list->next;
        std::free(list->data);
        std::free(list);
        list = next;
    }
}
}
