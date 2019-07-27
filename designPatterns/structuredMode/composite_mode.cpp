/* 组合模式即将对象组合成树形结构以表示“部分-整体”的层次结构，使得用户对单个对象和组合对象的使用具有一致性。
 * 模式采用树形结构来实现普遍存在的对像容器，从而将“一对多”的关系转化为“一对一”的关系，使得客户代码可以一致的
 * （复用）处理对象和对象容器，无需关心处理的是单个对象还是组合的对象容器。
*/

#include<iostream>
#include<string>
#include<list>

class AbsFile
{
    public:
        void GetFileName(){std::cout<<m_name;}
        virtual void AddChild(AbsFile* file) = 0;
        virtual void RemoveChild(AbsFile* file) = 0;
        virtual std::list<AbsFile*> * GetChildren() = 0;
    protected:
        std::string m_name;
};

class File: public AbsFile
{
    public:
        File(std::string name){m_name = name;}
        void AddChild(AbsFile* file){}
        void RemoveChild(AbsFile* file){}
        std::list<AbsFile*> * GetChildren(){return nullptr;}
};

class Floder: public AbsFile
{
    public:
        Floder(std::string name){m_name = name;}
        void AddChild(AbsFile* file){
            m_childList.push_back(file);
        }
        void RemoveChild(AbsFile* file){
            m_childList.remove(file);
        }
        std::list<AbsFile*> * GetChildren(){return &m_childList;}

    private:
        std::list<AbsFile*> m_childList;
};

int main()
{
    AbsFile* firstDir = new Floder("/home");
    AbsFile* secondDir = new Floder("/frame");
    AbsFile* file = new File("/a.exe");

    firstDir->AddChild(secondDir);
    firstDir->AddChild(file);

    std::list<AbsFile*> * cl = firstDir->GetChildren();
    firstDir->GetFileName();
    for(auto& it : *cl){
        it->GetFileName();
    }
    std::cout<<std::endl;
    return 0;
}
