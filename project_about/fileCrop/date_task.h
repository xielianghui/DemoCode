#pragma once


#include "base_task.h"

class DateTask :public BaseTask
{
public:
    DateTask() = default;
    ~DateTask() = default;
    int Run() override;
};