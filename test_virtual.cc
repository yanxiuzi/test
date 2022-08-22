#include <iostream>
#include <string>
#include <memory>

class AA
{
public:
    virtual ~AA()
    {
        std::cout << "~AA" << std::endl;
    }
    virtual void fun(int i)
    {
        std::cout << "fun(" << i << ")" << std::endl;
    };
};

class BB : public virtual AA
{
public:
    ~BB()
    {
        std::cout << "~BB" << std::endl;
    }
};

class CC : public virtual AA
{
public:
    ~CC()
    {
        std::cout << "~CC" << std::endl;
    }
};

class DD : public BB, CC
{
public:
    ~DD()
    {
        std::cout << "~DD" << std::endl;
    }
};

class EE : public AA
{
public:
    ~EE()
    {
        std::cout << "~EE" << std::endl;
    }
};

int main_t1()
{
    std::unique_ptr<AA> p;
    std::string input;
    {
        p.reset(new AA);
        std::cin >> input;
        std::cout << "-----------------------" << input << std::endl;
    }
    {
        p.reset(new BB);
        std::cin >> input;
        std::cout << "-----------------------" << input << std::endl;
    }
    {
        p.reset(new CC);
        std::cin >> input;
        std::cout << "-----------------------" << input << std::endl;
    }
    {
        p.reset(new DD);
        std::cin >> input;
        std::cout << "-----------------------" << input << std::endl;
    }
    {
        p.reset(new EE);
        std::cin >> input;
    }

    return 0;
}