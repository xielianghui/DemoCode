#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <array>

#include <uv.h>

#include "utils.h"
#include "table_struct.h"
#include "sql_server_wrapper.h"

#define ORDWTH "dbo.ashare_ordwth"
#define ORDWTH2 "dbo.ashare_ordwth2"
#define CJHB "dbo.ashare_cjhb"

const static std::string currentDate{ timeutils::CurDateStr() };
static int dealNumberSatrt = 1;
// sql cmd
const static std::string ordwthSelectCmd{ "select * from " ORDWTH };

void Ordwth2DefaultData(const std::vector<std::string>& order, std::array<std::string, 19>& confirm)
{
    confirm[ordwth2::rec_num] = order[ordwth::rec_num];
    confirm[ordwth2::date] = order[ordwth::date];
    confirm[ordwth2::reff] = order[ordwth::reff];
    confirm[ordwth2::acc] = order[ordwth::acc];
    confirm[ordwth2::stock] = order[ordwth::stock];
    confirm[ordwth2::bs] = order[ordwth::bs];
    confirm[ordwth2::price] = order[ordwth::price];
    confirm[ordwth2::qty] = order[ordwth::qty];
    confirm[ordwth2::owflag] = order[ordwth::owflag];
    confirm[ordwth2::firmid] = order[ordwth::firmid];
    confirm[ordwth2::branchid] = order[ordwth::branchid];
    confirm[ordwth2::checkord] = order[ordwth::checkord];
}

void CjhbDefaultData(const std::array<std::string, 19>& confirm, std::array<std::string, 15>& deal)
{
    deal[cjhb::gddm] = confirm[ordwth2::acc];
    deal[cjhb::bcrq] = confirm[ordwth2::date];
    deal[cjhb::gsdm] = confirm[ordwth2::firmid];
    deal[cjhb::zqdm] = confirm[ordwth2::stock];
    deal[cjhb::cjjg] = confirm[ordwth2::price];
    deal[cjhb::bs] = confirm[ordwth2::bs];
}

static void TimerCallback(uv_timer_t* /*hdl*/)
{
    // select ordwth
    std::vector<std::vector<std::string>> res;
    if (SqlServerHdl::Instance()->QueryCmd(ordwthSelectCmd, res) == -1) {
        std::cout << "QueryCmd() failed, cmd: "<< ordwthSelectCmd << std::endl;
        return;
    }
    for (auto& ordwthRcd : res)
    {
        if (ordwthRcd.size() < 15) {
            std::cout << "ordwth's col size less than 15, size: " << ordwthRcd.size() << std::endl;
            return;
        }
        // 1. update ordwth status
        /* 10 status 发送状态
         * 'R'或'r'表示该记录还没有发送,'P'表示已发往上交所后台.该字段是本表中唯一会被上交所报盘接口程序修改的字段.
         * 柜台系统可以通过该字段来判断是否已经向上交所发送该申报.
         */
        if (ordwthRcd[ordwth::status] == "P") {
           continue; // this record have process
        }
        std::string ordwthUpdateCmd{ "update " ORDWTH " set status = 'P' where rec_num = " + ordwthRcd[ordwth::rec_num] };
        if (SqlServerHdl::Instance()->UpdateCmd(ordwthUpdateCmd) == -1) {
            std::cout << "UpdateCmd() failed, cmd: " << ordwthUpdateCmd << std::endl;
            return;
        }
        // 2. insert ordwth2 records
        std::array<std::string, 19> ordwth2Rcd{ "" };
        Ordwth2DefaultData(ordwthRcd, ordwth2Rcd);
        // 委托数量为1000或者申报日期不是当前交易日直接废单
        /* 10 status 接收状态
        /* 'F'表示交易所后台判断该订单为废单。
        /* 'E'表示交易所前台判断该订单为废单;此时remark（字段12：错误信息）给出错误代码。
        /* '?'表示通信故障。
        /* 'O'表示上交所成功接收该笔申报。
        /* 'W'表示上交所成功接受该笔撤单;
        /* 当订单类型为最优五档即时成交剩余自动撤销的市价订单时，且订单有效时，该字段取值为'W'。
        */
        std::vector<int> dealVolume;
        if (ordwthRcd[ordwth::owflag] == "WTH") {// 撤单委托
            ordwth2Rcd[ordwth2::status] = "W";
            ordwth2Rcd[ordwth2::qty2] = ordwthRcd[ordwth::qty];
            ordwth2Rcd[ordwth2::ordrec] = ordwthRcd[ordwth::ordrec];
        }
        else {
            int orderVolume = std::atoi(ordwthRcd[ordwth::qty].c_str());
            if (orderVolume == 1000 || orderVolume < 0) {
                ordwth2Rcd[ordwth2::status] = "F";
            }
            else if (orderVolume == 900) {
                ordwth2Rcd[ordwth2::status] = "O";
            }
            else if (orderVolume == 600) {
                ordwth2Rcd[ordwth2::status] = "O";
                dealVolume.emplace_back(200);
            }
            else if(orderVolume > 1000){
                ordwth2Rcd[ordwth2::status] = "O";
                dealVolume.emplace_back(400);
                dealVolume.emplace_back(600);
                dealVolume.emplace_back(orderVolume - 1000);
            }
            else {
                ordwth2Rcd[ordwth2::status] = "O";
                dealVolume.emplace_back(orderVolume);
            }
        }
        ordwth2Rcd[ordwth2::time] = timeutils::CurTimeStr();
        std::string ordwth2InsertCmd{ "insert into " ORDWTH2 " values ('" + ordwth2Rcd[ordwth2::rec_num] + "','" + ordwth2Rcd[ordwth2::date] + "','" +
            ordwth2Rcd[ordwth2::time] + "','" + ordwth2Rcd[ordwth2::reff] + "','" + ordwth2Rcd[ordwth2::acc] + "','" + ordwth2Rcd[ordwth2::stock] + "','" + 
            ordwth2Rcd[ordwth2::bs] + "','" + ordwth2Rcd[ordwth2::price] + "','" + ordwth2Rcd[ordwth2::qty] + "','" + ordwth2Rcd[ordwth2::status] + "','" + 
            ordwth2Rcd[ordwth2::qty2] + "','" + ordwth2Rcd[ordwth2::remark] + "','" + ordwth2Rcd[ordwth2::status1] + "','" + ordwth2Rcd[ordwth2::teordernum] + "','" + 
            ordwth2Rcd[ordwth2::owflag] + "','" + ordwth2Rcd[ordwth2::ordrec] + "','" + ordwth2Rcd[ordwth2::firmid] + "','" + ordwth2Rcd[ordwth2::branchid] + "', 0)"};
        if (SqlServerHdl::Instance()->InsertCmd(ordwth2InsertCmd) == -1) {
            std::cout << "InsertCmd() failed, cmd: " << ordwth2InsertCmd << std::endl;
            return;
        }
        // 3. insert cjhb records
        if (!dealVolume.empty()) { // have deal
            std::array<std::string, 15> cjhbRcd{ "" };
            CjhbDefaultData(ordwth2Rcd, cjhbRcd);
            double cjjg = std::atof(ordwth2Rcd[ordwth2::price].c_str());
            for (auto& volumeIt : dealVolume)
            {
                cjhbRcd[cjhb::cjbh] = std::to_string(dealNumberSatrt++);
                cjhbRcd[cjhb::cjsl] = std::to_string(volumeIt);
                cjhbRcd[cjhb::cjjg] = std::to_string(cjjg);
                cjhbRcd[cjhb::cjje] = std::to_string(cjjg * volumeIt);
                cjhbRcd[cjhb::sqbh] = ordwth2Rcd[ordwth2::reff];
                std::string sbsj = ordwthRcd[ordwth::time];
                sbsj.erase(std::remove(sbsj.begin(), sbsj.end(), ':'), sbsj.end());
                cjhbRcd[cjhb::sbsj] = sbsj;
                cjhbRcd[cjhb::cjsj] = std::to_string(timeutils::CurTime());
                std::string cjhbInsertCmd{ "insert into " CJHB " values ('" + cjhbRcd[cjhb::gddm] + "','" + cjhbRcd[cjhb::gdxm] + "','" +
                    cjhbRcd[cjhb::bcrq] + "'," + cjhbRcd[cjhb::cjbh] + ",'" + cjhbRcd[cjhb::gsdm] + "','" + cjhbRcd[cjhb::cjsl] + "','" +
                    cjhbRcd[cjhb::bcye] + "','" + cjhbRcd[cjhb::zqdm] + "','" + cjhbRcd[cjhb::sbsj] + "','" + cjhbRcd[cjhb::cjsj] + "','" +
                    cjhbRcd[cjhb::cjjg] + "','" + cjhbRcd[cjhb::cjje] + "','" + cjhbRcd[cjhb::sqbh] + "','" + cjhbRcd[cjhb::bs] + "','" +
                    cjhbRcd[cjhb::mjbh] + "')" };
                if (SqlServerHdl::Instance()->InsertCmd(cjhbInsertCmd) == -1) {
                    std::cout << "InsertCmd() failed, cmd: " << cjhbInsertCmd << std::endl;
                    return;
                }
            }
        }


    }
}

static void Usage()
{
    puts("Usage: oiwst [-H host] [-p port] [-U user_name] [-P password] [-D dbname]");
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
    uv_loop_t* mainLoop = uv_default_loop();
    uv_timer_t timerHdl;
    uv_timer_init(mainLoop, &timerHdl);    
    uv_timer_start(&timerHdl, TimerCallback, 200, 200);// 200ms per cycle
    uv_run(mainLoop, UV_RUN_DEFAULT);
    return 0;
}

