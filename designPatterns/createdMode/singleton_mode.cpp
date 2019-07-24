/* 单例模式即一个进程中一个类只有一个实例，并提供他的全局访问节点，该实例被所有程序模块共享。
 * 根据实例创建时间可以分为 饿汉模式、懒汉模式
 * 单例模式并不复杂，但是需要考虑到线程安全（double checki/volatile）
*/
#include<iostream>
#include<mutex>

class Singleton
{
    public:
        static Singleton* Instance();
        void show();
    private:
        Singleton(){}
    private:
        static Singleton* m_instance;
};

Singleton* Singleton::m_instance = nullptr;

Singleton* Singleton::Instance()
{
    if(m_instance == nullptr){
        static std::once_flag flag;
        std::call_once(flag,[](){m_instance = new Singleton();});
    }
    return m_instance;
}

void Singleton::show(){std::cout<<"\033[32m"<<"Hello World"<<"\033[0m"<<std::endl;}

int main()
{
    Singleton::Instance()->show();
    return 0;
}
