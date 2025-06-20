/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : 
    wts(numThreads), 
    newTaskSemaphore(0),
    activeTasks(0),
    done(false) {
    
    // Initialize worker threads
    for (size_t i = 0; i < numThreads; i++) {
        wts[i].available = true;
        wts[i].assigned = false;
        wts[i].id = i;
        // Start worker thread
        wts[i].ts = thread(&ThreadPool::worker, this, i);
    }
    
    // Start dispatcher thread
    dt = thread(&ThreadPool::dispatcher, this);
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    // Handle nullptr or invalid function
    if (!thunk) {
        throw invalid_argument("Cannot schedule null function");
    }
    
    {
        lock_guard<mutex> lg(queueLock);
        if (done) {
            throw runtime_error("Cannot schedule task on destroyed ThreadPool");
        }
        taskQueue.push(thunk);
    }
    // Signal dispatcher that a new task is available
    newTaskSemaphore.signal();
}

void ThreadPool::wait() {
    unique_lock<mutex> ul(queueLock);
    // Wait until both queue is empty AND no tasks are actively executing
    allTasksComplete.wait(ul, [this]() {
        return taskQueue.empty() && activeTasks == 0;
    });
    
    // Double-check to handle any potential race conditions
    while (!taskQueue.empty() || activeTasks > 0) {
        allTasksComplete.wait(ul, [this]() {
            return taskQueue.empty() && activeTasks == 0;
        });
    }
}

void ThreadPool::dispatcher() {
    while (!done) {
        // Wait for new tasks to be scheduled
        newTaskSemaphore.wait();
        
        if (done) break;
        
        // Process all available tasks
        while (true) {
            function<void(void)> task;
            
            // Get task from queue and increment active counter atomically
            {
                lock_guard<mutex> lg(queueLock);
                if (taskQueue.empty()) break;
                task = taskQueue.front();
                taskQueue.pop();
                activeTasks++; // Increment here when we take the task
            }
            
            // Find an available worker and wait for one if necessary
            int workerIndex = -1;
            {
                unique_lock<mutex> ul(workerLock);
                workerAvailable.wait(ul, [this]() {
                    return done || any_of(wts.begin(), wts.end(), 
                        [](const worker_t& w) { return w.available; });
                });
                
                if (done) {
                    // If we're shutting down, put the task back and decrement counter
                    lock_guard<mutex> lg(queueLock);
                    taskQueue.push(task);
                    activeTasks--;
                    break;
                }
                
                // Assign task to available worker
                for (size_t i = 0; i < wts.size(); i++) {
                    if (wts[i].available) {
                        wts[i].available = false;
                        wts[i].assigned = true;
                        wts[i].thunk = task;
                        workerIndex = i;
                        break;
                    }
                }
            }
            
            // Signal the worker to execute the task
            if (workerIndex != -1) {
                wts[workerIndex].taskReady.signal();
            }
        }
    }
}

void ThreadPool::worker(int id) {
    while (!done) {
        // Wait for task assignment
        wts[id].taskReady.wait();
        
        if (done) break;
        
        // Execute the assigned task
        if (wts[id].assigned) {
            // Execute the task first
            wts[id].thunk();
            
            // Then mark as available and notify dispatcher atomically
            {
                lock_guard<mutex> workerGuard(workerLock);
                wts[id].available = true;
                wts[id].assigned = false;
                // Notify dispatcher that a worker is available - must hold workerLock
                workerAvailable.notify_one();
            }
            
            // Decrease active task count and notify - must be AFTER task execution
            {
                lock_guard<mutex> queueGuard(queueLock);
                activeTasks--;
                if (activeTasks == 0 && taskQueue.empty()) {
                    allTasksComplete.notify_all();
                }
            }
        }
    }
}

ThreadPool::~ThreadPool() {
    // Wait for all scheduled tasks to complete
    wait();
    
    // Signal shutdown
    done = true;
    
    // Wake up dispatcher
    newTaskSemaphore.signal();
    
    // Wake up workers waiting for availability notification - must hold workerLock
    {
        lock_guard<mutex> lg(workerLock);
        workerAvailable.notify_all();
    }
    
    // Wake up all workers
    for (size_t i = 0; i < wts.size(); i++) {
        wts[i].taskReady.signal();
    }
    
    // Join all threads
    if (dt.joinable()) {
        dt.join();
    }
    
    for (size_t i = 0; i < wts.size(); i++) {
        if (wts[i].ts.joinable()) {
            wts[i].ts.join();
        }
    }
}
