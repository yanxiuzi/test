// test_template_virtual_destructor.cc
#include <functional>
#include <map>
#include <memory>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#else
#include <cxxabi.h>
#endif

#include <iostream>

class noncopyable
{
    noncopyable(const noncopyable &) = delete;
    noncopyable& operator=(const noncopyable &) = delete;

  protected:
    noncopyable() = default;
    virtual ~noncopyable() = default;
};

inline std::string GetClearName(const char* name)
{
#ifdef _WIN32
    char buff[512]{ 0 };
    auto ret = UnDecorateSymbolName(name, buff, sizeof(buff), UNDNAME_COMPLETE);
    if (ret == 0) {
        std::cout <<  "LastError: " << GetLastError() << std::endl;
        return std::string(name);
    }
    return std::string(buff);
#else
    int status = -1;
    char* clear_name = abi::__cxa_demangle(name, NULL, NULL, &status);
    const char* demangle_name = (status == 0) ? clear_name : name;
    std::string ret_val(demangle_name);
    free(clear_name);
    return ret_val;
#endif
}

template <typename T> class Singleton : noncopyable
{
    // using Self_T = Singleton<T>;
    using Self_T = T;
  public:
    static Self_T &Instance()
    {
        static Self_T instance;
        return instance;
    }

    virtual int fun(){
        return 2;
    };

  protected:
    Singleton()
    {
        std::cout << GetClearName(typeid(Self_T).name()) << std::endl;
    };
    ~Singleton()
    {
        std::cout << "~" << GetClearName(typeid(Self_T).name()) << std::endl;
    }
};


class Test
{
public:
    virtual int fun()
    {
        return 1;
    };

    Test()
    {
        std::cout << "Test" << std::endl;
    }

    ~Test()	// error.
   //    virtual ~Test()
    {
        std::cout << "~Test" << std::endl;
    };
};

class TestSon : public Test, public Singleton<Test>
{
public:
    int fun()
    {
        return 0;
    };

    TestSon()
    {
        std::cout << "TestSon" << std::endl;
    }
    ~TestSon()
    {
        std::cout << "~TestSon" << std::endl;
    };
};


int main_2()
{
    TestSon t1;
    TestSon *t2 = new TestSon;

    // Singleton<Test> tt; // protected constructor.
    Singleton<Test> *tt1 = &t1;
    Test *tt2 = t2;

    // Singleton<TestSon> *tt1 = new TestSon1;
    // Test *tt2 = new TestSon1;

    std::cout << tt1->fun() << std::endl;
    std::cout << tt2->fun() << std::endl;

    Test &tt3 = Singleton<Test>::Instance();
    std::cout << tt3.fun() << std::endl;

    // delete tt1; // protected destructor.
    delete tt2;

    return 0;
}
