#include <iostream>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <thread>
#include <chrono>

using namespace stdexec;
using namespace exec;

// A simulated async computation task
task<int> async_compute(int id, int duration_ms) {
    // We are currently on whatever thread called us or resumed us.
    std::cout << "[Task " << id << "] Started on thread " << std::this_thread::get_id() << "\n";

    // Simulate work by sleeping (blocking the thread, but okay for demo)
    // In a real async I/O, we would suspend here waiting for I/O.
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));

    // Return a value
    int result = id * 10;
    std::cout << "[Task " << id << "] Finished on thread " << std::this_thread::get_id() << " with result " << result << "\n";
    co_return result;
}

task<void> complex_workflow(scheduler auto sched) {
    std::cout << "[Workflow] Starting workflow on thread " << std::this_thread::get_id() << "\n";

    // 1. Switch to the thread pool
    co_await schedule(sched);
    std::cout << "[Workflow] Switched to thread pool " << std::this_thread::get_id() << "\n";

    // 2. Launch two tasks concurrently using when_all
    // Note: async_compute returns a task (Sender).
    // We need to ensure they start on the thread pool, so we might need to be careful.
    // However, since we are already on the thread pool (due to step 1),
    // simply calling them and awaiting the result of when_all works if they are just tasks.
    // BUT: standard tasks are lazy.

    std::cout << "[Workflow] Launching concurrent tasks...\n";

    // We construct the tasks. They haven't started yet.
    auto t1 = async_compute(1, 500);
    auto t2 = async_compute(2, 300);

    // Use when_all to run them together.
    // Since async_compute as written above doesn't internally switch threads immediately
    // but executes synchronously until the first suspend,
    // and since our "async_compute" implementation *is* synchronous (blocking sleep),
    // they would actually run serially if we just did this on one thread.
    //
    // To make them truly parallel on the thread pool, we should make sure each task
    // starts by scheduling itself onto the thread pool or use `on`.
    // Let's modify the usage here to force parallelism:

    auto s1 = on(sched, std::move(t1));
    auto s2 = on(sched, std::move(t2));

    auto [res1, res2] = co_await when_all(std::move(s1), std::move(s2));

    std::cout << "[Workflow] Concurrent tasks finished. Results: " << res1 << ", " << res2 << "\n";

    co_return;
}

int main() {
    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    std::cout << "[Main] Main thread is " << std::this_thread::get_id() << "\n";

    try {
        // Run the workflow and wait for it to finish
        sync_wait(complex_workflow(sched));
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
