#include "code_task.h"
#include <unordered_map>

int CodeTask::Run()
{
    // get search map
    std::unordered_map<std::string, int> conditionMap;
    int pos = 0;
    int start = 0;
    while ((pos = m_condition.find(',', start)) != (int)m_condition.npos) {
        std::string part = m_condition.substr(start, pos - start);
        if (part.size() > 0) {
            conditionMap.insert(std::make_pair(part, 1));
        }
        start = pos + 1;
    }
    conditionMap.insert(std::make_pair(m_condition.substr(start, m_condition.size() - start), 1));

    // malloc new buf
    m_newFileMap = (char*)malloc(m_fileSize * (sizeof(char)));
    m_newFileSize = 0;
    pos = 0;
    char* lineStart = m_fileMap;
    // do search 
    while (pos < m_fileSize && lineStart != nullptr)
    {
        // get line size
        char* lineEnd = std::strchr(lineStart, '\n');
        if (lineEnd == nullptr) {
            break; //TODO
        }
        int lineSize = (int)(lineEnd - lineStart) + 1;

        int searchedSize = 0;

        char* searchPos = lineStart;
        char* preSearchPos = lineStart;
        bool isFind = false;
        while (1)
        {
            searchPos = std::strchr(searchPos, m_delimiter);
            if (searchPos == nullptr) {
                break;
            }
            searchedSize = (int)(searchPos - lineStart) + 1;
            if (searchedSize >= lineSize) {
                break;
            }
            if (conditionMap.find(std::string(preSearchPos, (int)(searchPos - preSearchPos))) != conditionMap.end()) {
                isFind = true;
                break;
            }
            ++searchPos;
            preSearchPos = searchPos;
        }
        if (isFind) {
            memcpy(m_newFileMap + m_newFileSize, lineStart, lineSize);
            m_newFileSize += lineSize;
        }
        pos += lineSize;
        lineStart = (lineEnd + 1);
    }
    return 0;
}