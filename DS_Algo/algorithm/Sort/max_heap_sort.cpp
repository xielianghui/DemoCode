#include <iostream>
#include <vector>

#define LEFT(i) 2 * i
#define RIGHT(i) 2 * i + 1
#define PARENT(i) i / 2

void Swap(int& x, int& y)
{
    int tmp = x;
    x = y;
    y = tmp;
}

void MaxHeapFix(std::vector<int>& nums, int i)
{
    if(i > nums.size() / 2) return;
    int iLargest = i;
    if(LEFT(i) < nums.size() && nums[LEFT(i)] > nums[iLargest]){
        iLargest = LEFT(i);
    }
    if(RIGHT(i) < nums.size() && nums[RIGHT(i)] > nums[iLargest]){
        iLargest = RIGHT(i);
    }
    if(i != iLargest){
        Swap(nums[i], nums[iLargest]);
        MaxHeapFix(nums, iLargest);
    }
}

void MaxHeapBuild(std::vector<int>& nums)
{
    if(nums.size() < 2) return;
    int iLastNotLeaf = nums.size() / 2;
    for(; iLastNotLeaf > 0; --iLastNotLeaf)
    {
        MaxHeapFix(nums, iLastNotLeaf);
    }
}

void MaxHeapSort(std::vector<int>& nums)
{
    if(nums.size() < 2) return;
    std::vector<int> tmpVec;
    tmpVec.reserve(nums.size());
    tmpVec.emplace_back(nums[0]);
    MaxHeapBuild(nums);
    while(nums.size() > 1)
    {
        Swap(nums[1], nums[nums.size() - 1]);
        tmpVec.emplace_back(nums.back());
        nums.pop_back();
        MaxHeapFix(nums, 1);
    }
    nums.swap(tmpVec);
}

int main()
{
    // data set from nums[1]
    std::vector<int> nums = {0,32,13,21,4,32,5,21,4,436,54,3,2431,4,36,54,67546,7,65,5,324,5234,432};
    MaxHeapSort(nums);
    for(auto& it : nums)
    {
        std::cout<<it<<" ";
    }
    return 0;
}