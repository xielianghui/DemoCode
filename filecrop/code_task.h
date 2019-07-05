#pragma once


#include "base_task.h"

class CodeTask :public BaseTask
{
public:
    CodeTask() = default;
    ~CodeTask() = default;
    int Run() override;
};