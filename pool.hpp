#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <type_traits>
#include <cassert>
#include "spinlock.hpp"

namespace acp
{
    template <typename T>
    class pool
    {
        typedef std::aligned_storage<sizeof(T),alignof(T)>::type TStore;
        union poolElement 
        {
            TStore storage;
            uint32_t nextFree;
        };

        class poolPage
        {
            uint32_t pageSize;
            uint32_t used;
            uint32_t firstFree;

            enum { NoneFree = 0xFFFFFFFF; }

            poolElement * data;
            std::vector<uint64_t> usage;
        public:
            poolPage() { pageSize = 0; used = 0; firstFree = NoneFree; data = nullptr; }
            poolPage(uint32_t sz) { data= nullptr; alloc(o.pageSize); }
            ~poolPage() 
            { 
                if(data!=nullptr)
                {
                   clear();
                   delete [] data; 
                }
            }
 
            poolPage(const poolPage& o) 
            { 
                data = nullptr;
                if(o.pageSize!=0)
                {
                    alloc(o.pageSize);
                }
                else
                {
                    pageSize = 0;
                    used = 0;
                    firsFree = NoneFree;
                }
            }

            poolPage(poolPage&& o)
            {
                data = o.data;
                firstFree = o.firstFree;
                used = o.used;
                used.swap(o.used);
            }
 
            void clear() 
            {
                for(uint32_t i = 0; i<pageSize;i++)
                {
                    uint64_t mask = (1ULL<<uint64_t(i&63));
                    if(mask & usage[i >> 6])
                    {
                        ((T*)(&data[i].storage))->~T();
                    }
                }
                
                for(auto & i: usage) i=0;
            }

            bool contains( T & ptr) { return ( ((poolElement*)&ptr)-data) < pageSize; }
            bool empty() { return firstFree != NoneFree; }
            bool is_allocated() { return data!=nullptr; }

            void alloc(uint32_t size)
            {
                if(data == nullptr)
                {
                    data = new poolElement[size];
                    usage.clear();
                    usage.resize(size,0);
                    for(uint32_t i = 0; i<size; i++)
                    {
                        data[i].nextFree = i+1;
                    }
                    data[size-1].nextFree = NoneFree;

                    firstFree = 0;
                    pageSize = size;
                    used = 0;
                }
            }

            template<typename ...Args>
            T& get(Args... args) 
            {
                if (empty()) return nullptr;
                uint32_t crt = firstFree;
                assert(crt < pageSize);
                firstFree = data[crt].nextFree;
                assert(firstFree < pageSize || firstFree == NoneFree);
                // assert that the elem is not used;
                assert( (usage[crt >> 6] & (1ULL << uint64_t(crt&63))) == 0);
                usage[crt >> 6] |= (1ULL << uint64_t(crt&63));
                used++;
                return new ((T*)&(data + crt)) T(args...);
            }

            void put_back(T& v)
            {
                T* p = &v;

                assert(used != 0);
                uint32_t idx = p - data;

                // assert that the elem is used;
                assert( (usage[ idx >> 6] & (1ULL << uint64_t(idx&63))) != 0);
                
                p->~p();

                data[idx].nextFree = firstFree;

                usage[idx >> 6] &= ~(1ULL << uint64_t(idx&63));
                used--;

                firstFree = idx;
            }
        };

        uint32_t usage;
        uint32_t poolPageSize;

        std::vector<poolPage> poolStorage;

    public:
        pool(const pool&) = delete;
        pool& operator=(const pool &) = delete;

        pool(uint32_t pageSize)
        {
            poolPageSize = pageSize;
            poolStorage.reserve(4);
            poolStorage.push_back(poolPage());
            poolStorage[0].alloc(poolPageSize);
        }
         template<typename ...Args>
            T& get(Args... args) 
        {
            for (auto & pp: poolStorage)
            {
                if(!pp.empty())
                {
                    return pp.get(args...);
                }
            }
            poolStorage.push_back(poolPage(poolPageSize));
            return this.get(args...);
        }
        void put_back(T& v)
        {
            for(auto & pp: poolStorage)
            {
                if(pp.contains(v))
                {
                    pp.put_back(v);
                    return;
                }
            }
            assert(false && " v is not contained in this pool ");
        }

        void size() { return poolPageSize * poolStorage.size();}
        void usage() { return usage; }
        void clear() 
        {
            for(auto & p:poolStorage)
            {
                p.clear();
            }
        }
    };
}