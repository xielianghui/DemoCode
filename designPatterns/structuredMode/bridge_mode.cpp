/* 桥接模式即将抽象部分与它的实现部分分离，使他们都能够独立地变化。
 * 继承是一种强耦合的结构。子类的实现与它的父类有很紧密的依赖关系。
 * 以至于父类实现的不论什么变化必定会导致子类发生变化。所以，我们在用继承时。
 * 一定要在是“is-a”的关系时再考虑使用，而不是不论什么时候都去使用。
*/
#include<iostream>
#include<string>

class MobilePhoneSoftware
{
    public:
        virtual void Run() = 0;
};

class QQ: public MobilePhoneSoftware
{
    public:
        void Run(){
            std::cout<< "QQ starting"<<std::endl;
        }
};

class WeChat: public MobilePhoneSoftware
{
    public:
        void Run(){
            std::cout<< "WeChat starting"<<std::endl;
        }
};

class MobilePhone
{
    public:
        void InstallSoftware(MobilePhoneSoftware* tmp){
            m_software = tmp;
        }
        void RunSoftware(){
            if(m_software){
                m_software->Run();
            }
        }
    protected:
        MobilePhoneSoftware* m_software;
};

class HuaWei: public MobilePhone{};
class Vivo: public MobilePhone{};

int main()
{
    MobilePhone* phone1 = new HuaWei();
    phone1->InstallSoftware(new QQ());
    phone1->RunSoftware();

    MobilePhone* phone2 = new Vivo();
    phone2->InstallSoftware(new WeChat());
    phone2->RunSoftware();
    return 0;
}
