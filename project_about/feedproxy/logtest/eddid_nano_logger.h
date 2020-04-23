#pragma once

#include "eddid_common.h"
#include "concurrentqueue.h"
#include "eddid_semlock.h"

namespace eddid{
namespace utility{
#define DEFAULT_STACK_BUFFER_SIZE 512
#define DEFAULT_STACK_SIZE 256
    
enum class ENU_LOG_LEVEL : uint8_t
{
    ELL_LOG_STDERR    = 0,     
    ELL_LOG_EMERG     = 1,
    ELL_LOG_ALERT     = 2,    
    ELL_LOG_CRIT      = 3,
    ELL_LOG_ERR       = 4,
    ELL_LOG_WARN      = 5,
    ELL_LOG_NOTICE    = 6,
    ELL_LOG_INFO      = 7,
    ELL_LOG_DEBUG     = 8
};

#define LOG_LEVEL_MAP(XX)                        \
XX(ENU_LOG_LEVEL::ELL_LOG_STDERR, "STDERR")      \
XX(ENU_LOG_LEVEL::ELL_LOG_EMERG,  "EMERG")       \
XX(ENU_LOG_LEVEL::ELL_LOG_ALERT, "ALERT")        \
XX(ENU_LOG_LEVEL::ELL_LOG_CRIT, "CRIT")          \
XX(ENU_LOG_LEVEL::ELL_LOG_ERR, "ERR")            \
XX(ENU_LOG_LEVEL::ELL_LOG_WARN, "WARN")          \
XX(ENU_LOG_LEVEL::ELL_LOG_NOTICE, "NOTICE")      \
XX(ENU_LOG_LEVEL::ELL_LOG_INFO,  "INFO")         \
XX(ENU_LOG_LEVEL::ELL_LOG_DEBUG, "DEBUG")
    
inline char const *to_string(ENU_LOG_LEVEL level)
{
#define LEVLE_TO_STRING(_l, _n) case _l: return _n;
    switch(level)
    {
        LOG_LEVEL_MAP(LEVLE_TO_STRING)
    default:
        break;  
    }
    return "KNOWN";
#undef LEVLE_TO_STRING
}

// 私有命名空间
namespace _{
static uint64_t TimestampNow()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// 精确到毫秒级的时间戳
static void format_timestamp(std::ostream & os, uint64_t timestamp)
{
    auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(timestamp));
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
    std::time_t t = sec.count();
    os << '[' << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << '.' << timestamp % 1000 << ']';
}

// 线程
static std::thread::id this_thread_id()
{
    static thread_local const std::thread::id id = std::this_thread::get_id();
    return id;
}

// 进程ID
static uint64_t this_process_id()
{
#ifdef __linux__
    static uint64_t id = getpid();
#elif defined(_WIN32) || defined(_WIN64)
    static uint64_t id = GetCurrentProcessId();
#endif 
    return id;
}

template< typename T, typename U>
struct TupleIndex;

template< typename T, typename ...Args>
struct TupleIndex<T, std::tuple<T, Args... >>
{
    static constexpr const std::size_t value = 0;
};

template<typename T, typename U, typename ...Args>
struct TupleIndex<T, std::tuple<U, Args...>>
{
    static constexpr const std::uint8_t value = 1 + TupleIndex<T, std::tuple< Args... > >::value;
};

template<typename T>
struct is_c_str;

template<typename T>
struct is_c_str : std::integral_constant<bool, std::is_same_v<char*, std::decay_t<T> > || std::is_same_v<const char*, std::decay_t<T>>>
{};

template<typename T>
constexpr bool is_c_str_v = is_c_str<T>::value;

struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

template <typename Key>
using HashType = typename std::conditional<std::is_enum<Key>::value, EnumClassHash, std::hash<Key>>::type;

template <typename T>
struct LogSupportRegist : std::false_type {};

} // end of namespace _

#define RegistLogSupport(class_name) template <> struct eddid::utility::_::LogSupportRegist<class_name> : std::true_type {};

class LogLine
{
public:
    using SupportedTypes = std::tuple<bool, int8_t, uint8_t, uint32_t, uint64_t, int32_t, int64_t, double, float, int16_t, uint16_t, const char*, char *>;
public:
    
    LogLine(ENU_LOG_LEVEL level, const char* file, const char* func, uint32_t line)
        : buffer_size_(sizeof(stack_buffer_))
        , used_byted_(0)
    {
        Encode0(_::TimestampNow(), _::this_process_id(), _::this_thread_id(), file, func, line, level);
    }
    virtual ~LogLine() = default;
    
    LogLine(LogLine&&) = default;
    LogLine& operator=(LogLine&&) = default;

public:
    template<typename Arg>
    inline LogLine& operator<<(Arg arg)
    {
        if constexpr(std::is_arithmetic_v<Arg>) {
            Encode(_::TupleIndex<Arg, SupportedTypes>::value, arg);
        } else if constexpr(_::is_c_str_v<Arg>) {
            EncodeStr(arg);
        } else if constexpr(_::LogSupportRegist<Arg>::value) {
            (*this) << arg;
        }
        return *this;
    }

    ENU_LOG_LEVEL get_level()
    {
        char * b = !head_buffer_ ? stack_buffer_ : head_buffer_.get();
        const size_t offset = sizeof(uint64_t)
                              + sizeof(std::thread::id)
                              + sizeof(const char*)
                              + sizeof(const char*)
                              + sizeof(uint32_t);

        return *reinterpret_cast <ENU_LOG_LEVEL *>(b + offset);
    }

    void stringify(std::ostream& os)
    {
        char* buf = !head_buffer_ ? stack_buffer_: head_buffer_.get();
        char const* const end = buf + used_byted_;
        uint64_t timestamp = *(reinterpret_cast<uint64_t*>(buf));
        buf += sizeof(uint64_t);
        uint64_t process_id = *(reinterpret_cast<uint64_t*>(buf));
        buf += sizeof(uint64_t);
        std::thread::id thread_id = *(reinterpret_cast<std::thread::id*>(buf));
        buf += sizeof(std::thread::id);
        const char* file = *(reinterpret_cast<const char* *>(buf));
        buf += sizeof(const char*);
        const char* func = *(reinterpret_cast<const char* *>(buf));
        buf += sizeof(const char*);
        uint32_t line = *(reinterpret_cast<uint32_t*>(buf));
        buf += sizeof(uint32_t);
        ENU_LOG_LEVEL log_level = *(reinterpret_cast<ENU_LOG_LEVEL*>(buf));
        buf += sizeof(ENU_LOG_LEVEL);

        _::format_timestamp(os, timestamp);

        os << '[' << to_string(log_level) << "][" << process_id << "][" << thread_id << "]["<< file << ':' << func << ':' << line << ']';
        Decode(os, buf, end);
        os << std::endl;
    }
private:
    char* GetBuffer()
    {
        return !head_buffer_ ? &stack_buffer_[used_byted_] : &(head_buffer_.get())[used_byted_];
    }
    
    void ResizeBuffer(size_t size)
    {
        size_t req_size = used_byted_ + size;
        if (req_size <= buffer_size_) {
            return;
        }

        if (!head_buffer_) {
            buffer_size_ = MAX(static_cast<size_t>(DEFAULT_STACK_BUFFER_SIZE), req_size);
            head_buffer_.reset(new char[buffer_size_],  std::default_delete<char[]>());
            memcpy(head_buffer_.get(), stack_buffer_, used_byted_);
        } else {
            buffer_size_ = MAX(static_cast<size_t>(buffer_size_ * 2), req_size);
            std::unique_ptr<char[]> tmp(new char[buffer_size_]{0}, std::default_delete<char[]>());
            memcpy(tmp.get(), head_buffer_.get(), used_byted_);
            head_buffer_.swap(tmp);
        }
    }
private:
    template<typename Arg>
    void EncodeStr(Arg arg)
    {
        if (arg == nullptr) {
            return;
        }

        size_t len = strlen(arg);
        if (len == 0) {
            return;
        }

        ResizeBuffer(1 + len + 1);
        char* buf = GetBuffer();
        auto type_id = _::TupleIndex<char*, SupportedTypes>::value;
        *reinterpret_cast<uint8_t*>(buf++) = static_cast<uint8_t>(type_id);
        memcpy(buf, arg, len + 1);
        used_byted_ += 1 + len + 1;
    }
    
    template<typename ...Arg>
    void Encode0(Arg... arg)
    {
        ((*reinterpret_cast<Arg*>(GetBuffer()) = arg, used_byted_ += sizeof(Arg)), ...);
    }
    
    template<typename Arg>
    void Encode(uint8_t id, Arg arg)
    {
        ResizeBuffer(sizeof(arg) + sizeof(uint8_t));
        Encode0(id, arg);
    }

     template <typename Arg>
     char* Decode0(std::ostream& os, char* buf, Arg* dummy)
     {
         if constexpr(std::is_arithmetic_v<Arg>) {
             os << (*reinterpret_cast<Arg*>(buf));
             return buf + sizeof(Arg);
         } else if constexpr(std::is_same_v<const char*, Arg>){
             const char* s = (*reinterpret_cast<const char* *>(buf));
             os << s;
             return buf + sizeof(const char*);
         } else if constexpr(std::is_same_v<char*, Arg>) {
             while(*buf != 0x00) {
                 os << *buf;
                 ++buf;
             }
             return ++buf;
         }
     }

    template<size_t I>
    using ele_type_p = std::tuple_element_t<I, SupportedTypes>*;
    void Decode(std::ostream& os, char* start, char const * const end)
    {
        if (start == end) {
            return;
        }
        int id = static_cast <int>(*start); 
        start++;

        switch (id) 
        {
        case 0:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<0>>(nullptr)), end);
            return;
        case 1:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<1>>(nullptr)), end);
            return;
        case 2:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<2>>(nullptr)), end);
            return;
        case 3:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<3>>(nullptr)), end);
            return;
        case 4:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<4>>(nullptr)), end);
            return;
        case 5:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<5>>(nullptr)), end);
            return;
        case 6:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<6>>(nullptr)), end);
            return;
        case 7:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<7>>(nullptr)), end);
            return;
        case 8:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<8>>(nullptr)), end);
            return;
        case 9:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<9>>(nullptr)), end);
            return;
        case 10:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<10>>(nullptr)), end);
            return;
        case 11:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<11>>(nullptr)), end);
            return;
        case 12:
            Decode(os, Decode0(os, start, static_cast<ele_type_p<12>>(nullptr)), end);
            return;
        }
    }

private:
    size_t buffer_size_;
    size_t used_byted_;
    std::unique_ptr<char []> head_buffer_;
    char stack_buffer_[DEFAULT_STACK_SIZE - 2 * sizeof(size_t) - sizeof(decltype(head_buffer_)) - 8];
};

template<typename Key>
class KeyLogLine : public LogLine
{
public:
    KeyLogLine(KeyLogLine &&) noexcept = default;
    KeyLogLine& operator=(KeyLogLine &&) noexcept = default;

    KeyLogLine(Key key, ENU_LOG_LEVEL level, char const * file, char const * function, uint32_t line)
            : LogLine(level, file, function, line), key_(key) {}

    ~KeyLogLine() final = default;

public:
    template < typename Arg, class = void>
    KeyLogLine& operator << (Arg arg)
    {
        LogLine::operator << (arg);
        return *this;
    }

public:
    Key GetKey() const { return key_; }

private:
    Key key_;
};

class ng_base_writer
{
public:
    ng_base_writer() = default;

    virtual ~ng_base_writer() = default;
public:
    virtual int Initialize() = 0;
    virtual void Release() = 0;
public:
    virtual void Write(std::shared_ptr<LogLine> & log_ptr) = 0;
};

struct log_content_size : public moodycamel::ConcurrentQueueDefaultTraits
{
    static const size_t BLOCK_SIZE = 64;
};
class NGNanoLogger
{
public:
    NGNanoLogger(const std::shared_ptr<ng_base_writer>& writer)
        : is_initialize_(false)
        , is_exit_(false)
        , log_producer_(log_content_)
        , log_consumer_(log_content_)
    {
        writer_ = writer;
    }

    ~NGNanoLogger()
    {
        Release();
    }

public:
    // 初始化环境参数
    int Initialize()
    {
        int ret_code(0);
        if (is_initialize_) {
            return ret_code;
        }

        if (!log_worker_semaphore_) {
            log_worker_semaphore_ = std::make_unique<ngp::utility::ng_semaphore>(false);
        }

        if (!log_worker_thread_) {
            log_worker_thread_ = std::make_unique<std::thread>(&ngp::NGNanoLogger::thread_func, this);
        }

        ret_code = writer_->Initialize();

        is_initialize_ = true;
        return ret_code;
    }
    
    // 释放资源
    void Release()
    {
        if (!is_initialize_) {
            return;
        }

        is_exit_ = true;
        if (log_worker_semaphore_) {
            log_worker_semaphore_->release_all();
        }

        if (log_worker_thread_) {
            log_worker_thread_->join();
            log_worker_thread_.reset();
        }
        if (log_worker_semaphore_) {
            log_worker_semaphore_.reset();
        }

        if (writer_) {
            writer_->Release();
        }

        is_initialize_ = false;
    }

    // 压入日志
    void Push(std::shared_ptr<LogLine>&& log)
    {
        if (!is_initialize_) {
            return;
        }

        log_content_.enqueue(log_producer_, std::move(log));
        if (log_worker_semaphore_->get_waitor_count() > 0) {
            log_worker_semaphore_->release();
        }
    }
public:
    // 线程工作函数
    void thread_func()
    {
        while (true) {
            std::shared_ptr<LogLine> log_ptr = nullptr;
            while (!log_content_.try_dequeue(log_consumer_, log_ptr) && !is_exit_) {
                log_worker_semaphore_->waite_for_semaphore();
            }

            if (is_exit_) {
                break;
            }

            if (writer_ != nullptr) {
                writer_->Write(log_ptr);
            }
        }
    }
private:
    std::atomic_bool is_initialize_;
    std::atomic_bool is_exit_;

    typedef moodycamel::ConcurrentQueue<std::shared_ptr<LogLine>, log_content_size> LOG_CONTENT_QUEUE;
    LOG_CONTENT_QUEUE log_content_;
    LOG_CONTENT_QUEUE::producer_token_t log_producer_;
    LOG_CONTENT_QUEUE::consumer_token_t log_consumer_;

    std::unique_ptr<std::thread> log_worker_thread_;
    std::unique_ptr<ngp::utility::ng_semaphore> log_worker_semaphore_;

    std::shared_ptr<ng_base_writer> writer_;
};

class ng_logger
{
public:
    virtual ~ng_logger() = default;

public:
    static ng_logger* Instance()
    {
        static ng_logger logger;
        return &logger;
//        static std::once_flag onceflag;
//        std::call_once(onceflag, [&]() {if (!instance_ptr_) { instance_ptr_.reset(new ng_logger); }});
//        return instance_ptr_.get();
    }

public:
    inline void Initialize(const std::shared_ptr<ng_base_writer>& writer, ENU_LOG_LEVEL level)
    {
        if (!log_worker_) {
            log_worker_ = std::make_unique<NGNanoLogger>(writer);
            loger_ptr_.store(log_worker_.get(), std::memory_order_seq_cst);
            log_worker_->Initialize();
        }
        log_level_.store(static_cast<unsigned int>(level), std::memory_order_release);
    }

    inline void Release()
    {
        if (log_worker_) {
            log_worker_->Release();
        }
    }

    inline void SetLogLevel(ENU_LOG_LEVEL level)
    {
        log_level_.store(static_cast<unsigned int>(level), std::memory_order_release);
    }

    inline bool IsLogged(ENU_LOG_LEVEL level)
    {
        return static_cast<unsigned int>(level) <= log_level_.load(std::memory_order_relaxed);
    }

    template<typename T,
            class = typename std::enable_if_t<
                    std::is_base_of_v<ngp::LogLine, T>
                    || std::is_same_v<ngp::LogLine, typename std::decay_t<T>>>>
    bool operator==(T& logline)
    {
        auto ptr = loger_ptr_.load(std::memory_order_acquire);

        if (nullptr == ptr) return false;

        ptr->Push(std::make_shared<T>(std::move(logline)));

        return true;
    }

    template<typename T,
            class = typename std::enable_if_t<
                    std::is_base_of_v<ngp::LogLine, T>
                    || std::is_same_v<ngp::LogLine, typename std::decay_t<T>>>>
    bool operator==(T&& logline)
    {
        auto ptr = loger_ptr_.load(std::memory_order_acquire);

        if (nullptr == ptr) return false;

        ptr->Push(std::make_shared<T>(std::move(logline)));

        return true;
    }

protected:
    ng_logger() = default;
private:
    std::unique_ptr<NGNanoLogger> log_worker_;
    std::atomic_uint32_t log_level_;
    std::atomic<NGNanoLogger*> loger_ptr_;
//private:
//    static std::unique_ptr<ng_logger> instance_ptr_;
};
//#ifdef __linux__
//inline std::unique_ptr<ng_logger> ngp::ng_logger::instance_ptr_;
//#endif
} // end of namesapce utility
} // end of namespace eddid

#define NG_LOG(LEVEL) *ngp::ng_logger::Instance() == ngp::LogLine(LEVEL, __FILE__, __func__, __LINE__)
#define NG_LOG_STDERR ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_STDERR) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_STDERR)
#define NG_LOG_EMERG  ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_EMERG) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_EMERG)
#define NG_LOG_ALERT  ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_ALERT) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_ALERT)
#define NG_LOG_CRIT   ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_CRIT) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_CRIT)
#define NG_LOG_ERR    ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_ERR) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_ERR)
#define NG_LOG_WARN   ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_WARN) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_WARN)
#define NG_LOG_NOTICE ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_NOTICE) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_NOTICE)
#define NG_LOG_INFO   ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_INFO) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_INFO)
#define NG_LOG_DEBUG  ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_DEBUG) && NG_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_DEBUG)


#define NG_KEY_LOG(LEVEL, KEY) *ngp::ng_logger::Instance() == ngp::KeyLogLine<decltype(KEY)>(KEY, LEVEL, __FILE__, __func__, __LINE__)
#define NG_KEY_LOG_STDERR(KEY) ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_STDERR) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_STDERR, KEY)
#define NG_KEY_LOG_EMERG(KEY)  ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_EMERG) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_EMERG, KEY)
#define NG_KEY_LOG_ALERT(KEY)  ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_ALERT) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_ALERT, KEY)
#define NG_KEY_LOG_CRIT(KEY)   ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_CRIT) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_CRIT, KEY)
#define NG_KEY_LOG_ERR(KEY)    ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_ERR) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_ERR, KEY)
#define NG_KEY_LOG_WARN(KEY)   ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_WARN) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_WARN, KEY)
#define NG_KEY_LOG_NOTICE(KEY) ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_NOTICE) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_NOTICE, KEY)
#define NG_KEY_LOG_INFO(KEY)   ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_INFO) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_INFO, KEY)
#define NG_KEY_LOG_DEBUG(KEY)  ngp::ng_logger::Instance()->IsLogged(ngp::ENU_LOG_LEVEL::ELL_LOG_DEBUG) && NG_KEY_LOG(ngp::ENU_LOG_LEVEL::ELL_LOG_DEBUG, KEY)






