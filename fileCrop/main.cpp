#include <iostream>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <vector>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>

#include <uv.h>

#include "base_task.h"
#include "col_task.h"
#include "code_task.h"
#include "date_task.h"
#include "task_dispatcher.h"

#define TASKCOUNTS 4

static void GetDateCondition(const std::string& dateList, std::vector<std::pair<int, int>>& dateConditionVec)
{
    auto InsetIntoVec = [](const std::string& part, std::vector<std::pair<int, int>>& dateConditionVec){
        int startDate = 0;
        int endDate = 0;
        int pos = part.find('-');
        if(pos == (int)std::string::npos){
            startDate = std::atoi(part.c_str());
            endDate = std::atoi(part.c_str());
        }else{
            startDate = std::atoi(part.substr(0, pos).c_str());
            endDate = std::atoi(part.substr(pos + 1, part.size() - pos).c_str());
        }
        dateConditionVec.emplace_back(std::make_pair(startDate, endDate));
    };
    int pos = 0;
    int start = 0;
    while ((pos = dateList.find(',', start)) != (int)dateList.npos) {
        std::string part = dateList.substr(start, pos - start);
        if (!part.empty()) {
            InsetIntoVec(part, dateConditionVec);
        }
        start = pos + 1;
    }
    std::string part = dateList.substr(start, dateList.size() - start);
    InsetIntoVec(part, dateConditionVec);
}

static void SeparateFile(char* fileMap, int fileSize, std::vector<std::pair<char*, int>>& res)
{
    int stepSize = (fileSize / TASKCOUNTS);
    char* pStart = fileMap;
    int finishedSize = 0;
    while ((finishedSize + stepSize) < fileSize)
    {
        char* pNext = std::strchr(pStart + stepSize, '\n');
        if (pNext != nullptr) {
            int oneStepSize = ( (int)(pNext - pStart) + 1 );
            res.emplace_back(std::make_pair(pStart, oneStepSize));
            pStart += oneStepSize;
            finishedSize += oneStepSize;
        }
        else {
            std::cerr << "File error, maybe it have no '\n'" << std::endl;
        }
    }
    if (finishedSize < fileSize) {
        res.emplace_back(std::make_pair(pStart, ( fileSize - finishedSize)));
    }
}

static void GetAllFileNamesInPath(const char *filePath, std::vector<std::string>& vtFileNames, bool procSubdirectory = false) 
 {
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    if ((dir = opendir(filePath)) != NULL) {
        while ((ptr = readdir(dir)) != NULL) {
            if (strcasecmp(ptr->d_name, ".") == 0 || strcasecmp(ptr->d_name, "..") == 0)
                continue;
            switch (ptr->d_type) {
                case 4: {
                    if (procSubdirectory)
                    GetAllFileNamesInPath(std::string(filePath).append(ptr->d_name).c_str(), vtFileNames, procSubdirectory);
                    break;
                }
                case 8: {
                    vtFileNames.emplace_back(std::string(filePath).append(ptr->d_name));
                    break;
                }
                case 10:
                default:
                break;
            }
        }
        closedir(dir);
    }
}
/*
    usage:
    filecrop fileDir code 000001,600000.. date 201807,201808-201811 col 1-1,2-10 <outputDir or onezipdir>
*/
int main(int argc, char** argv)
{
    if (argc < 9) {
        // show for front end
        puts("[error]:Please use like this: IP:port/filecrop/code&000001,600000&date&201807,201808-201811&col&1-1,2-10&output.zip");
        return 0;
    }
    std::string codeWord(argv[2]);
    std::string dateWord(argv[4]);
    std::string colWord(argv[6]);
    if(codeWord != "code" || dateWord != "date" || colWord != "col"){
        puts("[error]:Please use like this: IP:port/filecrop/code&000001,600000&date&201807,201808-201811&col&1-1,2-10&output.zip");
        return 0;
    }
    std::string fileDir(argv[1]);
    std::string codeList(argv[3]);
    std::string dateList(argv[5]);
    std::string colList(argv[7]);
    std::string output(argv[8]);
    // timing
    struct timeval start_tv;
    struct timeval end_tv;

    bool isAllCode = false;
    bool isAllDate = false;
    if(codeList == "*"){
        isAllCode = true;
    }
    // get date condition
    std::vector<std::pair<int, int>> dateConditionVec;
    if(dateList == "*"){
        isAllDate = true;
    }else{
        GetDateCondition(dateList, dateConditionVec);
    }

    // get fileDir all file name
    std::vector<std::string> allFileNameVec;
    std::vector<std::string> fileNameVec;
    GetAllFileNamesInPath(fileDir.c_str(), allFileNameVec, false);
    if(allFileNameVec.empty()){
        printf("[error]:Empty file dir %s", fileDir.c_str());
        return 0;
    }
    for(auto& it : allFileNameVec){
        if(it.find("_") == std::string::npos){
            continue;
        }
        if(isAllCode){
            // do nothing
        }else{
            std::string code = it.substr(it.find_last_of("_") - 6, 6);
            if(codeList.find(code) == std::string::npos){
                continue;
            }
        }
        if(isAllDate){
            // do nothing
        }else{
            int date = std::atoi(it.substr(it.find_last_of("_") + 1, 6).c_str());
            bool isMeet = false;
            for(auto& oneDateCondition : dateConditionVec){
                if(date < oneDateCondition.first || date > oneDateCondition.second){
                    continue;
                }else{
                    isMeet = true;
                    break;
                }
            }
            if(!isMeet){
                continue;
            }
        }
        fileNameVec.emplace_back(it);
    }
    if(fileNameVec.empty()){
        printf("[error]:No file meet the code date conditions in dir %s", fileDir.c_str());
        return 0;
    }
    
    // get output dir
    bool isOneZip = false;
    std::string oneZipName("");
    std::string outputDir = output;
    if (output.find(".zip") != std::string::npos) {
        isOneZip = true;
        if (output.find('/') != std::string::npos) {
            outputDir = output.substr(0, output.find_last_of('/') + 1);
            oneZipName = output.substr(output.find_last_of('/') + 1);
        }
        else {// Under this directory
            outputDir = "./";
            oneZipName = output;
        }
    }
    // create new file
    std::vector<std::string> newFileNameVec;
    for (auto& it : fileNameVec) {
        gettimeofday(&start_tv, NULL);
        // mmap file
        int fileFd = open(it.c_str(), O_RDONLY);
        if (fileFd < 0) {
            printf("[%s]File open failed, error code %d, error msg:%s\n", it.c_str(), errno, std::strerror(errno));
            return 0;
        }
        struct stat statBuff;
        if (lstat(it.c_str(), &statBuff) < 0) {
            printf("[%s]File lstat failed, error code %d, error msg:%s\n", it.c_str(), errno, std::strerror(errno));
            return 0;
        }
        auto fileAddr = mmap(NULL, statBuff.st_size, PROT_READ, MAP_PRIVATE, fileFd, 0);
        if (fileAddr == (void*)(-1)) {
            printf("[%s]File lstat failed, error code %d, error msg:%s\n", it.c_str(), errno, std::strerror(errno));
            return 0;
        }
        char* fileMap = (char*)fileAddr;
        std::vector<std::pair<char*, int>> res;
        BaseTask * pTask = nullptr;
        // do some work
        uv_loop_t uvloop;
        int rc = uv_loop_init(&uvloop);
        SeparateFile(fileMap, statBuff.st_size, res);
        
        pTask = new ColTask[res.size()];

        for (unsigned int i = 0; i < res.size(); ++i)
        {
            pTask[i].Initialize(res[i].first, res[i].second, colList);
            TaskDispatcher::Dispatch(&pTask[i], &uvloop);
        }
        uv_run(&uvloop, UV_RUN_DEFAULT);
        rc = uv_loop_close(&uvloop);
        res = TaskDispatcher::GetResult();

        if (!res.empty()) {
            // write new file
            std::string newFilePath = (outputDir + it.substr(it.find_last_of('/') + 1));
            int newFileFd = open(newFilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
            if (newFileFd < 0) {
                printf("[%s]File open failed, error code %d, error msg:%s\n", newFilePath.c_str(), errno, std::strerror(errno));
                return 0;
            }
            for (auto& it : res) {
                write(newFileFd, it.first, it.second);
                free(it.first);
            }
            close(newFileFd);
            gettimeofday(&end_tv, NULL);
            //printf("[%s]Use %f s\n", newFilePath.c_str(), (end_tv.tv_sec - start_tv.tv_sec + (double)(end_tv.tv_usec - start_tv.tv_usec) / 1000000));
            TaskDispatcher::ClearResult();
            newFileNameVec.emplace_back(it.substr(it.find_last_of('/') + 1));
        }
        else {
            printf("[%s]Get empty result!\n", it.c_str());
        }
        munmap(fileMap, statBuff.st_size);
        close(fileFd);
        delete[] pTask;
    }
    if (isOneZip) {
        std::stringstream cmd;
        std::string rmCmdFile;
        // swith dir cmd
        cmd << "cd " << outputDir << "\n";
        // zip cmd
        cmd << "rm -r " << oneZipName << "\n";
        cmd << "zip " << oneZipName << " ";
        for (auto& it : newFileNameVec) {
            cmd << it << " ";
            rmCmdFile += (it + " ");
        }
        // rm cmd
        cmd << "\n" << "rm -f " << rmCmdFile;
        std::system(cmd.str().c_str());
        printf("success:%s",oneZipName.c_str());
    }
    else {
        std::stringstream cmd;
        std::string outputZipName;
        for (auto& it : newFileNameVec) {
            cmd.str(""); //clear 
            // swith dir cmd
            cmd << "cd " << outputDir << "\n";
            std::string zipName = (it.substr(0, it.size() - 3) + "zip");
            cmd << "rm -f " << zipName << "\n";
            cmd << "zip " <<  zipName << " " << it << "\n" <<
                "rm -f " << it;
            std::system(cmd.str().c_str());
            outputZipName += (zipName + "&");
        }
        printf("success:%s",outputZipName.c_str());
    }
    return 0;
}