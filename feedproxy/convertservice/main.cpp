#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "convert_service.h"

int main()
{
    std::string lwsAddr = "120.78.155.211";//  延时:120.77.241.205  实时:120.78.155.211
    int lwsPort = 8500;
    std::string levAddr = "0.0.0.0";
    std::vector<int> levPortVec{8031, 8032};
    ConvertService srv;
    srv.Init();
    srv.InitLevService(levAddr, levPortVec);
    srv.InitUwsClient(lwsAddr, lwsPort);
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 60));
    }
    return 0;
}
