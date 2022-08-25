#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <thread>
#include <functional>
#include <algorithm>
#include <queue>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <boost/filesystem.hpp>
#include <atomic>

using namespace std;
using namespace boost::filesystem;


auto readFile = [](const string& filePath, int sleepMilli) {
    int fileSize = 0;

    ifstream fs(filePath.c_str());

    if(fs.is_open()) {
        string line;
        int lineNo = 1;
        while(getline(fs, line)) {
        }
        fs.close();
    }

    cout << "Done reading file - " << filePath << endl;
    if(sleepMilli > 0) {
        this_thread::sleep_for(chrono::milliseconds(sleepMilli));
    }

    cout << "Done sleeping - " << filePath << endl << flush;
};

class ThreadPool {
private:
    atomic_bool running_;
    vector<thread> threads_;
    queue<function<void()>> jobs_;
    mutable mutex mutex_;
    mutable condition_variable cond_;
    once_flag once_;
public:
    ThreadPool():
        running_(false)
    {
        call_once(once_, &ThreadPool::init, this);
    }

    ~ThreadPool() {
        stop();
    }

    void init() {
        int N = thread::hardware_concurrency();
        try {
            threads_.reserve(N);

            running_ = true;
            for(int i = 0; i < N; ++i) {
                threads_.push_back(thread(&ThreadPool::run, this));
            }
            cout << "Thread pool (size " << N << ") initialized" << endl;
        }
        catch(...) {
            running_ = false;
            throw;  //up
        }
    }

    void run() {
        while(running_) {
            function<void()> job;
            {
                unique_lock<mutex> lock(mutex_);
                cond_.wait(lock, [this] {
                    return !jobs_.empty() || !running_;
                });

                if(!running_) {
                    return;
                }
                job = jobs_.front();
                jobs_.pop();
                lock.unlock();
            }
            job();
        }
    }

    void queueJob(const function<void()>& job) {
        {
            unique_lock<mutex> lock(mutex_);
            jobs_.push(job);
        }
    }

    void stop() {
        running_ = false;
        cout << "Thread pool shutting down - signaling worker threads" << endl;
        cond_.notify_all();

        int i = 0;
        for(auto& worker: threads_) {
            if(worker.joinable()) {
                worker.join();
            }
        }
        cout << "Thread pool stopped gracefully" << endl;
    }

    template<typename Func>
    void submit_job(Func f) {
        queueJob(function<void()>(f));
        cond_.notify_one();
    }

};

int main() {
    ThreadPool pool;

    cout << "Enter a directory and we'll process all .txt files in it" << endl;
    cout << "Or Q to quit" << endl;
    string input;
    cin >> input;

    while(input != "Q") {
        path dir(input);
        if(is_directory(dir)) {
            cout << "Processing files in directory " << input << endl;
            for(auto entry: directory_iterator(dir)) {
                int sleepMillis = 5000;
                pool.submit_job(bind(readFile, entry.path().string(), sleepMillis));
            }

        } else {
            cout << "Error - exiting" << input << endl;
        }
        cout << "Enter a directory and we'll process all .txt files in it" << endl;
        cout << "Or Q to quit" << endl << flush;
        cin >> input;
    }
    cout << "Exiting..." << endl;
}
