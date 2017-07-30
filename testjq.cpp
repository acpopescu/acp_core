#include <stdio.h>
#include <future>
#include <random>
#include <vector>
#include <iostream>
#include "spinlock.hpp"
#include <stack>
#include <assert.h>

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

    template<typename JE = JobEngine>
    class jq_synchro_base: public refcounted
    {
        acp::ticket_spinlock<JE> wqLock;
        
        std::stack<jq_synchro_base*> deps_on_me;
        std::atomic<uint32_t> depCount;
        
        enum class state {
            not_ready,
            ready
        };

        volatile state status;

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

        void wait_for_my_deps()
        {
            while(deps_on_me.load(std::memory_order_acquire) != 0)
            {
                JE::yield();
            }
        }

        void wait()
        {
            wait_for_my_deps();
            while(status != state::ready)
            {
                JE::yield();
            }
        }

        void set_value()
        {
            wqLock.lock();
            status = state::ready;
            for(auto & x:deps_on_me)
            {
                x->depCount--;
                x->release_ref();
            }
            deps_on_me.clear();
            wqLock.unlock();
        }

        bool is_ready() 
        {
            return status == state::ready;
        }

        void add_dep_on_this(jq_synchro_base * x)
        {
            x->add_ref();

            wqLock.lock();
            if(status == state::ready)
            {
                wqLock.unlock();
                x->release_ref();
                return;
            }
            deps_on_me.push(x);
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