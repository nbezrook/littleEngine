#ifndef LITTLEENGINE_RENDER_WORKERS_H
#define LITTLEENGINE_RENDER_WORKERS_H
#include <stdbool.h>
#include <stddef.h>

// define workers who will take jobs in threaded exchange
typedef struct RenderWorkers RenderWorkers;

typedef bool (*RenderWorkFn)(void *user,size_t job_index,size_t worker_index);


#ifdef __cplusplus
extern "C" {
#endif
/** Allocate/start persistent workers. Zero chooses 1..4 based on hardware.
 * Explicit count is 1..64. NULL on invalid count, allocation or thread failure.
 * Constructor cleans up partial thread creation. No dispatch-time allocation. */
RenderWorkers *render_workers_create(size_t count);
/** Owner thread only, after dispatch returns. NULL accepted; wakes and joins all
 * workers before freeing resources. Never call from a job or concurrently. */
void render_workers_destroy(RenderWorkers *workers);
size_t render_workers_count(const RenderWorkers *workers);
/** Synchronous, non-reentrant dispatch: each index [0,job_count) runs once.
 * user and all borrowed data remain valid until return; callback must not throw.
 * Callbacks may run concurrently and must own disjoint outputs. No per-pixel
 * locks are used. Returns false for invalid/reentrant dispatch or any failed job;
 * all dispatched jobs finish before return. Zero jobs allows NULL callback. */
bool render_workers_run(RenderWorkers *workers,size_t job_count,RenderWorkFn function,void *user);
#ifdef __cplusplus
}
#endif
#endif
