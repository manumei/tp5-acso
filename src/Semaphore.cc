#include <mutex>
#include "Semaphore.h"
#include <condition_variable>

// Constructor que inicializa el contador
Semaphore::Semaphore(int count) : count_(count) {}

// Liberar el semaforo, despierta hilos esperando
void Semaphore::signal()
{
    lock_guard<mutex> lg(mutex_);
    count_++;
    if (count_ == 1)
        condition_.notify_all(); // despertar si pasamos de 0 a 1
}

// Esperar hasta que el semaforo este disponible
void Semaphore::wait()
{
    lock_guard<mutex> lg(mutex_);
    condition_.wait(mutex_, [this]()
                    { return count_ > 0; }); // esperar mientras count_ sea 0
    count_--;
}
