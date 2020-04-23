#pragma once
#include <string>
#include <cstring>

class BaseTask
{
public:
    virtual ~BaseTask();
    virtual int Run() = 0;
    int EndRun(char* &dataStart, int& dataSize);
    int Initialize(char* fileMap, int fileSize, const std::string& condition, const char& delimiter = ',');
public:
    char* m_fileMap;
    int m_fileSize;
    char* m_newFileMap;
    int m_newFileSize;
    std::string m_condition;
    char m_delimiter;
};