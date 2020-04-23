#pragma once

#include <string>

class LwsCbInterface
{
public:
    virtual ~LwsCbInterface(){}
    virtual void OnConnect() = 0;
    virtual void OnRecv(std::string&& msg) = 0;
};