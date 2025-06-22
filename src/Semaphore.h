#ifndef _semaphore_
#define _semaphore_

#include <condition_variable>
#include <mutex>

using namespace std;

class Semaphore
{
public:
    Semaphore(int count = 0);
    void signal(); // liberar
    void wait();   // esperar

private:
    int count_;
    mutex mutex_;
    condition_variable_any condition_;

    Semaphore(const Semaphore &orig) = delete;
    Semaphore &operator=(const Semaphore &orig) = delete;
};

#endif
