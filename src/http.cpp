/////////////////////////////////////////////////////////////////////////
///@file http.cpp
///@brief	http 工具函数
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "http.h"

#include <curl/curl.h>


static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    std::string* mem = (std::string*)userp;
    mem->append((char*)contents, realsize);
    return realsize;
}

int HttpDecompress(Byte* zdata, uLong nzdata, Byte *data, uLong *ndata)
{
    int err = 0;
    z_stream d_stream = { 0 }; /* decompression stream */
    static char dummy_head[2] =
    {
        0x8 + 0x7 * 0x10,
        (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };
    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    d_stream.next_in = zdata;
    d_stream.avail_in = 0;
    d_stream.next_out = data;
    if (inflateInit2(&d_stream, 47) != Z_OK) return -1;
    while (d_stream.total_out < *ndata && d_stream.total_in < nzdata)
    {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        if ((err = inflate(&d_stream, Z_NO_FLUSH)) == Z_STREAM_END) break;
        if (err != Z_OK)
        {
            if (err == Z_DATA_ERROR)
            {
                d_stream.next_in = (Bytef*)dummy_head;
                d_stream.avail_in = sizeof(dummy_head);
                if ((err = inflate(&d_stream, Z_NO_FLUSH)) != Z_OK)
                {
                    return -1;
                }
            } else return -1;
        }
    }
    if (inflateEnd(&d_stream) != Z_OK) return -1;
    *ndata = d_stream.total_out;
    return 0;
}

long HttpGet(const char* url, std::string* response)
{
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if (!curl)
        return -1;
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
    //note: 因使用的libcurl库未挂接zlib, 不能自动对gzip内容解压, 故补解压代码
    if (response->size() > 2 
        && ((unsigned char)response->at(0) == 0x1f) 
        && ((unsigned char)response->at(1) == 0x8b)) {
        std::string compressed_data = *response;
        long decompress_buf_len = 8096 * 1024;//此处长度需要足够大以容纳解压缩后数据
        response->resize(decompress_buf_len);
        int err = HttpDecompress((Byte*)compressed_data.data(), (uLong)compressed_data.size(), (Byte*)response->data(), (uLong*)&decompress_buf_len);
        if (err != Z_OK)
            return 1;
        response->resize(decompress_buf_len);
    }
    return res;
}

