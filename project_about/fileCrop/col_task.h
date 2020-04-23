#pragma once


#include "base_task.h"

class ColTask:public BaseTask
{
public:
    ColTask() = default;
    ~ColTask() = default;
    int Run() override;
};