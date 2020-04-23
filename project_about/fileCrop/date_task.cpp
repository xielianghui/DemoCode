#include "date_task.h"

int DateTask::Run()
{
    // get date select 
    int pos = m_condition.find('-');
    int startDate = std::atoi(m_condition.substr(0, pos).c_str());
    int endDate = std::atoi(m_condition.substr(pos + 1, m_condition.size() - pos).c_str());
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
            if ((int)(searchPos - preSearchPos) == 8) { // YYYYMMDD
                int date = std::atoi(preSearchPos);
                if (date >= startDate && date <= endDate) {
                    isFind = true;
                    break;
                }
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