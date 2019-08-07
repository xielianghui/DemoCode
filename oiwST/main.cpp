#include<unistd.h>
#include<iostream>

#include<uv.h>

#include "sql_server_wrapper.h"

static void TimerCallback(uv_timer_t* /*hdl*/)
{
    std::cout<<"haha"<<std::endl;
}

static void Usage()
{
    puts("Usage: owi [-H host] [-p port] [-U user_name] [-P password] [-D dbname]");
}

int main(int argc, char** argv)
{
    // get config
    int configSetCounts = 0;
    DBConfig cfg;
    int ch;
    while((ch = getopt(argc, argv, "H:p:U:P:D:")) != -1)
    {
        switch(ch)
        {
            case 'H': {cfg.host = std::string(optarg); ++configSetCounts; break;}
            case 'p': {cfg.port = std::string(optarg); ++configSetCounts; break;}
            case 'U': {cfg.userName = std::string(optarg); ++configSetCounts; break;}
            case 'P': {cfg.password = std::string(optarg); ++configSetCounts; break;}
            case 'D': {cfg.dbName = std::string(optarg); ++configSetCounts; break;}
        }
    }
    if(configSetCounts < 5){
        Usage();
        return -1;
    }
    // init sql server wrapper
    if(SqlServerHdl::Instance()->Init(cfg) == -1){
        std::cout<<"sql server connect failed!"<<std::endl;
        return -1;
    }
    std::vector<std::vector<std::string>> res;
    std::string cmd = "select * from dbo.ashare_ordwth";
    if(SqlServerHdl::Instance()->QueryCmd(cmd, res) == -1){
        std::cout<<"sql server connect failed!"<<std::endl;
        return -1;
    }
    for(auto& it : res)
    {
        for(auto& itit : it)
        {
            std::cout<<itit<<"   ";
        }
        std::cout<<std::endl;
    }
    
    uv_loop_t* mainLoop = uv_default_loop();
    uv_timer_t timerHdl;
    uv_timer_init(mainLoop, &timerHdl);    
    uv_timer_start(&timerHdl, TimerCallback, 200, 200);// 200ms per cycle
    uv_run(mainLoop, UV_RUN_DEFAULT);
    return 0;
}
