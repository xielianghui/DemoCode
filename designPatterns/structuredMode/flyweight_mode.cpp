/* flyweight即轻量级的意思，中文翻译为享元。该模式主要的设计目的是为了迎合系统大量相似数据的应用而生，
 * 减少用于创建和操作相似的细碎对象所花费的成本。大量的对象会消耗高内存，享元模式给出了一个解决方案，
 * 即通过共享对象来减少内存负载。
 * 享元模式比较迷惑在于理解两种状态的分类：
 * 1.内蕴状态是对象本身的属性，在生成对象以后一般不会进行改变，比如工具中的属性：名字、大小、重量等，
 *   还有就是我们一般需要一个关键性的属性作为其区别于其他对象的key，如工具的话我们可以把名称作为找到工具的唯一标识。
 * 2.外蕴状态是对象的外部描述，是每个对象的可变部分，比如对工具的使用地点、使用时间、使用人、工作内容的描述，这些属性
 *   不属于对象本身，而是根据每回使用情况进行变化的，这就需要制作成接口进行外部调用，而外蕴状态的维护是由调用者维护的，
 *   对象内不进行维护。
*/

#include<iostream>
#include<string>
#include<unordered_map>
#include<memory>
class Tool
{
    public:
        // 外蕴状态
        virtual void Used(std::string person, std::string work) = 0;
    protected:
        // 内蕴状态
        std::string m_name;
};

class Hammer: public Tool
{
    public:
        Hammer(){m_name = "hammer"; std::cout<<"Create a hammer!"<<std::endl;}
        void Used(std::string person, std::string work){
            std::cout<<person<<" use "<<m_name<<" for "<<work<<std::endl;
        }
};

class Screwdriver: public Tool
{
    public:
        Screwdriver(){ m_name = "screwdriver"; std::cout<<"Create a screwdriver!"<<std::endl;}
        void Used(std::string person, std::string work){
            std::cout<<person<<" use "<<m_name<<" for "<<work<<std::endl;
        }
};

// FlyweightFactory
class ToolBox
{
    public:
        ToolBox() = default;
        ~ToolBox() = default;
        std::shared_ptr<Tool> getTool(const std::string& name){
            if(name.compare("hammer") == 0){
                if(m_toolMap.count("hammer") == 0){
                    auto tmp = std::make_shared<Hammer>();
                    m_toolMap["hammer"] = tmp;
                }
                return m_toolMap["hammer"];
            }
            else if (name.compare("screwdriver") == 0){
                if(m_toolMap.count("screwdriver") == 0){
                    auto tmp = std::make_shared<Screwdriver>();
                    m_toolMap["screwdriver"] = tmp;
                }
                return m_toolMap["screwdriver"];
            }
            else{
                return nullptr;
            }
        }
    private:
        std::unordered_map<std::string, std::shared_ptr<Tool>> m_toolMap;
};

int main()
{
    ToolBox tb;
    auto it1 = tb.getTool("hammer");
    if(it1){
        it1->Used("A hui", "zhuangxiu");
    }
    auto it2 = tb.getTool("screwdriver");
    if(it2){
        it1->Used("A hui", "kaisuo");
    }
    return 0;
}
