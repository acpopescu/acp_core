#include <stdio.h>
#include <atomic>
#include <future>
#include <random>
#include <vector>
#include <iostream>

std::random_device glbRnd;
std::mt19937_64 randEngine;
std::uniform_real_distribution<float> sleeping(0,10000);
std::mutex mre;

std::future<uint32_t> jobtest(uint32_t id)
{
    return std::async( std::launch::async, 
        [=]() -> uint32_t {
            float f;
            {
                std::lock_guard<std::mutex> lg(mre);
                f = sleeping(randEngine);
            }
            printf("async job %d - sleeping for %f ms\n", id, f);
            std::this_thread::sleep_for(std::chrono::duration<float,std::milli>(f));
            std::thread::id myId = std::this_thread::get_id();
            std::cout << "async job " << id << " - " << myId << std::endl;
            return id;
        }
    );    
}

int main()
{
    uint32_t def = glbRnd();
    
    printf("Hello World of jobs!\n");
    printf(" Default random device has max %d and min %d\n", glbRnd.min, glbRnd.max);
    printf(" Seeding mersenne random number generator with %d\n", def);
    randEngine.seed(def);

    std::uniform_real_distribution<float> dis(0, 1);
    printf(" 10 random numbers:\n");
    for (uint32_t i = 0; i<10; i++)
    {
        printf(" %d > %7.5f\n", i, dis(randEngine));
    }
    
    std::vector<std::future<uint32_t> > oplist;

    for(uint32_t i = 0; i<10; i++)
    {
        oplist.push_back(jobtest(i));
    }

    std::this_thread::sleep_for( std::chrono::milliseconds((int64_t)sleeping(randEngine)) );
    
    printf("main done sleeping, waiting for the kids\n");
    int cnt;
    for( auto & x: oplist)
    {
        x.wait();
        printf("Done waiting for %d\n", x.get());
    }
    
    return 0;
}