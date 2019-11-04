#include <iostream>
#include <vector>

//Median-of-three
int Media(int* nums, int first, int mid, int last)
{
    if(nums[first] < nums[mid]){
        if(nums[mid] < nums[last]){
            return mid;// first < mid < last
        }
        else if(nums[first] < nums[last]){
            return last;// first < mid, first < last, mid >= last 
        }
        else{
            return first;
        }
    }
    else if(nums[first] < nums[last]){
        return first;// first >= mid, first < last
    }
    else if(nums[mid] < nums[last]){
        return last;// first >= mid, first >= last, mid < last
    }
    else{
        return mid;
    }
}
void QuickSort(int* nums, int first, int last)
{
    if(first > last) return;
    int tmp = nums[Media(nums, first, (first + last)/2, last)];
    int i = first, j = last;
    while(i != j)
    {
        while(nums[j] >= tmp && i < j) --j;
        nums[i] = nums[j];
        while(nums[i] <= tmp && i < j) ++i;
        nums[j] = nums[i];
    }
    nums[i] = tmp;
    QuickSort(nums, first, i - 1);
    QuickSort(nums, i + 1, last);
}

int main()
{
    std::vector<int> nums{64,316,41,64,1,64,35,43,465,7,6,4,35,4,27,52,54,376547,25,6};
    QuickSort(&nums[0], 0, nums.size() - 1);
    for(auto it : nums){
        std::cout<< it << " ";
    }
    getchar();
    return 0;
}
