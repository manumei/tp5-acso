#include "thread-pool.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads),
                                            newTaskSemaphore(0),
                                            activeTasks(0),
                                            done(false)
{
    // Inicializar todos los workers
    for (size_t i = 0; i < numThreads; i++)
    {
        wts[i].available = true;
        wts[i].assigned = false;
        wts[i].id = i; // hilo worker
        wts[i].ts = thread(&ThreadPool::worker, this, i);
    }

    // Arrancar el hilo dispatcher
    dt = thread(&ThreadPool::dispatcher, this);
}

void ThreadPool::schedule(const function<void(void)> &thunk)
{
    if (!thunk)
    {
        throw invalid_argument("Cannot schedule null function");
    }

    {
        lock_guard<mutex> lg(queueLock);
        if (done)
        {
            throw runtime_error("Cannot schedule task on destroyed ThreadPool");
        }
        taskQueue.push(thunk);
    }
    // Despertar al dispatcher
    newTaskSemaphore.signal();
}

void ThreadPool::wait()
{
    unique_lock<mutex> ul(queueLock);
    // Esperar hasta que se vacie la cola Y no haya tasks ejecutandose
    allTasksComplete.wait(ul, [this]()
                          { return taskQueue.empty() && activeTasks == 0; });

    // Doble check por las dudas de race conditions
    while (!taskQueue.empty() || activeTasks > 0)
    {
        allTasksComplete.wait(ul, [this]()
                              { return taskQueue.empty() && activeTasks == 0; });
    }
}

void ThreadPool::dispatcher()
{
    while (!done)
    {
        // Esperar a que lleguen tasks nuevas
        newTaskSemaphore.wait();

        if (done)
            break;

        // Procesar todas las tasks disponibles
        while (true)
        {
            function<void(void)> task;

            // Sacar task de la cola e incrementar contador atomicamente
            {
                lock_guard<mutex> lg(queueLock);
                if (taskQueue.empty())
                    break;
                task = taskQueue.front();
                taskQueue.pop();
                activeTasks++;
            }

            // Buscar un worker libre y esperar si es necesario
            int workerIndex = -1;
            {
                unique_lock<mutex> ul(workerLock);
                workerAvailable.wait(ul, [this]()
                                     { return done || any_of(wts.begin(), wts.end(),
                                                             [](const worker_t &w)
                                                             { return w.available; }); });

                if (done) // si lo estan cerrando, devuelve la task
                {
                    lock_guard<mutex> lg(queueLock);
                    taskQueue.push(task);
                    activeTasks--;
                    break;
                }

                // Asignar task al primer worker disponible
                for (size_t i = 0; i < wts.size(); i++)
                {
                    if (wts[i].available)
                    {
                        wts[i].available = false;
                        wts[i].assigned = true;
                        wts[i].thunk = task; // asigno task
                        workerIndex = i;
                        break;
                    }
                }
            }

            // Despertar al worker para la task
            if (workerIndex != -1)
            {
                wts[workerIndex].taskReady.signal();
            }
        }
    }
}

void ThreadPool::worker(int id)
{
    while (!done)
    {
        // Esperar a task
        wts[id].taskReady.wait();

        if (done)
            break;

        // Ejecutar la task asignada
        if (wts[id].assigned)
        {
            wts[id].thunk();

            // Despues nos marcamos como disponibles y avisamos
            {
                lock_guard<mutex> workerGuard(workerLock);
                wts[id].available = true; // ya estamos libres
                wts[id].assigned = false; // sin task asignada
                // Avisar al dispatcher que hay worker libre
                workerAvailable.notify_one();
            }

            // Decrementar contador de tasks activas - DESPUeS de ejecutar
            {
                lock_guard<mutex> queueGuard(queueLock);
                activeTasks--;
                if (activeTasks == 0 && taskQueue.empty()) // si terminamos todo
                {
                    allTasksComplete.notify_all(); // despertar al wait()
                }
            }
        }
    }
}

ThreadPool::~ThreadPool()
{
    // Esperar que terminen todas las tasks programadas
    wait();

    done = true;

    // Despertar al dispatcher
    newTaskSemaphore.signal();

    // Despertar workers que esperan disponibilidad
    {
        lock_guard<mutex> lg(workerLock);
        workerAvailable.notify_all();
    }

    // Despertar a todos los workers
    for (size_t i = 0; i < wts.size(); i++)
    {
        wts[i].taskReady.signal();
    }

    // Hacer join de todos los hilos
    if (dt.joinable()) // el dispatcher
    {
        dt.join();
    }

    for (size_t i = 0; i < wts.size(); i++) // todos los workers
    {
        if (wts[i].ts.joinable())
        {
            wts[i].ts.join();
        }
    }
}
