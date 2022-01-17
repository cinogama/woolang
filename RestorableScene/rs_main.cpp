#define _CRT_SECURE_NO_WARNINGS

#include "rs_roroutine_thread.hpp"

#include <thread>
#include <chrono>

void _fthread_test()
{
    printf("Helloworld\n");

    rs::fthread::yield();

    printf("This just a test\n");

    rs::fthread::yield();

    printf("Hey!\n");
}

int main()
{
    using namespace std;

    rs::fiber _main;

    while (true)
    {
        rs::fthread fth(_fthread_test);

        fth.join(&_main);
    }
}