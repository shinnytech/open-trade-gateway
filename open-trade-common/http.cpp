/////////////////////////////////////////////////////////////////////////
///@file http.cpp
///@brief	http 工具函数
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "http.h"
#include <curl/curl.h>
#include "log.h"

using namespace std;

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    std::string* mem = (std::string*)userp;
    mem->append((char*)contents, realsize);
    return realsize;
}

long HttpGet(const char* url, std::string* response)
{
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
	if (!curl)
	{
		return -1;
	}        
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    struct curl_slist* chunk = NULL;
    chunk = curl_slist_append(chunk, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    if (response) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)response);
    }
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(chunk);
    return res;
}

