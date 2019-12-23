#include <iostream>
#include <cstring>
#include <vector>


#define MAX_NUMBER 10
static int countingArray[MAX_NUMBER];

int MaxBit(const std::vector<int>& nums)
{
    int bits = 1;
    int standard = 10;
    for(auto& it : nums)
    {
        if(it >= standard){
            ++bits;
            standard *= 10;
        }
    }
    return bits;
}

void RadixSort(std::vector<int>& nums)
{
    if(nums.size() < 2) return;
    int radix = 1;
    std::vector<int> tmpVec;
    tmpVec.resize(nums.size());
    int maxBit = MaxBit(nums);
    for(int i = 0; i < maxBit; ++i)
    {
        memset(countingArray, 0, MAX_NUMBER*sizeof(int));
        for(auto& it : nums)
        {
            countingArray[(it / radix) % 10] += 1;
        }
        for(int j = 1; j < MAX_NUMBER; ++j)
        {
            countingArray[j] += countingArray[j - 1];
        }
        for(int j = nums.size() - 1; j >= 0; --j) //注意需要从后往前遍历，因为上一位大的数已经排在后面了
        {
            tmpVec[countingArray[(nums[j] / radix) % 10] - 1] = nums[j];
            countingArray[(nums[j] / radix) % 10] -= 1;
        }
        nums.swap(tmpVec);
        radix *= 10;
    }
}

int main()
{
    std::vector<int> nums{32,1,3,21,4,123,321,3,21,432,3,21,321,3,325,43,6,546,4,23,4,2134,32,45,3245,43,543,231};
    RadixSort(nums);
    for(auto& it : nums){
        std::cout<< it << " ";
    }
    getchar();
    return 0;
}
