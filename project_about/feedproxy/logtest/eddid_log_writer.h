#pragma once
/**********************************************************************************************
*** File:ng_log_writer.h
*** Author: xiay
*** DateTime: 2018/10/28 09:15
*** Descreption: 对日志的writer进行封装
***********************************************************************************************/
#include <sstream>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "ng_nano_logger.h"

namespace ngp {
    class NgLogFileWriter : public ng_base_writer
    {
    public:
        NgLogFileWriter(const std::string& file_path, const std::string& file_name, uint32_t max_size_mb)
                : file_number_(0)
                , bytes_written_(0)
                , log_file_max_size_(max_size_mb * 1024 * 1024)
                , file_name_(file_path + file_name)
        {

        }

        ~NgLogFileWriter() final = default;

    public:
        int Initialize() final
        {
            RollFile();

            return 0;
        }

        void Release() final
        {
            if (file_os_) {
                file_os_->flush();
                file_os_->close();
            }
        }

        void Write(std::shared_ptr<LogLine> & log_ptr) final
        {
            if (!log_ptr) return;
            auto pos = file_os_->tellp();
            log_ptr->stringify(*file_os_);
            bytes_written_ += file_os_->tellp() - pos;
            if (bytes_written_ > log_file_max_size_) {
                RollFile();
            }
        }
    private:
        void RollFile()
        {
            if (file_os_) {
                file_os_->flush();
                file_os_->close();
            }

            bytes_written_ = 0;
            file_os_ = std::make_unique<std::ofstream>();
            std::string file_name = file_name_;
            time_t date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::stringstream ss;
            ss << std::put_time(std::localtime(&date), "%Y-%m-%d");
            file_name += "_";
            file_name += ss.str();
            file_name += "_";
            file_name += std::to_string(file_number_++);
            file_name += ".log";
            file_os_->open(file_name, std::ofstream::out | std::ofstream::trunc);
        }
    private:
        uint32_t file_number_;
        std::streamoff bytes_written_;
        uint32_t log_file_max_size_;
        std::string file_name_;
        std::unique_ptr<std::ofstream> file_os_;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////////
/////// class NgLogConsoleWriter
    class NgLogConsoleWriter : public  ng_base_writer
    {
    public:
        NgLogConsoleWriter() = default;
        ~NgLogConsoleWriter() final = default;

    public:
        int Initialize() final
        {
            return 0;
        }

        void Release() final
        {
        }

        void Write(std::shared_ptr<LogLine> & log_ptr) final
        {
            if (!log_ptr) return;
            std::stringstream ss;
            log_ptr->stringify(ss);
            std::string str = ss.str();
            std::cout << ss.str();
        }
    };

//////////////////////////////////////////////////////////////////////////////////////////////////////
///// class NgLogNetWriter
    class NgLogNetWriter : public  ng_base_writer
    {
    public:
        NgLogNetWriter() = default;
        ~NgLogNetWriter() final = default;

    public:
        int Initialize() final
        {
            return 0;
        }

        void Release() final
        {
        }

        void Write(std::shared_ptr<LogLine> & log_ptr) final
        {
        }
    protected:
    private:
    };


/// multi file
    class NgLogMultiFileWriter : public ng_base_writer
    {
    public:
        NgLogMultiFileWriter(std::string const & log_directory, std::string const & log_file_name, uint32_t log_file_roll_size_mb)
                : m_log_level({
                                      ENU_LOG_LEVEL::ELL_LOG_STDERR,
                                      ENU_LOG_LEVEL::ELL_LOG_EMERG,
                                      ENU_LOG_LEVEL::ELL_LOG_ALERT,
                                      ENU_LOG_LEVEL::ELL_LOG_CRIT,
                                      ENU_LOG_LEVEL::ELL_LOG_ERR,
                                      ENU_LOG_LEVEL::ELL_LOG_WARN,
                                      ENU_LOG_LEVEL::ELL_LOG_NOTICE,
                                      ENU_LOG_LEVEL::ELL_LOG_INFO,
                                      ENU_LOG_LEVEL::ELL_LOG_DEBUG})
        {
            m_file_writer.resize(m_log_level.size());
            for (size_t index = 0; index < m_log_level.size(); index++)
            {
                m_file_writer[index].reset(
                        new NgLogFileWriter(
                                log_directory,
                                log_file_name + "." + to_string(m_log_level[index]),
                                log_file_roll_size_mb));
            }
        }
        ~NgLogMultiFileWriter() final
        {
            for (auto& item : m_file_writer)
            {
                item.reset();
            }
        }

    protected:
        std::shared_ptr<NgLogFileWriter> GetFileWriter(ENU_LOG_LEVEL level)
        {
            for (size_t index = 0; index < m_file_writer.size(); index++)
            {
                if (level == m_log_level[index])
                {
                    return  m_file_writer[index];
                }
            }
            return nullptr;
        }

    public:
        inline int Initialize() final
        {
            int ret = 0;
            for (auto& item : m_file_writer)
            {
                if (nullptr == item) continue;
                ret = item->Initialize();
                if (0 != ret) break;
            }
            return ret;
        }

        inline void Release() final
        {
            for (auto& item : m_file_writer)
            {
                if (nullptr == item) continue;
                item->Release();
            }
        }

        void Write(std::shared_ptr<LogLine> & log_ptr) final
        {
            if (!log_ptr) return;

            std::shared_ptr<NgLogFileWriter> file_writer = GetFileWriter(log_ptr->get_level());
            if (nullptr == file_writer) return;
            file_writer->Write(log_ptr);
        }

    private:
        std::vector<ENU_LOG_LEVEL> m_log_level;
        using WriterType = std::vector<std::shared_ptr<NgLogFileWriter>>;
        WriterType m_file_writer;
    };

    template<typename Key>
    class NgLogMapWriter : public ng_base_writer
    {
    public:
        NgLogMapWriter() = default;
        ~NgLogMapWriter() final = default;
    public:
        bool Register(const Key& key, std::shared_ptr<ng_base_writer> writer)
        {
            if (m_writer.end() != m_writer.find(key))
            {
                return false;
            }
            m_writer[key] = writer;
            return true;
        }

        void UnRegister(const Key& key)
        {
            m_writer.erase(key);
        }

    public:
        // return 0 mean success, -1 is failed
        int Initialize() final
        {
            for (auto& item : m_writer)
            {
                if (!item.second) continue;

                item.second->Initialize();
            }
            return 0;
        }

        void Release() final
        {
            for (auto& item : m_writer)
            {
                if (!item.second) continue;

                item.second->Release();
            }
        }

    public:
        void Write(std::shared_ptr<LogLine> & log_ptr) final
        {
            auto line_ptr = std::dynamic_pointer_cast<KeyLogLine<Key>>(log_ptr);
            if (!line_ptr) return;

            auto it = m_writer.find(line_ptr->GetKey());

            if (m_writer.end() == it) return;

            if (!it->second) return;

            it->second->Write(log_ptr);
        }

    private:
        using WirterMap = std::unordered_map<Key, std::shared_ptr<ng_base_writer>, _::HashType<Key>>;

        WirterMap m_writer;
    };

} // end of namespace ngp

