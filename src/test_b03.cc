#include "thread-pool.h"
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <thread>
using namespace std;

bool test_serial_execution()
{
    try
    {
        cout << "Starting test_serial_execution..." << endl;
        stringstream log;
        mutex mtx;
        ThreadPool pool(1);
        for (int i = 0; i < 5; ++i)
        {
            cout << "Scheduling task " << i << endl;
            pool.schedule([i, &log, &mtx]()
                          {
                lock_guard<mutex> l(mtx);
                cout << "Executing task " << i << endl;
                log << i << " "; });
        }
        cout << "Calling wait..." << endl;
        pool.wait();
        cout << "Wait complete" << endl;
        string result = log.str();
        cout << "Result: '" << result << "'" << endl;
        cout << "Expected: '0 1 2 3 4 '" << endl;
        return result == "0 1 2 3 4 ";
    }
    catch (const exception &e)
    {
        cout << "Exception: " << e.what() << endl;
        return false;
    }
    catch (...)
    {
        cout << "Unknown exception" << endl;
        return false;
    }
}

int main()
{
    for (int i = 0; i < 10; i++)
    {
        cout << "Attempt " << (i + 1) << ":" << endl;
        bool result = test_serial_execution();
        cout << "Result: " << (result ? "PASS" : "FAIL") << endl;
        if (!result)
        {
            cout << "Failed on attempt " << (i + 1) << endl;
            return 1;
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    cout << "All 10 attempts passed!" << endl;
    return 0;
}
