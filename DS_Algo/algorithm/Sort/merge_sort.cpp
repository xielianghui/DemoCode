#include <iostream>
#include <vector>

// 归并排序
void Merge(int* nums, int s, int m, int e)
{
    int tmpPos = 0;
    int tmpSize = e - s + 1;
    int tmpNums[tmpSize];
    int i = s;
    int j = m + 1;
    while(i <= m && j <= e)
    {
        tmpNums[tmpPos++] = nums[i] < nums[j] ? nums[i++] : nums[j++];
    }
    while(i <= m)
    {
        tmpNums[tmpPos++] = nums[i++];
    }
    while(j <= e)
    {
        tmpNums[tmpPos++] = nums[j++];
    }
    for(tmpPos = 0; tmpPos < tmpSize;)
    {
        nums[s++] = tmpNums[tmpPos++];
    }
}

void MergeSort(int* nums, int s, int e)
{
    if(s < e){
        int m = (s + e) / 2;
        MergeSort(nums, s, m);
        MergeSort(nums, m + 1, e);
        Merge(nums, s, m, e);
    }
}

int main()
{
    std::vector<int> a = {32,13,21,4,21,321,4,3,254,7,65,523,4,23,654,3,2,4,32,423,5,43,654,7,658,43};
    MergeSort(&a[0], 0, a.size() - 1);
    for(auto& it : a)
    {
        std::cout<<it<<" ";
    }
    return 0;
}