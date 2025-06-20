#include "thread-pool.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;

// Test that exercises ThreadPool extensively without any test code race conditions
int main() {
    cout << "Testing ThreadPool implementation under Helgrind..." << endl;
    
    // Test 1: Basic functionality
    {
        ThreadPool pool(4);
        atomic<int> counter{0};
        
        for (int i = 0; i < 100; ++i) {
            pool.schedule([&counter]() {
                counter++;
                this_thread::sleep_for(chrono::milliseconds(1));
            });
        }
        
        pool.wait();
        cout << "Test 1 - Basic: " << (counter == 100 ? "PASS" : "FAIL") << endl;
    }
    
    // Test 2: Multiple concurrent schedule/wait cycles
    {
        ThreadPool pool(2);
        atomic<int> total{0};
        
        for (int cycle = 0; cycle < 10; ++cycle) {
            atomic<int> cycle_counter{0};
            
            for (int i = 0; i < 20; ++i) {
                pool.schedule([&cycle_counter, &total]() {
                    cycle_counter++;
                    total++;
                });
            }
            
            pool.wait();
            if (cycle_counter != 20) {
                cout << "Test 2 - Cycle " << cycle << ": FAIL" << endl;
                return 1;
            }
        }
        
        cout << "Test 2 - Multiple cycles: " << (total == 200 ? "PASS" : "FAIL") << endl;
    }
    
    // Test 3: Concurrent pool operations from multiple threads
    {
        ThreadPool pool(3);
        atomic<int> counter{0};
        vector<thread> schedulers;
        
        for (int t = 0; t < 4; ++t) {
            schedulers.emplace_back([&pool, &counter]() {
                for (int i = 0; i < 25; ++i) {
                    pool.schedule([&counter]() {
                        counter++;
                    });
                }
            });
        }
        
        for (auto& t : schedulers) {
            t.join();
        }
        
        pool.wait();
        cout << "Test 3 - Concurrent scheduling: " << (counter == 100 ? "PASS" : "FAIL") << endl;
    }
    
    // Test 4: Pool lifecycle stress test
    {
        atomic<int> total{0};
        
        for (int i = 0; i < 5; ++i) {
            ThreadPool pool(2);
            atomic<int> local_counter{0};
            
            for (int j = 0; j < 20; ++j) {
                pool.schedule([&local_counter, &total]() {
                    local_counter++;
                    total++;
                });
            }
            
            pool.wait();
            // Pool destructor is called here
        }
        
        cout << "Test 4 - Lifecycle stress: " << (total == 100 ? "PASS" : "FAIL") << endl;
    }
    
    cout << "All ThreadPool tests completed." << endl;
    return 0;
}
