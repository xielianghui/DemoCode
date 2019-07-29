#include<iostream>
#include<string>

#ifdef EXTERNC
extern "C" {
#endif

void SayHello()
{
    std::cout<<"Hello world"<<std::endl;
}
#ifdef EXTERNC
}
#endif
