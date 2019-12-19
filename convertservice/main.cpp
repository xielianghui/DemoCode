#include <iostream>
#include "convert_service.h"

int main()
{
    std::string lwsAddr = "119.23.15.70";
    int lwsPort = 8500;
    std::string levAddr = "127.0.0.1";
    int levPort = 8032;
    ConvertService srv;
    srv.Init();
    srv.InitLwsClient(lwsAddr, lwsPort);
    srv.InitLevService(levAddr, levPort);
    char ch_input;
    while (true) {
        std::cin >> ch_input;
        if (ch_input == 'q' || ch_input == 'Q') {
            break;
        }
    }
    return 0;
}