/* 求最长回文串
 * input: [a,b,a,b,d]
 * output: [a,b,a] or [b,a,b]
*/
#include <iostream>
#include <algorithm>

// 普通遍历 O(n^2)
std::string longestPalindrome1(std::string s) {
    int strSize = s.size();
    int startPos = 0;
    int maxSize = 0;
    for (int i = 1; i < strSize; ++i)
    {
        auto CompareLR = [&](int& front, int& behind, int& tmpSize) {
            for (; front >= 0 && behind < strSize; --front, ++behind)
            {
                if (s[front] == s[behind]) {
                    ++tmpSize;
                }
                else {
                    return;
                }
            }
        };
        int tmpSize = 0;
        int f = i - 1, b = i;
        CompareLR(f, b, tmpSize);
        if (tmpSize * 2 >= maxSize) {
            maxSize = tmpSize * 2;
            startPos = f + 1;
        }
        tmpSize = 0;
        f = i - 1;
        b = i + 1;
        CompareLR(f, b, tmpSize);
        if (tmpSize * 2 + 1 >= maxSize) {
            maxSize = tmpSize * 2 + 1;
            startPos = f + 1;
        }
    }
    return s.substr(startPos, maxSize);
}

/* Manacher算法 O(n) http://www.javabin.cn/2018/manacher.html
 * 算法思想类似与KMP，尽量利用遍历过得到的已知信息，避免普通遍历。
*/
std::string longestPalindrome2(std::string s) {
    std::string ss;
    int sSize = s.size();
    int ssSize = s.size() * 2 + 1;
    ss.reserve(ssSize);
    for (int i = 0; i < ssSize; ++i)
    {
        ss += (i % 2 == 0 ? '#' : s[i / 2]);
    }
    int mx = 0;
    int pos = 0;
    int* r = new int[ssSize] {0};
    int maxPos = 0;
    int maxVal = 0;
    for (int i = 0; i < ssSize; ++i)
    {
        r[i] = mx > i ? std::min(r[2 * pos - i], mx - i) : 1;
        while (i - r[i] >= 0 && i + r[i] < ssSize) {
            if (ss[i - r[i]] == ss[i + r[i]]) {
                ++r[i];
            }
            else {
                break;
            }
        }
        if (i + r[i] > mx) {
            mx = i + r[i];
            pos = i;
        }
        if (r[i] > maxVal) {
            maxVal = r[i];
            maxPos = i;
        }
    }
    return s.substr((maxPos + 1 - maxVal) / 2, maxVal - 1);
}

int main()
{
    std::string a = "babad";
    std::cout << longestPalindrome1(a) << std::endl;
	std::cout << longestPalindrome2(a) << std::endl;
    return 0;
}
