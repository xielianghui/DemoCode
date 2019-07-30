/* 门面模式就是用一个门面类来处理子系统的复杂关系，门面类简单的Api接口供客户端调用。*/

#include<iostream>
#include<string>

class Camera
{
    public:
        void TrunOn(){std::cout << "摄像机打开!" << std::endl;}
        void TrunOff(){std::cout << "摄像机关闭!" << std::endl;}
};

class Light
{
    public:
        void TrunOn(){std::cout << "灯光打开!" << std::endl;}
        void TrunOff(){std::cout << "灯光关闭!" << std::endl;}
};

class Facade
{
public:
    Facade()
    {
        m_camera = new Camera();
        m_lights = new Light();
    }
    ~Facade()
    {
        delete m_camera;
        delete m_lights;
    }
    void Activate()
    {
        std::cout<< "激活设备开始直播!"<<std::endl;
        m_camera->TrunOn();
        m_lights->TrunOn();

    }
    void Deactivate()
    {
        std::cout << "关闭设备!" << std::endl;
        m_camera->TrunOff();
        m_lights->TrunOff();
    }

private:
    Camera *m_camera;
    Light *m_lights;
};

int main()
{
    Facade * facade = new Facade();
    facade->Activate();
    std::cout <<  "直播中!" << std::endl;
    facade->Deactivate();
    delete facade;
    return 0;
}
