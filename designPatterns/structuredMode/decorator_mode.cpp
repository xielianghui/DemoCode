/* 装饰器模式也称为Wrapper(包装器)，即动态的给已经定义好的类添加新职能。*/

#include<iostream>
#include<string>

class Pancake
{
    public:
        virtual std::string getDesc() = 0;
        virtual double Cost() = 0;
};

class CondimentDecorator: public Pancake{};
    
class EggPancake: public Pancake
{
    public:
        std::string getDesc(){ return "EggPancake"; }
        double Cost(){ return 5.0; }
};

class BaconCondiment: public CondimentDecorator
{
    public:
        BaconCondiment(Pancake* tmp):m_basePancake(tmp){}
        std::string getDesc(){ return m_basePancake->getDesc() + ",Bacon"; }
        double Cost(){ return m_basePancake->Cost() + 1.5; }
    private:
        Pancake* m_basePancake;
};

int main()
{
    Pancake* baseP = new EggPancake();
    CondimentDecorator* baseC = new BaconCondiment(baseP);
    std::cout<< baseC->getDesc()<<std::endl;
    std::cout<< baseC->Cost()<<std::endl;

    delete baseP;
    delete baseC;
    return 0;
}
