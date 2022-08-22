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


auto readFile = [](const string& filePath) {
    int fileSize = 0;

    ifstream fs(filePath.c_str());

    if(fs.is_open()) {
        string line;
        int lineNo = 1;
        while(getline(fs, line)) {
        }
        fs.close();
    }
    cout << "Done reading file " << filePath << endl;
};

class JoinThreads {
public:
    explicit JoinThreads(vector<thread>& thrds) :
        threads_(thrds) {}
    ~JoinThreads() {
        for(auto i = 0; i < threads_.size(); ++i) {
            if(threads_[i].joinable()) {
                threads_[i].join();
            }
        }
    }
private:
    vector<thread>& threads_;
};

template<typename T>
class ThreadQueue {
private:
    mutable mutex mutex_;    
    queue<shared_ptr<T>> queue_;
    condition_variable cond_;

public:
    ThreadQueue() {}
    ~ThreadQueue() {}

    //  Disallow copy, assign, and move operations
    ThreadQueue(const ThreadQueue&) = delete;
    ThreadQueue(ThreadQueue&&) = delete;
    ThreadQueue& operator=(const ThreadQueue&) = delete;
    ThreadQueue& operator=(ThreadQueue&&) = delete;

    bool empty() const {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }

    void push(T obj) {
        shared_ptr<T> data(make_shared<T>(move(obj)));
        lock_guard<mutex> lock(mutex_);
        queue_.push(data);
        cond_.notify_one();
    }
   
    void wait_and_pop(T& obj) {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this]{return !queue_.empty();});

        obj = move(*queue_.front());
        queue_.pop();
    } 

    shared_ptr<T> wait_and_pop() {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this]{return !queue_.empty();});
        shared_ptr<T> result = queue_.front();
        queue_.pop();
        return result;
    }

    bool try_pop(T& obj) {
        lock_guard<mutex> lock(mutex_);
        if(queue_.empty()) {
            return false;
        }
        obj = move(*queue_.front());
        queue_.pop();
        return true;
    }

    shared_ptr<T> try_pop() {
        lock_guard<mutex> lock(mutex_);
        if(queue_.empty()) {
            return shared_ptr<T>();
        }
        shared_ptr<T> result(make_shared<T>(move(queue_.front())));
        queue_.pop();
        return result;
    }
};

class ThreadPool {
private:
    atomic_bool running_;
    vector<thread> workers_;
    mutable ThreadQueue<function<void()>> jobs_;
    JoinThreads joiner_;
    bool initialized_;

    mutable mutex mutex_;
    mutable condition_variable_any cond_;
    once_flag once_;
public:
    ThreadPool():
        running_(false),
        joiner_(workers_)
    {
        call_once(once_, &ThreadPool::init, this);
    }

    ~ThreadPool() {
        running_ = false;
    }

    void init() {
        int N = thread::hardware_concurrency();
        try {
            workers_.reserve(N);

            for(int i = 0; i < N; ++i) {
                workers_.push_back(thread(&ThreadPool::run, this));
            }
            initialized_ = true;
            running_ = true;
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
            if(jobs_.try_pop(job))
            {
                //  Start job.
                cout << "Starting job " << endl;
                job();
            }
            else {
                this_thread::yield();
            }
        }
    }

    void stop() {
        unique_lock<mutex> lock(mutex_);
        running_ = false;
        cout << "Thread pool notifying all" << endl;
        cond_.notify_all();

        cout << "Thread pool notified all - joining threads now" << endl;
        int i = 1;
        for(auto& worker: workers_) {
            if(worker.joinable()) {
                worker.join();
                cout << "Thread join completed - i = " << i++ << endl;
            } else {
              cout << "Skipping non-joinable thread on shutdown for i = " << i << endl;
            }
        }
        cout << "Thread pool stopped gracefully" << endl;
    }

    template<typename Func>
    void submit_job(Func f) {
        jobs_.push(function<void()>(f));
        cout << "Job added - new number of jobs: " << jobs_.size() << endl;
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
                pool.submit_job(bind(readFile, entry.path().string()));
            }
            
        } else {
            cout << "Error - exiting" << input << endl;
        }
        cin >> input;
    }
    pool.stop();
    cout << "Exiting..." << endl;
}
