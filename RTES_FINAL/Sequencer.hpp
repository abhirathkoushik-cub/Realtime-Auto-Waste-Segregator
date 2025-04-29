/*
 * This is a C++ version of the canonical pthread service example. It intends
 * to abstract the service management functionality and sequencing for ease
 * of use. Much of the code is left to be implemented by the student.
 *
 * Build with g++ --std=c++23 -Wall -Werror -pedantic
 * Steve Rizor 3/16/2025
 */

 #pragma once

 #include <atomic>
 #include <chrono>
 #include <cstdint>
 #include <functional>
 #include <semaphore>
 #include <thread>
 #include <vector>
 #include <iostream>
 #include <pthread.h>
 #include <chrono>
 
 class Service_Statistic
 {
 public:
     Service_Statistic()
         : _min_execution_time(0xFFFFFFFF),
           _max_execution_time(0),
           _min_start_time(0xFFFFFFFF),
           _max_start_time(0),
           _num_iteration(0),
           _total_execution_time(0)
     {}
 
     void update_statistic(uint32_t exec_time, uint32_t start_time)
     {
         _num_iteration++;
         _total_execution_time += exec_time;
 
         if (exec_time < _min_execution_time) {
             _min_execution_time = exec_time;
         }
 
         if (exec_time > _max_execution_time) {
             _max_execution_time = exec_time;
         }
 
         if (start_time < _min_start_time) {
             _min_start_time = start_time;
         }
 
         if (start_time > _max_start_time) {
             _max_start_time = start_time;
         }
         return;
     }
 
     void get_statistic(uint32_t& min_exec_time, uint32_t& max_exec_time,
                        double& avg_exec_time, uint32_t& jitter_exec_time,
                        uint32_t& jitter_start_time)
     {
         if (_num_iteration) {
             min_exec_time = _min_execution_time;
             max_exec_time = _max_execution_time;
             avg_exec_time = _total_execution_time / _num_iteration;
             jitter_exec_time = _max_execution_time - _min_execution_time;
             jitter_start_time = _max_start_time - _min_start_time;
         } else {
             min_exec_time = 0;
             max_exec_time = 0;
             avg_exec_time = 0;
             jitter_exec_time = 0;
             jitter_start_time = 0;
         }
     }
 
 private:
     uint32_t _min_execution_time;
     uint32_t _max_execution_time;
     uint32_t _min_start_time;
     uint32_t _max_start_time;
     uint32_t _num_iteration;
     double _total_execution_time;
 };
 
 
 // The service class contains the service function and service parameters
 // (priority, affinity, etc). It spawns a thread to run the service, configures
 // the thread as required, and executes the service whenever it gets released.
 
 class Service
 {
 public:
     template<typename T>
     Service(T&& doService, uint8_t affinity, uint8_t priority, uint32_t period,
             Service_Statistic* service_stat, uint32_t service_id)
         : _doService(std::forward<T>(doService)),
           _affinity(affinity),
           _priority(priority),
           _period(period),
           _sem(0),
           _running(true),
           _service_stat(service_stat),
           _service_id(service_id)
     {
         _service = std::jthread(&Service::_provideService, this);
     }
 
     // Prevent copying and moving
     Service(const Service&) = delete;
     Service& operator=(const Service&) = delete;
     Service(Service&&) = delete;
     Service& operator=(Service&&) = delete;
 
     void stop()
     {
         _running = false;
         _sem.release(); // Wake up if waiting
     }
 
     void release()
     {
         _sem.release(); // Trigger the service to run once
     }
 
     uint32_t getPeriod() const
     {
         return _period;
     }
 
     uint32_t getServiceID() const
     {
         return _service_id;
     }
 
 private:
     std::function<void(void)> _doService;
     std::jthread _service;
     uint8_t _affinity;
     uint8_t _priority;
     uint32_t _period;
     std::binary_semaphore _sem;
     std::atomic<bool> _running;
     Service_Statistic* _service_stat;
     uint32_t _service_id;
 
     void _initializeService()
     {
         pthread_t handle = pthread_self();
 
         // Set priority (optional, may require sudo)
         sched_param sch_params;
         sch_params.sched_priority = _priority;
         if (pthread_setschedparam(handle, SCHED_FIFO, &sch_params) != 0) {
             std::cerr << "Warning: Failed to set thread priority.\n";
         }
 
         // Set CPU affinity (optional)
         cpu_set_t cpuset;
         CPU_ZERO(&cpuset);
         CPU_SET(_affinity, &cpuset);
         if (pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset) != 0) {
             std::cerr << "Warning: Failed to set CPU affinity.\n";
         }
     }
 
     void _provideService()
     {
         // start measuring service start time
         auto _t1 = std::chrono::high_resolution_clock::now();
 
         _initializeService();
 
         while (_running) {
             _sem.acquire(); // Wait until released
             if (!_running) break;
 
             auto _t2 = std::chrono::high_resolution_clock::now();
             uint32_t _start_time = std::chrono::duration_cast<std::chrono::microseconds>(_t2 - _t1).count();
 
             // start measuring execution time
             auto _t3 = std::chrono::high_resolution_clock::now();
 
             _doService();   // Run the task
 
             auto _t4 = std::chrono::high_resolution_clock::now();
             uint32_t _execution_time = std::chrono::duration_cast<std::chrono::microseconds>(_t4 - _t3).count();
 
             // update service statistic
             _service_stat->update_statistic(_execution_time, _start_time);
         }
     }
 };
 
 
 bool customLess(const std::unique_ptr<Service>& a, const std::unique_ptr<Service>& b)
 {
     Service* a_ptr = a.get();
     Service* b_ptr = b.get();
     return a_ptr->getPeriod() < b_ptr->getPeriod();
 }
 
 
 // The sequencer class contains the services set and manages
 // starting/stopping the services. While the services are running,
 // the sequencer releases each service at the requisite timepoint.
 
 class Sequencer
 {
 public:
     template<typename... Args>
     void addService(Args&&... args)
     {
         _services.push_back(std::make_unique<Service>(std::forward<Args>(args)...));
     }
 
     void startServices()
     {
         _running = true;
 
         _tickThread = std::jthread([this]() {
             uint64_t tick = 0;
             while (_running) {
                 std::this_thread::sleep_for(std::chrono::milliseconds(1));
                 tick++;
 
                 for (auto& service : _services) {
                     if (tick % service->getPeriod() == 0) {
                         service->release();
                     }
                 }
             }
         });
     }
 
     void stopServices()
     {
         _running = false;
         for (auto& service : _services) {
             service->stop();
         }
     }
 
     void sortServicesbyAscendingPeriod()
     {
         std::sort(_services.begin(), _services.end(), customLess);
     }
 
     void printServices()
     {
         for (size_t i = 0; i < _services.size(); i++) {
             Service* serv_ptr = _services[i].get();
             std::cout << "Service " << serv_ptr->getServiceID()
                       << " period: " << serv_ptr->getPeriod() << std::endl;
         }
     }
 
 private:
     std::vector<std::unique_ptr<Service>> _services;
     std::jthread _tickThread;
     std::atomic<bool> _running{false};
 };
 