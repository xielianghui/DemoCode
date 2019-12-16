#pragma once

#include "json/json.h"

#include "typedef.pb.h"
#include "msg_carrier.pb.h"
#include "basic_info.pb.h"

class ConvertUtils
{
public:
    static int ProtoReq2JsonReq(int64_t reqid, const std::string& reqProtoStr, Json::Value& reqJson)
    {
        reqJson.clear();
        ed::MsgCarrier msgCarrier;
        if(!msgCarrier.ParseFromString(reqProtoStr) || msgCarrier.msg_type() != ed::TypeDef_MsgType::TypeDef_MsgType_REQ){
            return -1;
        }
        // write same fields
        reqJson["reqid"] = reqid;
        reqJson["reqtype"] = (int)msgCarrier.req_type();
        reqJson["session"] = "";
        if(msgCarrier.req_type() == ed::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT){
            ed::Heartbeat heartbeatReq;
            if(!heartbeatReq.ParseFromString(msgCarrier.data())){
                return -1;
            }
            reqJson["data"]["connectionid"] = heartbeatReq.connection_id();
        }
        else{
            return -1;
        }
        return 0;
    }

    static int JsonRes2ProtoRes(int64_t& reqid, const Json::Value& resJson, std::string& resProto)
    {
        reqid = resJson.isMember("reqid") && resJson["reqid"].isInt() ? resJson["reqid"].asInt() : -1;
        if(reqid == -1){
            printf("Get req id failed, msg: %s\n", resJson.toStyledString().c_str());
            return -1;
        }
        // convert
        ed::MsgCarrier msgCarrier;
        msgCarrier.set_req_id(reqid);
        msgCarrier.set_msg_type(reqid == 0 ? ed::TypeDef_MsgType::TypeDef_MsgType_PUSH : ed::TypeDef_MsgType::TypeDef_MsgType_RESP);
        int reqType = resJson["reqtype"].asInt();
        if(reqType == 1 || reqType == 0){// heartbeat
            ed::HeartbeatResp heartbeatResp;
            heartbeatResp.set_error_code(resJson["status"].asInt());
            heartbeatResp.set_error_msg("");
            heartbeatResp.set_server_time(resJson["servertime"].asString());
            msgCarrier.set_req_type(ed::TypeDef_ReqType::TypeDef_ReqType_HEARTBEAT);
            msgCarrier.set_data(heartbeatResp.SerializeAsString());
        }
        else{
            return -1;
        }
        resProto = msgCarrier.SerializeAsString();
        return 0;
    }
};