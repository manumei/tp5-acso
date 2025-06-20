#include "thread-pool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
using namespace std;

// Test 1: Heavy concurrent scheduling and waiting
bool test_concurrent_schedule_wait_stress() {
    const int num_threads = 8;
    const int num_pools = 10;
    const int tasks_per_pool = 100;
    const int iterations = 5;
    
    cout << "Running concurrent schedule/wait stress test..." << endl;
    
    for (int iter = 0; iter < iterations; iter++) {
        cout << "  Iteration " << (iter + 1) << "/" << iterations << endl;
        
        vector<thread> threads;
        atomic<int> total_executed(0);
        atomic<bool> all_done(false);
        
        // Create multiple threads that create pools, schedule tasks, and wait
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                random_device rd;
                mt19937 gen(rd());
                uniform_int_distribution<> dis(1, 5);
                
                for (int p = 0; p < num_pools; p++) {
                    ThreadPool pool(dis(gen)); // Random number of workers
                    atomic<int> pool_executed(0);
                    
                    // Schedule tasks
                    for (int i = 0; i < tasks_per_pool; i++) {
                        pool.schedule([&pool_executed, &total_executed, i]() {
                            pool_executed++;
                            total_executed++;
                            // Simulate some work
                            this_thread::sleep_for(chrono::microseconds(10));
                        });
                    }
                    
                    // Wait for completion
                    pool.wait();
                    
                    // Verify all tasks executed
                    if (pool_executed.load() != tasks_per_pool) {
                        cout << "ERROR: Pool " << p << " executed " << pool_executed.load() 
                             << " tasks instead of " << tasks_per_pool << endl;
                        return;
                    }
                }
            });
        }
        
        // Join all threads
        for (auto& t : threads) {
            t.join();
        }
        
        int expected_total = num_threads * num_pools * tasks_per_pool;
        if (total_executed.load() != expected_total) {
            cout << "ERROR: Total executed " << total_executed.load() 
                 << " instead of " << expected_total << endl;
            return false;
        }
    }
    
    cout << "Concurrent schedule/wait stress test PASSED" << endl;
    return true;
}

// Test 2: Rapid pool creation/destruction
bool test_rapid_pool_lifecycle() {
    const int num_iterations = 100;
    const int tasks_per_iteration = 50;
    
    cout << "Running rapid pool lifecycle test..." << endl;
    
    for (int i = 0; i < num_iterations; i++) {
        if (i % 20 == 0) {
            cout << "  Iteration " << (i + 1) << "/" << num_iterations << endl;
        }
        
        atomic<int> executed_count(0);
        
        {
            ThreadPool pool(4);
            
            // Schedule tasks rapidly
            for (int j = 0; j < tasks_per_iteration; j++) {
                pool.schedule([&executed_count, j]() {
                    executed_count++;
                    // Small amount of work
                    volatile int x = 0;
                    for (int k = 0; k < 1000; k++) x++;
                });
            }
            
            // Pool destructor should wait for all tasks
        }
        
        // Verify all tasks completed
        if (executed_count.load() != tasks_per_iteration) {
            cout << "ERROR: Iteration " << i << " executed " << executed_count.load() 
                 << " tasks instead of " << tasks_per_iteration << endl;
            return false;
        }
    }
    
    cout << "Rapid pool lifecycle test PASSED" << endl;
    return true;
}

// Test 3: Multiple wait() calls from different threads
bool test_concurrent_wait_calls() {
    const int num_wait_threads = 8;
    const int num_tasks = 100;
    
    cout << "Running concurrent wait calls test..." << endl;
    
    ThreadPool pool(4);
    atomic<int> executed_count(0);
    atomic<bool> scheduling_done(false);
    
    // Thread that schedules tasks slowly
    thread scheduler([&]() {
        for (int i = 0; i < num_tasks; i++) {
            pool.schedule([&executed_count]() {
                executed_count++;
                this_thread::sleep_for(chrono::microseconds(100));
            });
            this_thread::sleep_for(chrono::microseconds(50));
        }
        scheduling_done = true;
    });
    
    // Multiple threads calling wait()
    vector<thread> waiters;
    vector<atomic<bool>> wait_results(num_wait_threads);
    
    for (int i = 0; i < num_wait_threads; i++) {
        wait_results[i].store(false);
        waiters.emplace_back([&, i]() {
            // Wait for some tasks to be scheduled
            while (executed_count.load() < 10 && !scheduling_done.load()) {
                this_thread::sleep_for(chrono::milliseconds(1));
            }
            
            pool.wait();
            
            // After wait() returns, all scheduled tasks should be done
            wait_results[i].store(executed_count.load() == num_tasks);
        });
    }
    
    scheduler.join();
    for (auto& t : waiters) {
        t.join();
    }
    
    // Check results
    for (int i = 0; i < num_wait_threads; i++) {
        if (!wait_results[i].load()) {
            cout << "ERROR: Wait thread " << i << " failed" << endl;
            return false;
        }
    }
    
    cout << "Concurrent wait calls test PASSED" << endl;
    return true;
}

// Test 4: Schedule from within tasks (nested scheduling)
bool test_nested_scheduling_stress() {
    const int levels = 3;
    const int tasks_per_level = 10;
    
    cout << "Running nested scheduling stress test..." << endl;
    
    ThreadPool pool(4);
    atomic<int> total_executed(0);
    
    function<void(int)> schedule_recursive = [&](int level) {
        total_executed++;
        
        if (level > 0) {
            for (int i = 0; i < tasks_per_level; i++) {
                pool.schedule([&, level]() {
                    schedule_recursive(level - 1);
                });
            }
        }
    };
    
    // Start the recursive scheduling
    pool.schedule([&]() {
        schedule_recursive(levels);
    });
    
    pool.wait();
    
    // Calculate expected total
    int expected = 1; // Initial task
    int level_tasks = 1;
    for (int i = 0; i < levels; i++) {
        level_tasks *= tasks_per_level;
        expected += level_tasks;
    }
    
    if (total_executed.load() != expected) {
        cout << "ERROR: Expected " << expected << " tasks, got " << total_executed.load() << endl;
        return false;
    }
    
    cout << "Nested scheduling stress test PASSED" << endl;
    return true;
}

// Test 5: FIFO ordering stress test
bool test_fifo_ordering_stress() {
    const int num_tests = 20;
    const int tasks_per_test = 100;
    
    cout << "Running FIFO ordering stress test..." << endl;
    
    for (int test = 0; test < num_tests; test++) {
        if (test % 5 == 0) {
            cout << "  Test " << (test + 1) << "/" << num_tests << endl;
        }
        
        ThreadPool pool(1); // Single worker to ensure FIFO
        stringstream result;
        mutex result_mutex;
        
        // Schedule tasks that write their index
        for (int i = 0; i < tasks_per_test; i++) {
            pool.schedule([&result, &result_mutex, i]() {
                lock_guard<mutex> lock(result_mutex);
                result << i << " ";
            });
        }
        
        pool.wait();
        
        // Verify FIFO order
        string output = result.str();
        stringstream expected;
        for (int i = 0; i < tasks_per_test; i++) {
            expected << i << " ";
        }
        
        if (output != expected.str()) {
            cout << "ERROR: FIFO violation in test " << test << endl;
            cout << "Expected: " << expected.str().substr(0, 100) << "..." << endl;
            cout << "Got:      " << output.substr(0, 100) << "..." << endl;
            return false;
        }
    }
    
    cout << "FIFO ordering stress test PASSED" << endl;
    return true;
}

int main() {
    cout << "=== ThreadPool Race Condition Stress Tests ===" << endl;
    
    vector<pair<string, function<bool()>>> tests = {
        {"Concurrent Schedule/Wait Stress", test_concurrent_schedule_wait_stress},
        {"Rapid Pool Lifecycle", test_rapid_pool_lifecycle},
        {"Concurrent Wait Calls", test_concurrent_wait_calls},
        {"Nested Scheduling Stress", test_nested_scheduling_stress},
        {"FIFO Ordering Stress", test_fifo_ordering_stress}
    };
    
    bool all_passed = true;
    
    for (auto& test : tests) {
        cout << "\n--- " << test.first << " ---" << endl;
        auto start = chrono::high_resolution_clock::now();
        
        bool passed = test.second();
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        
        cout << test.first << ": " << (passed ? "PASSED" : "FAILED") 
             << " (" << duration.count() << "ms)" << endl;
        
        if (!passed) {
            all_passed = false;
        }
    }
    
    cout << "\n=== Summary ===" << endl;
    cout << "All tests: " << (all_passed ? "PASSED" : "FAILED") << endl;
    
    return all_passed ? 0 : 1;
}
