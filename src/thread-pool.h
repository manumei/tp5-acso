#ifndef _thread_pool_
#define _thread_pool_

#include <cstddef>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex> // la 'talking pillow' xd (S01, E04 para cultos)
#include <condition_variable>
#include "Semaphore.h"

using namespace std;

// Un worker que labura en el thread pool
typedef struct worker
{
  thread ts;
  function<void(void)> thunk; // la task
  bool available;
  bool assigned;
  int id;
  Semaphore taskReady; // para avisarle
} worker_t;

class ThreadPool
{
public:
  // Crea un pool con la cantidad de hilos que quieras
  ThreadPool(size_t numThreads);

  // Programa una task pa' que la ejecute algun worker
  void schedule(const function<void(void)> &thunk);

  // Espera a que terminen todas las tasks
  void wait();

  // Destructor que limpia todo bien
  ~ThreadPool();

private:
  void worker(int id);
  void dispatcher();
  thread dt;            // hilo para tasks
  vector<worker_t> wts; // todos los workers
  mutex queueLock;
  queue<function<void(void)>> taskQueue; // pendientes
  Semaphore newTaskSemaphore;
  mutex workerLock;
  condition_variable workerAvailable;  // libera notification
  int activeTasks;                     // cuantas rn
  condition_variable allTasksComplete; // wake up
  atomic<bool> done;

  ThreadPool(const ThreadPool &original) = delete;
  ThreadPool &operator=(const ThreadPool &rhs) = delete;
};
#endif
