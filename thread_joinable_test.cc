#include <iostream>
#include <thread>

int main(int argv, char **args)
{
    std::thread *t1, *t2;

    t1 = new std::thread([&t1]
                         {
         std::this_thread::sleep_for(std::chrono::seconds(1));
         std::cout << "t1: " << std::boolalpha << t1->joinable() << std::endl;

         /* dead lock */
         //  t1->join();

         if (t1->joinable())
         {
             t1->detach();
             std::cout << "t1: after detached, joinable: " << std::boolalpha << t1->joinable() << std::endl;
         } });

    t2 = new std::thread([&t2]
                         {
         std::this_thread::sleep_for(std::chrono::seconds(1));
         std::cout << "t2: " << std::boolalpha << t2->joinable() << std::endl;

         /* dead lock */
         //  t2->join();

         if (t2->joinable())
         {
             t2->detach();
             std::cout << "t2: after detached, joinable: " << std::boolalpha << t2->joinable() << std::endl;
         } });

    std::cout << "main thread id: " << std::boolalpha << std::this_thread::get_id() << std::endl;
    std::cout << "main t1 joinable: " << std::boolalpha << t1->joinable() << std::endl;
    std::cout << "main t2 joinable: " << std::boolalpha << t2->joinable() << std::endl;

    if (t1->joinable())
    {
        t1->join();
        std::cout << "main: t1 after joined, joinable: " << std::boolalpha << t1->joinable() << std::endl;
    }

    /* if no joinable judge, this occures an error
    t2->join();
    std::cout << "main: t2 after joined, joinable: " << std::boolalpha << t2->joinable() << std::endl;
    */

    /*right*/
    if (t2->joinable())
    {
        t2->join();
        std::cout << "main: t2 after joined, joinable: " << std::boolalpha << t2->joinable() << std::endl;
    }

    delete t2;
    delete t1;

    return 0;
}

/// @test_resutl: join() 之前最好都跟上 joinable()的判断，以防其他地方join或者detach后报错