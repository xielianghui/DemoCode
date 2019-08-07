#pragma once
#include<iostream>
#include<string>
#include<mutex>
#include<vector>

#include<sybfront.h>
#include<sybdb.h>


struct DBConfig
{
    std::string host;
    std::string port;
    std::string userName;
    std::string password;
    std::string dbName;
};

struct ResCol // save col info
{
    std::string name; // col name
    int type;
    int size;
    int status;
    char* buf;
    ResCol():buf(nullptr){}
};

class SqlServerHdl
{
    public:
        SqlServerHdl(const SqlServerHdl&) = delete;
        SqlServerHdl& operator = (const SqlServerHdl&) = delete;
        static SqlServerHdl* Instance();
        int Init(DBConfig& cfg);
        int QueryCmd(std::string& cmd, std::vector<std::vector<std::string>>& res);
    private:
        SqlServerHdl();
        ~SqlServerHdl();
    private:
        static SqlServerHdl* m_instance;
        DBPROCESS* m_dbproc;
};
