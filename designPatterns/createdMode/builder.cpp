/* Builder模式的定义：将一个复杂的对象的构建和其部件分离，将耦合度降到最低的一种设计模式。
 * 通俗的说，某个对象在创建过程中会设置很多的属性（很重要），需要一步一步的堆砌，为了把
 * 这个复杂的堆砌过程和自身每个属性设置解耦，可以单独设置某个属性，这种设计的模式就是
 * builder模式，也就是将复杂模型对象的创建，和部件的表示分离。
*/
#include<iostream>
#include<string>

class MobilePhone
{
    public:
        virtual void setCPU(std::string cpuName) = 0;
        virtual void setOS(std::string osName) = 0;
        virtual void AboutPhone() = 0;
    protected:
        std::string m_cpu;
        std::string m_os;
};

class HuaWei: public MobilePhone
{
    public:
        void setCPU(std::string cpuName){
            m_cpu = cpuName;
        }
        void setOS(std::string osName){
            m_os = osName;
        }
        void AboutPhone(){
            std::cout<<" HuaWei"<<std::endl;
            if(!m_cpu.empty()){
                std::cout<<" CPU: "<<m_cpu<<std::endl;
            }
            if(!m_os.empty()){
                std::cout<<" OS: "<<m_os<<std::endl;
            }
        }
};

class MobilePhoneBuilder
{
    public:
        virtual void BuildCPU(std::string cpuName) = 0;
        virtual void BuildOS(std::string osName) = 0;
        virtual MobilePhone* CreateMobilePhone() = 0;
};

class HuaWeiBuilder: public MobilePhoneBuilder
{
    public:
        HuaWeiBuilder(){
            m_hw = new HuaWei();
        }
        ~HuaWeiBuilder(){
            delete m_hw;
        }
        void BuildCPU(std::string cpuName){
            m_hw->setCPU(cpuName);
        }
        void BuildOS(std::string osName){
            m_hw->setOS(osName);
        }
        MobilePhone* CreateMobilePhone(){
            return m_hw;
        }
    private:
        HuaWei* m_hw;
};
/* 优点：客户端不必知道产品内部组成的细节，属性和产品创建解耦，相同过程可以产生不同产品；
 * 具体的builder相互独立，不同的builder产生不同的产品；更加精细的控制产品；系统拓展方便；
 * 
 * 缺点：代码量增加，如属性设置，添加一倍工作量；当属性多时容易遗忘builder相对于的添加该属性方法
*/
int main()
{   
    MobilePhoneBuilder* builder = new HuaWeiBuilder();
    builder->BuildCPU("麒麟970");
    builder->BuildOS("鸿蒙");
    MobilePhone* mp = builder->CreateMobilePhone();
    mp->AboutPhone();
    return 0;
}
