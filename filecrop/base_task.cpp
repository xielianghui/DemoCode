#include "base_task.h"

int BaseTask::Initialize(char* fileMap, int fileSize, const std::string& condition, const char& delimiter)
{
    m_fileMap = fileMap;
    m_fileSize = fileSize;
    m_condition = condition;
    m_delimiter = delimiter;

    return 0;
}

int BaseTask::EndRun(char* &dataStart, int& dataSize)
{
    dataStart = m_newFileMap;
    dataSize = m_newFileSize;
    return 0;
}

BaseTask::~BaseTask(){}