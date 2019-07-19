#include<iostream>
#include<string>

/* 简单工厂相当于一个模子，根据传入的参数通过一些自己的逻辑判断；生产出不同类型的相应的产品、对象。
 * 但这种模式局限性很大，一但参数错误，就不能做出正确的操作，而且新增一些产品就要更改工厂类；所以这种模式一般用于比较简单的场景
*/
namespace simplefactory
{
class MobilePhone
{
    public:
        virtual void production() = 0;
};

class Huawei: public MobilePhone
{
    public:
        void production(){ std::cout<<"Product a huawei moblie phone"<<std::endl; }
};

class Vivo: public MobilePhone
{
    public:
        void production(){ std::cout<<"Product a vivo mobile phone"<<std::endl; }
};

class MobilePhoneFactory
{
    public:
        MobilePhone* CreateMobilePhone(std::string brandName)
        {
            if(brandName.compare("huawei") == 0){
                return new Huawei();
            }
            else if(brandName.compare("vivo") == 0){
                return new Vivo();
            }
            else{
                std::cout<<"Unknown mobile phone"<<std::endl;
                return nullptr;
            }
        }
};
}
/* 工厂方法模式是对简单工厂方法模式的改进，在简单工厂方法模式中，如果传递的字符串出错，
 * 则不能正确创建对象，而且新增一些产品就要更改工厂类而多个工厂方法模式是提供多个工厂方法，分别创建对象.
*/
namespace commonfactory
{
class MobilePhone
{
    public:
        virtual void production() = 0;
};

class Huawei: public MobilePhone
{
    public:
        void production(){ std::cout<<"Product a huawei moblie phone"<<std::endl; }
};

class Vivo: public MobilePhone
{
    public:
        void production(){ std::cout<<"Product a vivo mobile phone"<<std::endl; }
};
// multi factory
class MobilePhoneFactory
{
    public:
        virtual MobilePhone* CreateMobilePhone() = 0;
};
class HuaweiFactory: public MobilePhoneFactory
{
    public:
        MobilePhone* CreateMobilePhone()
        {
            return new Huawei();
        }
};
class VivoFactory: public MobilePhoneFactory
{
    public:
        MobilePhone* CreateMobilePhone()
        {
            return new Vivo();
        }
};
}
/* 抽象工厂为创建一组相关或相互依赖的对象提供一个接口，而且无需指定他们的具体类
 * 这里我们除了生产手机还生产对应的充电器
*/
namespace abstractfactory
{
class MobilePhone
{
    public:
        virtual void production() = 0;
};

class Huawei: public MobilePhone
{
    public:
        void production(){ std::cout<<"Product a huawei moblie phone"<<std::endl; }
};

class Vivo: public MobilePhone
{
    public:
        void production(){ std::cout<<"Product a vivo mobile phone"<<std::endl; }
};
class PhoneCharger
{
    public:
        virtual void production() = 0;
};

class HuaweiCharge: public PhoneCharger
{
    public:
        void production(){ std::cout<<"Product a huawei moblie phone charge"<<std::endl; }
};

class VivoCharge: public PhoneCharger
{
    public:
        void production(){ std::cout<<"Product a vivo mobile phone charge"<<std::endl; }
};

class MobilePhoneFactory
{
    public:
        virtual MobilePhone* CreateMobilePhone() = 0;
		virtual PhoneCharger* CreatePhoneCharger() = 0;
};
class HuaweiFactory: public MobilePhoneFactory
{
    public:
        MobilePhone* CreateMobilePhone()
        {
            return new Huawei();
        }
		PhoneCharger* CreatePhoneCharger()
		{
			return new HuaweiCharge();
		}
};
class VivoFactory: public MobilePhoneFactory
{
    public:
        MobilePhone* CreateMobilePhone()
        {
            return new Vivo();
        }
        PhoneCharger* CreatePhoneCharger()
        {
            return new VivoCharge();
        }
};

}

int main()
{
    // 简单工厂模式
    simplefactory::MobilePhoneFactory* spFactory = new simplefactory::MobilePhoneFactory();
    simplefactory::MobilePhone* phone1 = spFactory->CreateMobilePhone("huawei");
    phone1->production();
    // 普通工厂模式
    commonfactory::HuaweiFactory* cmFactory = new commonfactory::HuaweiFactory();
    commonfactory::MobilePhone* phone2 = cmFactory->CreateMobilePhone();
    phone2->production();
    // 抽象工厂模式
    abstractfactory::MobilePhoneFactory* absFactory = new abstractfactory::HuaweiFactory();
    abstractfactory::MobilePhone* phone3 = absFactory->CreateMobilePhone();
    abstractfactory::PhoneCharger* charger1 = absFactory->CreatePhoneCharger();
    phone3->production();
    charger1->production();
    return 0;
}
