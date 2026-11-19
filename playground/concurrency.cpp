// playground/concurrency.cpp — EDUCATIONAL ONLY.
//
// The real matching core stays single-threaded for determinism (see
// docs/DESIGN_DECISIONS.md #3). This file demonstrates the concurrency
// primitives that are still fair interview territory: a mutex-protected
// shared counter, an atomic counter, and a short producer/consumer using
// a condition variable.
//
// Build/run: ./playground_concurrency
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace {

// --- data race vs protected access ------------------------------------------
// Two threads incrementing a plain `int` concurrently is a DATA RACE
// (undefined behavior): both threads' read-modify-write can interleave and
// lose updates. std::mutex + std::lock_guard makes each increment atomic
// with respect to other threads.
int sharedCounterUnsafe = 0; // demonstrated only conceptually below, never
                              // actually raced in this file (UB is UB)
int sharedCounterMutex = 0;
std::mutex counterMutex;

void incrementWithMutex(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        std::lock_guard<std::mutex> lock(counterMutex); // RAII: unlocks on scope exit
        ++sharedCounterMutex;
    }
}

// --- std::atomic: lock-free (on common platforms) increment -----------------
std::atomic<int> atomicCounter{0};

void incrementAtomic(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        // Default memory_order_seq_cst: strongest, simplest to reason about.
        // A relaxed ordering could be used here since we don't need this
        // increment to synchronize any OTHER memory access, but seq_cst is
        // the safe default and the one to reach for absent a measured need.
        ++atomicCounter;
    }
}

// --- producer/consumer with a condition variable -----------------------------
std::mutex queueMutex;
std::condition_variable queueCv;
std::queue<int> workQueue;
bool producerDone = false;

void producer() {
    for (int i = 0; i < 5; ++i) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            workQueue.push(i);
        }
        queueCv.notify_one(); // wake one waiting consumer
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        producerDone = true;
    }
    queueCv.notify_all(); // wake any consumer still waiting, so it can exit
}

void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);
        // wait() atomically unlocks while blocked, relocks before returning;
        // the predicate guards against spurious wakeups.
        queueCv.wait(lock, [] { return !workQueue.empty() || producerDone; });
        if (workQueue.empty()) {
            if (producerDone) return;
            continue;
        }
        int item = workQueue.front();
        workQueue.pop();
        lock.unlock(); // release before doing "work" / I/O so we don't hold the lock
        std::cout << "  consumed item=" << item << '\n';
    }
}

} // namespace

int main() {
    (void)sharedCounterUnsafe;

    std::cout << "-- mutex-protected shared counter --\n";
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) threads.emplace_back(incrementWithMutex, 1000);
        for (auto& th : threads) th.join();
    }
    std::cout << "  sharedCounterMutex=" << sharedCounterMutex << " (expected 4000)\n";

    std::cout << "\n-- std::atomic counter --\n";
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) threads.emplace_back(incrementAtomic, 1000);
        for (auto& th : threads) th.join();
    }
    std::cout << "  atomicCounter=" << atomicCounter.load() << " (expected 4000)\n";

    std::cout << "\n-- producer/consumer with condition_variable --\n";
    {
        std::thread producerThread(producer);
        std::thread consumerThread(consumer);
        producerThread.join();
        consumerThread.join();
    }

    std::cout << "\nConcepts not implemented here (deliberately, per project scope):\n"
                 "  - deadlock/lock ordering: avoided by always locking a single mutex\n"
                 "    per critical section above; a two-mutex example would need\n"
                 "    std::scoped_lock(m1, m2) to lock both atomically and avoid\n"
                 "    inconsistent acquisition order across threads.\n"
                 "  - false sharing: independent atomics/counters used by different\n"
                 "    threads but placed on the same cache line cause invisible\n"
                 "    contention; fixed by padding/aligning to the cache line size\n"
                 "    (commonly 64 bytes), not attempted here to keep this short.\n"
                 "  - relaxed memory ordering: default seq_cst used throughout; a\n"
                 "    relaxed ordering could speed up a pure counter with no other\n"
                 "    synchronized data, but is easy to misuse and is a 'measure\n"
                 "    first' optimization, not a default.\n";

    return 0;
}
