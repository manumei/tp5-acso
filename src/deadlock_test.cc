#include "thread-pool.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
using namespace std;

// This test replicates the scenario from test_potential_deadlock 
// but without the race condition in the test code itself
bool test_threadpool_no_deadlock() {
    const int num_iterations = 50;
    
    cout << "Testing ThreadPool deadlock avoidance..." << endl;
    
    for (int iter = 0; iter < num_iterations; iter++) {
        if (iter % 10 == 0) {
            cout << "  Iteration " << (iter + 1) << "/" << num_iterations << endl;
        }
        
        atomic<bool> ready{false};
        atomic<bool> finished{false};
        mutex done_mutex;
        condition_variable done_cv;
        
        thread t([&]() {
            try {
                ThreadPool pool(2);
                
                // Schedule a task that might cause issues if not handled properly
                pool.schedule([&ready]() {
                    ready = true;
                    // Simulate some work
                    this_thread::sleep_for(chrono::milliseconds(10));
                });
                
                pool.wait();  // This should complete successfully
                
                {
                    lock_guard<mutex> lock(done_mutex);
                    finished = true;
                }
                done_cv.notify_one();
                
            } catch (...) {
                {
                    lock_guard<mutex> lock(done_mutex);
                    finished = false;
                }
                done_cv.notify_one();
            }
        });
        
        bool result = false;
        {
            unique_lock<mutex> lock(done_mutex);
            result = done_cv.wait_for(lock, chrono::milliseconds(1000), [&]() {
                return finished.load();
            });
        }
        
        if (t.joinable()) {
            t.join();
        }
        
        if (!result) {
            cout << "ERROR: Test timed out or failed at iteration " << iter << endl;
            return false;
        }
        
        if (!ready.load()) {
            cout << "ERROR: Task was not executed at iteration " << iter << endl;
            return false;
        }
    }
    
    cout << "ThreadPool deadlock avoidance test PASSED" << endl;
    return true;
}

int main() {
    cout << "=== ThreadPool Deadlock Test ===" << endl;
    
    bool passed = test_threadpool_no_deadlock();
    
    cout << "\n=== Result ===" << endl;
    cout << "Test: " << (passed ? "PASSED" : "FAILED") << endl;
    
    return passed ? 0 : 1;
}
