#!/bin/bash

g++ -std=c++17 threads.cpp -lpthread -lboost_system -lboost_filesystem -o threads
