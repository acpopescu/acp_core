#include <stdio.h>
#include <future>
#include <random>
#include <vector>
#include <iostream>
#include "spinlock.hpp"
#include <deque>
#include <cassert>


std::random_device glbRnd;
std::mt19937_64 randEngine;
std::uniform_real_distribution<float> sleeping(0,10000);
std::mutex mre;

/*
let's do a circular vector as a lf stack, locking only when we need to 
*/
namespace acp
{
    struct JobEngine{
        static inline void yield() { std::this_thread::yield();}
        static bool inside_jobengine() { return true; }
    };

    class refcounted
    {
    public:
        refcounted():refcnt(0) {}

        void add_ref() { refcnt++; }
        void release_ref() 
        {
            uint32_t val = refcnt.fetch_sub(-1);
            if(val == 1)
            {
                this->onRelease();
            }

        }
        uint32_t get_refcnt() { return refcnt.load(std::memory_order_relaxed);}
        virtual void onRelease() = 0;
    private:
        std::atomic<uint32_t> refcnt;
    };


    class jq_work_set
    {
    public:
        std::atomic<uint32_t> numReadyJobs;
        std::atomic<uint32_t> concurrency;
        std::vector<uint32_t> priority;


    };

    template<typename JE = JobEngine>
    class jq_synchro_base: public refcounted
    {
        acp::ticket_spinlock<JE> wqLock;
        
        std::deque<jq_synchro_base*> deps_on_me;
        std::atomic<uint32_t> depCount;
        std::shared_ptr<jq_work_set> workSetOwner;
        

        enum class state {
            not_ready,
            ready
        };

        volatile state status;
        std::mutex mutcv;
        std::condition_variable cv;

        virtual void onRelease() { delete this; }
    public:
        jq_synchro_base()
        {
            depCount = 0;
            status = state::not_ready;
        }

        ~jq_synchro_base()
        {
            wait();
        }

        void wait()
        {
            if(!JE::inside_jobengine())
            {
                std::unique_lock<std::mutex> l(mutcv);
                cv.wait(l);
                return;
            }
            while(depCount.load(std::memory_order_acquire) != 0 )
            {
                JE::yield();
            }
            while(depCount.load(std::memory_order_acquire) != 0 || status != state::ready)
            {
                JE::yield();
            }
        }
        
        void kick()
        {
            auto val = --depCount;
            if(depCount == 0)
            {
                workSetOwner.numReadyJobs++;
            }
        }

        void set_value()
        {
            if(status != state::not_ready)
            {
                return;
            }
            
            assert(depCount.load(std::memory_order_relaxed) == 0);

            wqLock.lock();
            assert(status == state::not_ready);
            status = state::ready;
            for(auto & x:deps_on_me)
            {
                x->kick();
                x->release_ref();
            }
            deps_on_me.clear();
            wqLock.unlock();

            cv.notify_all();
        }

        bool is_ready() 
        {
            return status == state::ready;
        }

        void add_dep_on_this(jq_synchro_base * x)
        {
            if(status == state::ready)
            {
                return;
            }
            x->add_ref();

            wqLock.lock();
            if(status == state::ready)
            {
                wqLock.unlock();
                x->release_ref();
                return;
            }
            deps_on_me.push_back(x);
            x->depCount++;
            wqLock.unlock();
        }
    };
}

int main()
{
    randEngine.seed(glbRnd());
    return 0;
}