#include "thread-pool.h"
#include <iostream>
#include <sstream>
#include <mutex>
using namespace std;

int main() {
    cout << "Testing serial execution..." << endl;
    
    stringstream log;
    mutex mtx;
    ThreadPool pool(1);
    
    for (int i = 0; i < 5; ++i) {
        cout << "Scheduling task " << i << endl;
        pool.schedule([i, &log, &mtx]() {
            lock_guard<mutex> l(mtx);
            cout << "Executing task " << i << endl;
            log << i << " ";
        });
    }
    
    cout << "Calling wait()..." << endl;
    pool.wait();
    cout << "Wait() complete" << endl;
    
    string result = log.str();
    cout << "Result: '" << result << "'" << endl;
    cout << "Expected: '0 1 2 3 4 '" << endl;
    
    bool success = (result == "0 1 2 3 4 ");
    cout << "Test " << (success ? "PASSED" : "FAILED") << endl;
    
    return success ? 0 : 1;
}
