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

enum class CMDTYPE
{
    UNKNOW = 0,
    SELECT = 1,
    INSERT = 2,
    UPDATE = 3
};

class SqlServerHdl
{
    public:
        SqlServerHdl(const SqlServerHdl&) = delete;
        SqlServerHdl& operator = (const SqlServerHdl&) = delete;
        static SqlServerHdl* Instance();
        int Init(DBConfig& cfg);
        int QueryCmd(const std::string& cmd, std::vector<std::vector<std::string>>& res);
        int InsertCmd(const std::string& cmd);
        int UpdateCmd(const std::string& cmd);
    private:
        SqlServerHdl();
        ~SqlServerHdl();
        int Cmd(CMDTYPE type, const std::string& cmd, std::vector<std::vector<std::string>>& res);
    private:
        static SqlServerHdl* m_instance;
        DBPROCESS* m_dbproc;
};
