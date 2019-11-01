#include <iostream>
#include <vector>

class MaxHeap
{
    public:
        MaxHeap()
        {
            m_vec.reserve(100);
            m_vec.emplace_back(-1);
        }
        ~MaxHeap() = default;
        void Insert(int val)
        {
            m_vec.emplace_back(val);
            InsertFix();
        }
        void Delete()
        {
            if(m_vec.size() < 1) return;
            m_vec[1] = m_vec.back();
            m_vec.pop_back();
            DeleteFix();
        }
        int Top()
        {
            if(m_vec.size() > 1) return m_vec[1];
            else return m_vec[0];
        }
        void Show()
        {
            for(int i = 1; i < m_vec.size(); ++i)
            {
                std::cout<<m_vec[i]<<" ";
            }
            std::cout<<std::endl;
        }
    private:
        void InsertFix()
        {
            if(m_vec.size() < 3) return;
            int tmp = m_vec.back();
            int i = m_vec.size() - 1;
            while(i > 1 && tmp > m_vec[i / 2])
            {
                i = i / 2;
            }
            m_vec.back() = m_vec[i];
            m_vec[i] = tmp;
        }
        void DeleteFix()
        {
            if(m_vec.size() < 3) return;
            int tmp = m_vec[1];
            int i = 1;
            while((2 * i + 1) < m_vec.size())
            {
                if(tmp < m_vec[2*i] || tmp < m_vec[2*i+1]){
                    if(m_vec[2*i] < m_vec[2*i+1]){
                        m_vec[i] = m_vec[2*i+1];
                        i = 2 * i + 1;
                    }
                    else{
                        m_vec[i] = m_vec[2*i];
                        i = 2 * i;
                    }
                }
                else{
                    break;   
                }
            }
            if(i < m_vec.size()) m_vec[i] = tmp;
        }
    private:
        std::vector<int> m_vec;
};


int main()
{
    MaxHeap hp;
    hp.Insert(1);
    hp.Insert(2);
    hp.Insert(10);
    hp.Insert(14);
    hp.Insert(15);
    hp.Insert(20);
    hp.Show();
    hp.Delete();
    hp.Show();
    getchar();
    return 0;
}
