#include <vector>
#include "col_task.h"


int ColTask::Run()
{
    // get col select
    std::vector<std::pair<unsigned int, unsigned int>> conditionVec;
    int pos = 0;
    int start = 0;
    while ((pos = m_condition.find(',', start)) != (int)m_condition.npos) {
        std::string part = m_condition.substr(start, pos - start);
        if (part.size() > 0) {
            int pos = part.find('-');
            int startCol = std::atoi(part.substr(0, pos).c_str());
            int endCol = std::atoi(part.substr(pos + 1, part.size() - pos).c_str());
            conditionVec.emplace_back(std::make_pair(startCol, endCol));
        }
        start = pos + 1;
    }
    std::string part = m_condition.substr(start, m_condition.size() - start);
    pos = part.find('-');
    int startCol = std::atoi(part.substr(0, pos).c_str());
    int endCol = std::atoi(part.substr(pos + 1, part.size() - pos).c_str());
    conditionVec.emplace_back(std::make_pair(startCol, endCol));

    // malloc new buf
    m_newFileMap = (char*)malloc(m_fileSize * (sizeof(char)));
    m_newFileSize = 0;
    pos = 0;
    char* lineStart = m_fileMap;
    std::vector<std::pair<char*, unsigned int>> res;
    // do search 
    while (pos < m_fileSize && lineStart != nullptr)
    {
        res.clear();
        // get line size
        char* lineEnd = std::strchr(lineStart, '\n');
        if (lineEnd == nullptr) {
            break; 
        }
        int lineSize = (int)(lineEnd - lineStart) + 1;
        int searchedSize = 0;
        char* searchPos = lineStart;
        char* preSearchPos = lineStart;
        while (1)
        {
            searchPos = std::strchr(searchPos, m_delimiter);
            if (searchPos == nullptr) {
                res.emplace_back(std::make_pair(preSearchPos, lineSize));
                break;
            }
            searchedSize = (int)(searchPos - lineStart) + 1;
            if (searchedSize >= lineSize) {
                res.emplace_back(std::make_pair(preSearchPos, lineSize));
                break;
            }
            res.emplace_back(std::make_pair(preSearchPos, searchedSize));
            ++searchPos;
            preSearchPos = searchPos;
        }
        unsigned int resSize = res.size();
        for (auto& it : conditionVec) {
            if (it.first < resSize && it.second < resSize && it.first >= 1 && it.first <= it.second) {
                if (it.first == 1) {
                    int size = res[it.second - 1].second;
                    memcpy(m_newFileMap + m_newFileSize, res[0].first, size);
                    m_newFileSize += size;
                    continue;
                }
                int size = res[it.second - 1].second - res[it.first - 2].second;
                memcpy(m_newFileMap + m_newFileSize, res[it.first - 1].first, size);
                m_newFileSize += size;
            }
            else {
                memcpy(m_newFileMap + m_newFileSize, lineStart, lineSize);
                m_newFileSize += lineSize;
                break;
            }
        }
        if (m_newFileMap[m_newFileSize - 1] != '\n') {
            m_newFileMap[m_newFileSize - 1] = '\n';
        }
        pos += lineSize;
        lineStart = (lineEnd + 1);
    }
    return 0;
}