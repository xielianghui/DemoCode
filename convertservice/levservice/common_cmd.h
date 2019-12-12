#pragma once
#include <ng_platform.h>

// 通用命令字

namespace ngp {

// 定义 消息ID 格式 为 0xabcc，其中a为系统编号, b 为模块编号, cc为消息编号
#define CMD_DEF(system_id, module_id, msg_id) ((((uint8_t)system_id & 0xf) << 12) | (((uint8_t)module_id & 0xf) << 8) | ((uint16_t)msg_id & 0xff))

#pragma pack(push)
#pragma pack(1)

    //
    enum struct ENU_SYSTEM_ID : uint8_t
    {
        ESYSID_COMMON           = 0x0,          // 公共部分
        ESYSID_COLLECTION       = 0x1,          // 采集服务器
        ESYSID_PUBLISHER        = 0x2,          // 发布服务器
        ESYSID_QUOTE_HISTORY    = 0x3,          // 历史行情服务器
        ESYSID_TRADING_OFFER    = 0x4,          // 交易报盘机
        ESYSID_TRADER_SERVICE   = 0x5,          // 交易服务器
        ESYSID_RISK_SERVICE     = 0x6,          // 风控服服务器
        ESYSID_AUTH_SERVICE     = 0x7,          // 认证服务器
        ESYSID_COUNTER_MGR_SRV  = 0x8,          // 柜台服务器
        ESYSID_FOLLOW_STRATEGY  = 0x9,          // 跟单服务器
        ESYSID_DATA_CENTER_SRV  = 0xA,          // 数据中心服务器
    };

    enum struct ENU_COMM_MODULE : uint8_t
    {
        COMMON                  = 0x1,
        HEART                   = 0x2,
        LOGIN                   = 0x3,
        OPERTOR                 = 0x4,         // 操作
        QUERY                   = 0x5,         // 查询
        PUSH                    = 0x6,         // 服务器主推数据
    };


    #define COMMON_SYSTEM_ID ngp::ENU_SYSTEM_ID::ESYSID_COMMON

    enum struct ENU_COMMON_CMD : uint16_t
    {
//        ECMD_UNKNOWN            = 0x0000,       // 无效命令字
//        ECMD_HEART              = 0x0001,       // 心跳
//        ECMD_HEART_RSP          = 0x0002,       // 心跳应答
//        ECMD_USRLOGIN           = 0x0003,       // 用户登陆
//        ECMD_USRLOGIN_RSP       = 0x0004,       // 用户登陆应答
//        ECMD_USRLOGOUT          = 0x0005,       // 用户登出
//        ECMD_USRLOGOUT_RSP      = 0x0006,       // 用户登出应答
//
//        ECMD_COMMON_END         = 0x000F,       // 结尾
//        // 注：从0x0000到0x000F为通用命令字,其他各业务皆从 0x0010开始


        ECMD_UNKNOWN                = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::COMMON, 0x01),  // 无效命令字

        ECMD_HEART                  = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::HEART, 0x01),       // 心跳
        ECMD_HEART_RSP              = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::HEART, 0x02),       // 心跳应答

        ECMD_USRLOGIN               = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::LOGIN, 0x01),       // 用户登陆
        ECMD_USRLOGIN_RSP           = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::LOGIN, 0x02),       // 用户登陆应答
        ECMD_USRLOGOUT              = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::LOGIN, 0x03),       // 用户登出
        ECMD_USRLOGOUT_RSP          = CMD_DEF(COMMON_SYSTEM_ID, ENU_COMM_MODULE::LOGIN, 0x04),       // 用户登出应答

    };
    
    // eg:
//     enum struct ENU_QUOTE_CMD : uint16_t
//     {
//       ....  
//     };
    
#pragma pack(pop)
} // end of namespace ngp
