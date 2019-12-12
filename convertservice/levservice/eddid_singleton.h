#pragma once
/****
*** @file  eddid_singleton.h
*** @brief 封装单例模板
*** @author xiay
*** @version v0.1.1
*** @date   2019-11-22
*****/

#include <memory>
#include <mutex>

namespace eddid { namespace utility {
template<typename T>
class CSingleTonTemplate
{
    CSingleTonTemplate(const CSingleTonTemplate&) = delete;
    CSingleTonTemplate& operator=(const CSingleTonTemplate&) = delete;
public:
    virtual ~CSingleTonTemplate() = default;
protected:
    CSingleTonTemplate() = default;
public:
    static T* instance();
private:
    static std::unique_ptr<T> instance_ptr_;
};
template<typename T>
std::unique_ptr<T> utility::CSingleTonTemplate<T>::instance_ptr_;

template<typename T>
T* utility::CSingleTonTemplate<T>::instance()
{
    static std::once_flag onceflag;
    std::call_once(onceflag, [&]() {if (!instance_ptr_) { instance_ptr_.reset(new T); }});
    return instance_ptr_.get();
}

#define  DEF_FRIEND_CLASS(thisClass)                            \
    private:                                                    \
        friend class eddid::utility::CSingleTonTemplate<thisClass>;
}
}


