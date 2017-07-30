#pragma once
#include <atomic>
#include <thread>

namespace acp
{
    template<typename JE>
    class ticket_spinlock 
    {
    public:
        struct __native_type {
            std::atomic<uint32_t> ticket;
            std::atomic<uint32_t> turn;
        };

        typedef __native_type* 			native_handle_type;

       
        ticket_spinlock() noexcept
        {
            counters.ticket = 0;
            counters.turn = 0;
        }
        ~ticket_spinlock() = default;

        void lock() 
        {
            uint32_t ticket = counters.ticket.fetch_add(1, std::memory_order::memory_order_acq_rel);
            uint32_t iterC = 0;
            
            const uint32_t SPIN_TRESHOLD = 32;

            while(counters.turn.load(std::memory_order_relaxed) < ticket)
            {
                iterC++;
                if(iterC > SPIN_TRESHOLD)
                {
                    JE::yield();
                    while(counters.turn.load(std::memory_order_relaxed) < ticket)
                    {
                        JE::yield();
                    }
                    return;
                }
            }
        }

        bool try_lock()
        {
            uint32_t cticket = counters.ticket.load(std::memory_order_relaxed);
            if(!counters.ticket.compare_exchange_weak(cticket, cticket+1, std::memory_order_release, std::memory_order_relaxed))
            {
                return false;
            }
            return true;
        }

        void unlock()
        {
            counters.turn++;
        }

        native_handle_type native_handle() noexcept
        {
            return &counters;
        }

    private:
        __native_type counters;
    };
};
