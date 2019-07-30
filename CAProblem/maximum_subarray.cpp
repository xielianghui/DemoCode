/* 求整数数组的最大连续子数组之和
 * input: [-2,1,-3,4,-1,2,1,-5,4],
 * Output: 6
 * Explanation: [4,-1,2,1] has the largest sum = 6.
*/
#include<iostream>
#include<vector>
// 暴力遍历 O(n^2)
namespace violent
{
    int ViolentSolution(std::vector<int>& nums)
    {
        unsigned int vecSize = nums.size();
        int maxSubArraySum = 0;
        for (unsigned int i = 0; i < vecSize; ++i)
        {
            int tmp = 0;
            for (unsigned int j = i; j < vecSize; ++j)
            {
                tmp += nums[j];
                if (tmp > maxSubArraySum) {
                    maxSubArraySum = tmp;
                }
            }
        }
        return maxSubArraySum;
    }
}
/* 分治法 O(n*logn)
 * 使用分治技术意味着我们要将子数组划分为两个规模尽量相等的子数组，也就是找到子数组的中央位置（比如mid），
 * 然后考虑求解两个子数组A[low..mid]和A[mid+1..high]。A[low..high]的任意连续子数组A[i..j]所处的位置必然是以下三种情况之一：
 *  （a）完全位于子数组A[low..mid]中，因此low≤i≤j≤mid
 *  （b）完全位于子数组A[mid+1..high]中，因此mid≤i≤j≤high
 *  （c）跨越了中点，因此low≤i≤mid≤j≤high
 * 所以我们需要求解这三种情况的最大子数组，然后再取最大值
 * a,b两种情况是很简单的递归。
 * 对于c这种情况，我们可以像下面getMaxCrossArrayRes()代码实现一样理解：
 * 以mid为固定起始点，向两边搜索计算，然后把两边的结果相加就是跨越中点的子数组的最大和
*/
namespace divide
{
    int getMaxCrossArrayRes(int* arr, int low, int mid, int high)
    {
        int lRes = arr[mid];
        int rRes = arr[mid];
        int sum = arr[mid];
        for (int i = mid - 1; i >= low; --i)
        {
            sum += arr[i];
            lRes = (lRes > sum ? lRes : sum);
        }
        sum = arr[mid];
        for (int i = mid + 1; i <= high; ++i)
        {
            sum += arr[i];
            rRes = (rRes > sum ? rRes : sum);
        }
        return lRes + rRes - arr[mid];
    }
    int getMaxSubArrayRes(int* arr, int low, int high)
    {
        if (low == high) return arr[low];
        int mid = (low + high) / 2;
        int leftRes = getMaxSubArrayRes(arr, low, mid);
        int rightRes = getMaxSubArrayRes(arr, mid + 1, high);
        int crossRes = getMaxCrossArrayRes(arr, low, mid, high);
        if (leftRes >= rightRes && leftRes >= crossRes) {
            return leftRes;
        }
        else if (rightRes >= leftRes && rightRes >= crossRes) {
            return rightRes;
        }
        else {
            return crossRes;
        }
    }
    int DivideSolution(std::vector<int>& nums)
    {
        int high = nums.size();
        if (high == 0) return 0;
        int* arr = &nums[0];
        return getMaxSubArrayRes(arr, 0, --high);
    }
}
/* Kadane算法 O(n)
 * 由于真正的最大连续子数列一定存在一个结尾元素(单个的就是自己)，所以我们可以遍历数组假设遍历到的那一位
 * 是我们要的结尾元素。假设我们遍历到了nums[i]这一位，要么他单个就是最大，要么就是和nums[i-1]为结尾元素时
 * 得到的最大子数列结合最大。然后我们把这些结果的最大值记录到maxSoFar里，等遍历完就得到了最大子数列的和。
*/
namespace kadane
{
    int KadaneSolution(std::vector<int>& nums)
    {
        int arrSize = nums.size();
        if (arrSize == 0) return 0;
        int maxSoFar = nums[0];
        int maxEndingHere = nums[0];
        for (int i = 1; i < arrSize; ++i)
        {
            maxEndingHere = maxEndingHere + nums[i] > nums[i] ? maxEndingHere + nums[i] : nums[i];
            maxSoFar = maxSoFar > maxEndingHere ? maxSoFar : maxEndingHere;
        }
        return maxSoFar;
    }
}

int main()
{
    std::vector<int> nums{-2,1,-3,4,-1,2,1,-5,4};
    std::cout << violent::ViolentSolution(nums) << std::endl;
    std::cout << divide::DivideSolution(nums) << std::endl;
    std::cout << kadane::KadaneSolution(nums) << std::endl;
    return 0;
}

