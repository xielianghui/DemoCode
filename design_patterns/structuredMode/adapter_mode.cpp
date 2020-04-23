/* 适配器模式即将一个类的接口转换成我们希望的另外一个接口。
 * 其主要应用于“希望复用一些现存的类，但是接口又与复用环境要求不一致的情况”。
 * 有对象适配器和类适配器两种形式的实现结构，但是类适配器采用“多继承”的实现方式，
 * 带来了不良的高耦合，所以一般不推荐使用。对象适配器采用“对象组合”的方式，更符合松耦合精神。
 * 以电源适配器为例：
*/
#include<iostream>
#include<string>

namespace classadapter
{
    class CNOutlet
    {
        public:
            CNOutlet():m_voltage(220){}
            void Output(){
                std::cout<<"Output "<<m_voltage<<"V"<<std::endl;
            }
        private:
            const int m_voltage;
    };

    class JPOutlet
    {
        public:
            JPOutlet():m_voltage(110){}
            void Output(){
                std::cout<<"Output "<<m_voltage<<"V"<<std::endl;
            }
        private:
            const int m_voltage;
    };
	
    class PowerAdapter:public CNOutlet,JPOutlet
    {
        public:
            void Output(){
                this->JPOutlet::Output();
                std::cout<<"Do some change!"<<std::endl;
                this->CNOutlet::Output();
            }
    };
}

namespace objadapter
{
    class CNOutlet
    {
        public:
            CNOutlet():m_voltage(220){}
            void Output(){
                std::cout<<"Output "<<m_voltage<<"V"<<std::endl;
            }
        private:
            const int m_voltage;
    };

    class JPOutlet
    {
        public:
            JPOutlet():m_voltage(110){}
            void Output(){
                std::cout<<"Output "<<m_voltage<<"V"<<std::endl;
            }
        private:
            const int m_voltage;
    };

    class PowerAdapter:public CNOutlet
    {
        public:
            PowerAdapter(JPOutlet* jpOut):m_jpOut(jpOut){}
            void Output(){
                m_jpOut->Output();
                std::cout<<"Do some change!"<<std::endl;
                this->CNOutlet::Output();
            }
        private:
            JPOutlet* m_jpOut;
    };
}

int main()
{
    std::cout<<"class adapter mode:"<<std::endl;
    classadapter::PowerAdapter* cd = new classadapter::PowerAdapter();
    cd->Output();
    
    std::cout<<"object adapter mode:"<<std::endl;
    objadapter::JPOutlet* jpOl = new objadapter::JPOutlet();
    objadapter::PowerAdapter* od = new objadapter::PowerAdapter(jpOl);
    od->Output();

    delete cd;
    delete jpOl;
    delete od;
    return 0;
}


