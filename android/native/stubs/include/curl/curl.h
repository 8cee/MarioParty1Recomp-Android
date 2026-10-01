#ifndef MP1_ANDROID_CURL_STUB_H
#define MP1_ANDROID_CURL_STUB_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void CURL;
typedef enum {
    CURLE_OK=0, CURLE_UNSUPPORTED_PROTOCOL=1, CURLE_FAILED_INIT=2,
    CURLE_COULDNT_RESOLVE_HOST=6, CURLE_COULDNT_CONNECT=7,
    CURLE_WRITE_ERROR=23, CURLE_OUT_OF_MEMORY=27,
    CURLE_OPERATION_TIMEDOUT=28, CURLE_PEER_FAILED_VERIFICATION=60
} CURLcode;
typedef enum {
    CURLOPT_WRITEDATA=10001, CURLOPT_URL=10002, CURLOPT_TIMEOUT=13,
    CURLOPT_FOLLOWLOCATION=52, CURLOPT_SSL_VERIFYPEER=64,
    CURLOPT_SSL_VERIFYHOST=81, CURLOPT_USERAGENT=10018,
    CURLOPT_WRITEFUNCTION=20011, CURLOPT_HTTPHEADER=10023
} CURLoption;
typedef enum { CURLINFO_RESPONSE_CODE=0x200002 } CURLINFO;
#define CURL_GLOBAL_DEFAULT ((long)3)
struct curl_slist { char* data; struct curl_slist* next; };
typedef size_t (*curl_write_callback)(void*, size_t, size_t, void*);
CURLcode curl_global_init(long);
void curl_global_cleanup(void);
CURL* curl_easy_init(void);
CURLcode curl_easy_setopt(CURL*, CURLoption, ...);
CURLcode curl_easy_perform(CURL*);
CURLcode curl_easy_getinfo(CURL*, CURLINFO, ...);
void curl_easy_cleanup(CURL*);
const char* curl_easy_strerror(CURLcode);
struct curl_slist* curl_slist_append(struct curl_slist*, const char*);
void curl_slist_free_all(struct curl_slist*);
#ifdef __cplusplus
}
#endif
#endif
