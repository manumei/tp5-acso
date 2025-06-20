#include "thread-pool.h"
#include <iostream>
#include <vector>
using namespace std;

int main()
{
    cout << "Testing basic functionality..." << endl;

    ThreadPool pool(2);
    vector<int> result(3, 0);

    for (int i = 0; i < 3; ++i)
    {
        cout << "Scheduling task " << i << endl;
        pool.schedule([i, &result]()
                      {
            cout << "Executing task " << i << endl;
            result[i] = i + 1;
            cout << "Task " << i << " complete, result[" << i << "] = " << result[i] << endl; });
    }

    cout << "Calling wait()..." << endl;
    pool.wait();
    cout << "Wait() complete" << endl;

    cout << "Result: [";
    for (int i = 0; i < 3; ++i)
    {
        cout << result[i] << (i < 2 ? ", " : "");
    }
    cout << "]" << endl;

    bool success = (result[0] == 1 && result[1] == 2 && result[2] == 3);
    cout << "Test " << (success ? "PASSED" : "FAILED") << endl;

    return success ? 0 : 1;
}
