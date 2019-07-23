/* 原型模式，以配钥匙理解即可*/
#include<iostream>
#include<string>

class KeyPrototype
{
    public:
        virtual KeyPrototype* Clone() = 0;
};

class MyKey: public KeyPrototype
{
    public:
     MyKey(){
        std::cout<<"Create one key"<<std::endl;
    }
    KeyPrototype* Clone(){
        return new MyKey();
    }
};
/*Prototype模式和Builder模式、AbstractFactory模式都是通过一个类（对象实例）来
 * 专门负责对象的创建工作（工厂对象），它们之间的区别是：
 * Builder模式重在复杂对象的一步步创建（并不直接返回对象）
 * AbstractFactory模式重在产生多个相互依赖类的对象
 * Prototype模式重在从自身复制自己创建新类。
*/
int main()
{
    KeyPrototype* myFirstKey = new MyKey();
    KeyPrototype* mySecondKey = new MyKey();
    delete myFirstKey;
    delete mySecondKey;
    return 0;
}
