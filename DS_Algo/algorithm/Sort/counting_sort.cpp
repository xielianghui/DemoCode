#include <iostream>
#include <vector>

// suppose number in nums less than 1000
#define MAX_NUMBER 1000
static int countingArray[MAX_NUMBER];

void CountingSort(std::vector<int>& nums)
{
    if(nums.size() < 2) return;
    for(auto& it : nums)
    {
        countingArray[it] += 1;
    }
    for(int i = 1; i < MAX_NUMBER; ++i)
    {
        countingArray[i] +=  countingArray[i - 1];
    }
    std::vector<int> tmpVec;
    tmpVec.resize(nums.size());
    for(auto& it : nums)
    {
        tmpVec[countingArray[it]] = it;
        countingArray[it] -= 1;
    }
    nums.swap(tmpVec);
}

int main()
{
    std::vector<int> nums{6,32,21,3,21,23,4,32,54,35,6,4,323,21,4,36,54,76,5,87,32,13,21,3,12};
    CountingSort(nums);
    for(auto it : nums){
        std::cout<< it << " ";
    }
    getchar();
    return 0;
}
