/////////////////////////////////////////////////////////////////////////
///@file rapid_serialize.h
///@brief	json序列化工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <map>
#include <list>
#include <deque>
#include <set>
#include <string>
#include <sstream>
#include <functional>
#include <assert.h>

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/allocators.h"
#include "rapidjson/pointer.h"
#include "rapidjson/stream.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "encoding.h"
#include "log.h"

namespace rapidjson{

template<>
inline bool Writer<rapidjson::EncodedOutputStream<rapidjson::UTF8<>, rapidjson::StringBuffer>, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag>::WriteDouble(double d) 
{
    if (internal::Double(d).IsNanOrInf()) {
        if (internal::Double(d).IsNan()) {
            PutReserve(*os_, 3);
            PutUnsafe(*os_, '"'); PutUnsafe(*os_, '-'); PutUnsafe(*os_, '"');
            return true;
        }
        if (internal::Double(d).Sign()) {
            PutReserve(*os_, 9);
            PutUnsafe(*os_, '-');
        }
        else
            PutReserve(*os_, 8);
        PutUnsafe(*os_, 'I'); PutUnsafe(*os_, 'n'); PutUnsafe(*os_, 'f');
        PutUnsafe(*os_, 'i'); PutUnsafe(*os_, 'n'); PutUnsafe(*os_, 'i'); PutUnsafe(*os_, 't'); PutUnsafe(*os_, 'y');
        return true;
    }

    char buffer[25];
    char* end = internal::dtoa(d, buffer, maxDecimalPlaces_);
    PutReserve(*os_, static_cast<size_t>(end - buffer));
    for (char* p = buffer; p != end; ++p)
        PutUnsafe(*os_, static_cast<typename UTF8<>::Ch>(*p));
    return true;
}
};

namespace RapidSerialize
{


template<class T>
struct StringSerialize {
    static std::string to_str(const T& v)
    {
        std::stringstream s;
        s << v;
        return s.str();
    }
    static T from_str(const std::string& utf8_str)
    {
        std::stringstream s(utf8_str);
        T val;
        return (s >> val).fail() ? T() : val;
    }
};

template<>
struct StringSerialize<std::string> {
    static std::string to_str(const std::string& utf8_string /*utf8_string*/)
    {
        return utf8_string;
    }
    static std::string from_str(const std::string& utf8_string)
    {
        return utf8_string;
    }
};


template<class TSerializer>
class Serializer
{
public:
    Serializer(rapidjson::Document* doc = NULL)
    {
        if(doc){
            m_doc = doc;
            m_del_doc = false;
        }else{
            m_doc = new rapidjson::Document;
            m_del_doc = true;
        }
        m_current_node = NULL;
    }
    virtual ~Serializer(){
        if (m_del_doc)
            delete m_doc;
    }
    bool ToString(std::string* json_utf8_string)
    {
        rapidjson::StringBuffer buffer(0, 2048);
        typedef rapidjson::EncodedOutputStream<rapidjson::UTF8<>, rapidjson::StringBuffer> OutputStream;
        OutputStream os(buffer, false);
        rapidjson::Writer<OutputStream, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(os);
        m_doc->Accept(writer);
        *json_utf8_string = std::string(buffer.GetString());
        return true;
    }
    
    bool FromString(const char* json_utf8_string)
    {
        rapidjson::StringStream buffer(json_utf8_string);
        typedef rapidjson::EncodedInputStream<rapidjson::UTF8<>, rapidjson::StringStream> InputStream;
        InputStream os(buffer);
        m_doc->ParseStream<rapidjson::kParseNanAndInfFlag, rapidjson::UTF8<> >(os);
        if (m_doc->HasParseError()){
            Log(LOG_ERROR, NULL, "json string (%s) parse fail", json_utf8_string);
            return false;
        }
        return true;
    }

    bool FromFile(const char* json_file)
    {
        FILE* fp = fopen(json_file, "rb");
        if (!fp)
            return false;
        char* readBuffer = new char[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        typedef rapidjson::EncodedInputStream<rapidjson::UTF8<>, rapidjson::FileReadStream> InputStream;
        InputStream os(is);
        m_doc->ParseStream<rapidjson::kParseNanAndInfFlag, rapidjson::UTF8<> >(os);
        delete[] readBuffer;
        if (m_doc->HasParseError()){
            Log(LOG_ERROR, NULL, "json file (%s) parse fail", json_file);
            return false;
        }
        fclose(fp);
        return true;
    }

    bool ToFile(const char* json_file)
    {
        FILE* fp = fopen(json_file, "wb"); // 非 Windows 平台使用 "w"
        if (!fp){
            Log(LOG_ERROR, NULL, "open json file (%s) for write fail", json_file);
            return false;
        }
        char* writeBuffer = new char[65536];
        rapidjson::FileWriteStream file_stream(fp, writeBuffer, sizeof(writeBuffer));
        typedef rapidjson::EncodedOutputStream<rapidjson::UTF8<>, rapidjson::FileWriteStream> OutputStream;
        OutputStream encoded_stream(file_stream, false);
        rapidjson::Writer<OutputStream, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(encoded_stream);
        m_doc->Accept(writer);
        delete[] writeBuffer;
        fclose(fp);
        return true;
    }

    template<class T>
    void FromVar(const T& val, rapidjson::Value* node = NULL)
    {
        is_save = true;
        if(node)
            Process(const_cast<T&>(val), *node);
        else
            Process(const_cast<T&>(val), *m_doc);
    }
    template<class T>
    void ToVar(T& val, rapidjson::Value* node = NULL)
    {
        is_save = false;
        if (node)
            Process(val, *node);
        else
            Process(val, *m_doc);
    }
    template<class T>
    void AfterStructChanged(T* data)
    {
    }
    template<class T>
    void AfterStructFieldChanged(T* data, const char* name)
    {
    }
    template<class TMap, class TMapIter>
    void AfterMapChanged(TMap* pmap, TMapIter* piter)
    {
    }
    template<class TMap, class TMapKey>
    bool BeforeMapDeleteKey(TMap* pmap, const TMapKey& key)
    {
        return true;
    }
    template<class TMapKey, class TMapValue>
    bool FilterMapItem(const TMapKey& key, TMapValue& value)
    {
        return true;
    }

    template<class T>
    void AddItem(T& data, const char* name)
    {
        if (is_save) {
            rapidjson::Value child_node;
            Process(data, child_node);
            rapidjson::Value jname;
            jname.SetString(name, m_doc->GetAllocator());
            m_current_node->AddMember(jname, child_node, m_doc->GetAllocator());
        } else {
            rapidjson::Value::MemberIterator itr = m_current_node->FindMember(name);
            if (itr == m_current_node->MemberEnd())
                return;
            if (itr->value.IsNull()) {
                m_deleted = true;
                return;
            }
            if (Process(data, itr->value)) {
                return;
            }
            static_cast<TSerializer*>(this)->AfterStructFieldChanged(&data, name);
        }
    }
    template<class TKey>
    void AddItemEnum(TKey& data, const char* name, std::map<TKey, const char*> enum_map)
    {
        if (is_save) {
            rapidjson::Value child_node;
            std::string value_str = enum_map[data];
            AddItem(value_str, name);
        } else {
            std::string value_str;
            rapidjson::Value::MemberIterator itr = m_current_node->FindMember(name);
            if (itr == m_current_node->MemberEnd())
                return;
            if (itr->value.IsNull()) {
                m_deleted = true;
                return;
            }
            if (Process(value_str, itr->value)) {
                return;
            }
            for (auto it = enum_map.begin(); it != enum_map.end(); ++it) {
                if (it->second == value_str) {
                    data = it->first;
                    static_cast<TSerializer*>(this)->AfterStructFieldChanged(&data, name);
                    break;
                }
            }
        }
    }

    template<class T>
    bool ProcessSeq(T& data, rapidjson::Value& node)
    {
        if (is_save) {
            node.SetArray();
            typedef T value_type;
            typedef typename T::value_type element_type;
            for (typename T::iterator it = data.begin(); it != data.end(); ++it) {
                rapidjson::Value value;
                Process(*it, value);
                node.PushBack(value, m_doc->GetAllocator());
            }
            return false;
        } else {
            typedef T value_type;
            typedef typename T::value_type element_type;
            for (auto& v : node.GetArray()) {
                element_type ev;
                Process(ev, v);
                data.insert(data.end(), ev);
            }
            return false;
        }
    }
    template<class V>
    bool Process(std::vector<V>& data, rapidjson::Value& node)
    {
        return ProcessSeq(data, node);
    }
    template<class V>
    bool Process(std::list<V>& data, rapidjson::Value& node)
    {
        return ProcessSeq(data, node);
    }
    template<class V>
    bool Process(std::deque<V>& data, rapidjson::Value& node)
    {
        return ProcessSeq(data, node);
    }
    template<class V>
    bool Process(std::set<V>& data, rapidjson::Value& node)
    {
        return ProcessSeq(data, node);
    }
    template<class K, class V>
    bool Process(std::map<K, V>& data, rapidjson::Value& node)
    {
        if (is_save){
            node.SetObject();
            for (typename std::map<K, V>::iterator it = data.begin(); it != data.end(); ++it) {
                if (static_cast<TSerializer*>(this)->FilterMapItem(it->first, it->second)){
                    rapidjson::Value value;
                    rapidjson::Value key(StringSerialize<K>::to_str(it->first), m_doc->GetAllocator());
                    Process(it->second, value);
                    node.AddMember(key, value, m_doc->GetAllocator());
                }
            }
            return false;
        }else{
            for (rapidjson::Value::MemberIterator it = node.MemberBegin(); it != node.MemberEnd(); ++it)
            {
                K key = StringSerialize<K>::from_str(it->name.GetString());
                if (it->value.IsNull()) {
                    if (static_cast<TSerializer*>(this)->BeforeMapDeleteKey(&data, key))
                        data.erase(key);
                } else {
                    V* pvalue = &data[key];
                    auto it2 = data.find(key);
                    if (Process(*pvalue, it->value)) {
                        if (static_cast<TSerializer*>(this)->BeforeMapDeleteKey(&data, key))
                            data.erase(key);
                    } else {
                        static_cast<TSerializer*>(this)->AfterMapChanged(&data, &it2);
                    }
                }
            }
            return false;
        }
        return false;
    }
    template<class K, class V>
    bool Process(std::multimap<K, V>& data, rapidjson::Value& node)
    {
        if (is_save) {
            node.SetObject();
            for (typename std::map<K, V>::iterator it = data.begin(); it != data.end(); ++it) {
                rapidjson::Value value;
                rapidjson::Value key(StringSerialize<K>::to_str(it->first), m_doc->GetAllocator());
                Process(it->second, value);
                node.AddMember(key, value, m_doc->GetAllocator());
            }
            return false;
        } else {
            for (rapidjson::Value::MemberIterator it = node.MemberBegin(); it != node.MemberEnd(); ++it)
            {
                K key = StringSerialize<K>::from_str(it->name.GetString());
                if (it->value.IsNull()) {
                    if (static_cast<TSerializer*>(this)->BeforeMapDeleteKey(&data, key))
                        data.erase(key);
                } else {
                    V* pvalue = &data[key];
                    auto it2 = data.find(key);
                    if (Process(*pvalue, it->value)) {
                        if (static_cast<TSerializer*>(this)->BeforeMapDeleteKey(&data, key))
                            data.erase(key);
                    } else {
                        static_cast<TSerializer*>(this)->AfterMapChanged(&data, &it2);
                    }
                }
            }
            return false;
        }
        return false;
    }

    //template<class T>
    //struct JsonConvertorMultiMap {
    //    static bool FromJson(TEncoder& encoder, T& data, rapidjson::Value& node, bool update = false)
    //    {
    //        typedef typename T::key_type key_type;
    //        typedef typename T::mapped_type mapped_type;
    //        if (!update) {
    //            data.clear();
    //        }
    //        for (rapidjson::Value::MemberIterator it = node.MemberBegin(); it != node.MemberEnd(); ++it)
    //        {
    //            key_type key = StringSerialize<key_type>::from_str(it->name.GetString());
    //            if (it->value.IsNull())
    //                continue;
    //            mapped_type value;
    //            if (!BindSelect<mapped_type>::json_bind_type::FromJson(doc, value, it->value, update)) {
    //                auto it2 = data.insert(std::pair<key_type, mapped_type>(key, value));
    //                NotifyJsAfterMapChanged(&data, &it2);
    //            }
    //        }
    //        return false;
    //    }
    //    static void ToJson(TEncoder& encoder, const T& data, rapidjson::Value& node)
    //    {
    //        node.SetObject();
    //        typedef typename T::key_type key_type;
    //        typedef typename T::mapped_type mapped_type;
    //        for (T::const_iterator it = data.begin(); it != data.end(); ++it) {
    //            rapidjson::Value value;
    //            rapidjson::Value key(StringSerialize<key_type>::to_str(it->first), doc.GetAllocator());
    //            BindSelect<mapped_type>::json_bind_type::ToJson(doc, it->second, value);
    //            node.AddMember(key, value, doc.GetAllocator());
    //        }
    //    }
    //};

    template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    bool Process(T& data, rapidjson::Value& node)
    {
        if (is_save) {
            if (sizeof(data) == 8)
                node.SetInt64(data);
            else
                node.SetInt(data);
            return false;
        } else{
            if (node.IsNumber()) {
                if (sizeof(data) == 8)
                    data = node.GetInt64();
                else
                    data = node.GetInt();
            } else {
                if (sizeof(data) == 8)
                    data = std::numeric_limits<long long>::max();
                else
                    data = std::numeric_limits<int>::max();
            }
            return false;
        }
    }
    template<class T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    bool Process(T& data, rapidjson::Value& node)
    {
        if (is_save) {
            node.SetDouble(data);
            return false;
        } else {
            if (node.IsDouble()) {
                data = node.GetDouble();
            } else if (node.IsInt()) {
                data = node.GetInt();
            } else {
                data = std::numeric_limits<double>::quiet_NaN();
            }
            return false;
        }
    }
    template<class T, typename std::enable_if<std::is_class<T>::value, int>::type = 0>
    auto Process(T& data, rapidjson::Value& node)
    {
        auto orign_current_node = m_current_node;
        m_current_node = &node;
        if (is_save) {
            m_current_node->SetObject();
            static_cast<TSerializer*>(this)->DefineStruct(data);
            m_current_node = orign_current_node;
            return false;
        } else {
            m_deleted = false;
            if (!m_current_node->IsObject())
                return false;
            static_cast<TSerializer*>(this)->DefineStruct(data);
            m_current_node = orign_current_node;
            if (m_deleted)
                return true;
            static_cast<TSerializer*>(this)->AfterStructChanged(&data);
            return false;
        }
    }
    template<size_t N>
    bool Process(const char(&data)[N], rapidjson::Value& node)
    {
        if (is_save) {
            std::string ws = data;
            node.SetString(ws, m_doc->GetAllocator());
            return false;
        } else {
            if (node.IsString()) {
                std::string str = node.GetString();
                strncpy((char*)data, str.c_str(), N);
            }
            return false;
        }
    }
    bool Process(bool& data, rapidjson::Value& node)
    {
        if (is_save) {
            node.SetBool(data);
            return false;
        } else {
            if (node.IsBool())
                data = node.GetBool();
            return false;
        }
    }
    bool Process(std::string& data, rapidjson::Value& node)
    {
        if (is_save) {
            node.SetString(data, m_doc->GetAllocator());
            return false;
        } else {
            if (node.IsString()) {
                data = node.GetString();
            }
            return false;
        }
    }
    bool m_del_doc;
    rapidjson::Document* m_doc;
    rapidjson::Value* m_current_node;
    bool is_save;
    bool m_deleted;
};

}
