#include<dlfcn.h>
#include<iostream>
#include<string>

int main(int argc, char** argv)
{
    if(argc < 2){
        return -1;
    }
    char* libName = argv[1];
    void* hdl = dlopen(libName, RTLD_NOW);
    char* pzerror = dlerror();
    if (pzerror != 0) {
        std::cerr << "Failed to Load Library "<< libName << "! error msg: " << pzerror << std::endl;
        return -1;
    }
    typedef void (* FuncSayHello)();
    FuncSayHello func = (FuncSayHello)dlsym(hdl, "SayHello");
    pzerror = dlerror();
    if (0 != pzerror) {
        std::cerr << libName <<" has no factory function " << " error msg: " << pzerror << std::endl;
        return -1;
    }
    (*func)();
    return 0;

}
