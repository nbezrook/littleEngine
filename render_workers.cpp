#include "render_workers.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

struct RenderWorkers {
    //boiler plate cpp
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::condition_variable work,done;
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    size_t count=0,jobs=0,pending=0;
    unsigned long long generation=0;
    bool quit=false,busy=false;
    RenderWorkFn function=nullptr;
    void *user=nullptr;
};

static void worker_loop(RenderWorkers *pool,size_t index)
{
    unsigned long long seen=0;
    std::unique_lock<std::mutex> lock(pool->mutex);
    for(;;) {
        pool->work.wait(lock,[&]{return pool->quit || pool->generation!=seen;});
        if(pool->quit) return;
        seen=pool->generation;
        auto function=pool->function;void *user=pool->user;size_t jobs=pool->jobs;
        lock.unlock();
        for(;;) {
            size_t job=pool->next.fetch_add(1,std::memory_order_relaxed);
            if(job>=jobs) break;
            if(!function(user,job,index)) pool->failed.store(true,std::memory_order_relaxed);
        }
        lock.lock();
        if(--pool->pending==0) pool->done.notify_one();
    }
}
RenderWorkers *render_workers_create(size_t count)
{
    if(count>64) return nullptr;
    //thread count
    if (count == 0) {
        unsigned available = std::thread::hardware_concurrency();

        count = available > 1 ? available - 1 : 1;
    }

    RenderWorkers *pool=nullptr;
    try {
        pool=new RenderWorkers;
        pool->count=count;pool->threads.reserve(count);
        for(size_t i=0;i<count;++i) pool->threads.emplace_back(worker_loop,pool,i);
        return pool;
    } catch(...) {
        render_workers_destroy(pool);
        return nullptr;
    }
}
void render_workers_destroy(RenderWorkers *pool)
{
    if(!pool) return;
    {std::lock_guard<std::mutex> lock(pool->mutex);pool->quit=true;pool->work.notify_all();}
    for(auto &thread:pool->threads) if(thread.joinable()) thread.join();
    delete pool;
}
size_t render_workers_count(const RenderWorkers *pool) {return pool?pool->count:0;}
bool render_workers_run(RenderWorkers *pool,size_t jobs,RenderWorkFn function,void *user)
{
    if(!pool || (jobs && !function) || jobs>std::numeric_limits<size_t>::max()-pool->count) return false;
    std::unique_lock<std::mutex> lock(pool->mutex);
    if(pool->busy || pool->quit) return false;
    if(!jobs) return true;
    pool->busy=true;pool->jobs=jobs;pool->function=function;pool->user=user;
    pool->next.store(0,std::memory_order_relaxed);pool->failed.store(false,std::memory_order_relaxed);
    pool->pending=pool->threads.size();++pool->generation;
    pool->work.notify_all();
    pool->done.wait(lock,[&]{return pool->pending==0;});
    bool success=!pool->failed.load(std::memory_order_relaxed);
    pool->busy=false;pool->function=nullptr;pool->user=nullptr;
    return success;
}
