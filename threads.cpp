#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <thread>
#include <functional>
#include <algorithm>
#include <queue>
#include <future>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <boost/filesystem.hpp>
#include <atomic>

using namespace std;
using namespace boost::filesystem;

//  Test function to simulate reading a file and returns its length.  
auto readFile = [](const string& filePath, int sleepMilli) -> size_t {
    int fileSize = 0;

    ifstream fs(filePath.c_str());

    size_t numBytes = 0;
    if(fs.is_open()) {
        fs.seekg(0, ios::end);
        numBytes = fs.tellg();
    }

    fs.close();
    if(sleepMilli > 0) {
        this_thread::sleep_for(chrono::milliseconds(sleepMilli));
    }

    return numBytes;
};

class ThreadPool {
private:
    atomic_bool running_;
    vector<thread> threads_;
    queue<packaged_task<size_t()>> jobs_;
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
            packaged_task<size_t()> job;
            {
                unique_lock<mutex> lock(mutex_);
                cond_.wait(lock, [&] {
                    return !jobs_.empty() || !running_;
                });

                if(!running_) {
                    return;
                }
                job = move(jobs_.front());
                jobs_.pop();
                lock.unlock();
            }
            if(job.valid()){
                job();
            }
        }
    }

    void queueJob(packaged_task<size_t()>&& job) {
        unique_lock<mutex> lock(mutex_);
        jobs_.emplace(move(job));
    }

    void stop() {
        running_ = false;
        cond_.notify_all();

        int i = 0;
        for(auto& worker: threads_) {
            if(worker.joinable()) {
                worker.join();
            }
        }
        cout << "Thread pool stopped gracefully" << endl;
    }

    template<typename Func, class... Args>
    future<size_t> submit_job(Func&& f, Args... args) {
        packaged_task<size_t()> task(bind(move(f), args...));
        future<size_t> fut = task.get_future();
        queueJob(move(task));
        cond_.notify_one();
        return fut;
    }
};

int main() {
    ThreadPool pool;

    cout << "Enter a directory and we'll process all .txt files in it" << endl;
    cout << "Or Q to quit" << endl;
    string input;
    cin >> input;

    vector<future<size_t>> bytesRead;

    while(input != "Q") {
        path dir(input);
        if(is_directory(dir)) {
            cout << "Processing files in directory " << input << endl;
            for(auto entry: directory_iterator(dir)) {
                int sleepMillis = 100 + rand()%500;
                bytesRead.push_back(pool.submit_job(readFile, entry.path().string(), sleepMillis));
            }

        } else {
            cout << "Error - exiting" << input << endl;
        }
        cout << "Enter a directory and we'll process all .txt files in it" << endl;
        cout << "Or Q to quit" << endl << flush;
        cin >> input;
    }
    cout << "Counting bytes read...." << endl;
    size_t total = 0;
    for(auto iter = bytesRead.begin(); iter != bytesRead.end(); ++iter) {
        total += iter->get();
        cout << "Total = " << total << endl;
    }
    cout << "Exiting..." << endl;
}
