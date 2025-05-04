#pragma once

#include <cstdint>
#include <iostream>
#include <functional>
#include <thread>
#include <vector>
#include <semaphore.h>
#include <atomic>
#include <csignal>
#include <time.h>
#include <memory>
#include <limits>
#include <string>

class Service
{
public:
    uint32_t getPeriod() const { return _period; }
    std::string service_name;

    template<typename T>
    Service(std::string name, T&& doService, uint8_t affinity, uint8_t priority, uint32_t period) :
        _doService(doService)
    {
        service_name = std::move(name);
        _affinity = affinity;
        _priority = priority;
        _period = period;
        _isRunning = true;
        sem_init(&_releaseSem, 0, 0);
        _service = std::jthread(&Service::_provideService, this);
    }

    void stop()
    {
        _isRunning = false;
        sem_post(&_releaseSem);
        _service.request_stop();
        sem_destroy(&_releaseSem);
        logStatistics();
    }

    void release()
    {
        sem_post(&_releaseSem);
    }

private:
    std::function<void(void)> _doService;
    std::jthread _service;
    sem_t _releaseSem;
    std::atomic<bool> _isRunning;

    uint8_t _affinity;
    uint8_t _priority;
    uint32_t _period;

    std::chrono::high_resolution_clock::time_point _lastStartTime;
    double _minExecTime = std::numeric_limits<double>::max();
    double _maxExecTime = 0.0;
    double _totalExecTime = 0.0;
    int _executionCount = 0;

    double _minStartJitter = std::numeric_limits<double>::max();
    double _maxStartJitter = 0.0;

    void _initializeService()
    {
        pthread_t thisThread = pthread_self();

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(_affinity, &cpuset);

        if (pthread_setaffinity_np(thisThread, sizeof(cpu_set_t), &cpuset) != 0) {
            perror("Failed to set CPU affinity");
        }

        sched_param sch_params;
        sch_params.sched_priority = _priority;

        if (pthread_setschedparam(thisThread, SCHED_FIFO, &sch_params) != 0) {
            perror("Failed to set SCHED_FIFO priority");
        }
    }

    void _provideService()
    {
        _initializeService();
        while (_isRunning) {
            sem_wait(&_releaseSem);

            if (_isRunning) {
                auto start = std::chrono::high_resolution_clock::now();

                if (_executionCount > 0) {
                    double actualInterval = std::chrono::duration<double, std::milli>(start - _lastStartTime).count();
                    double expectedInterval = _period;
                    double jitter = std::abs(actualInterval - expectedInterval);
                    _minStartJitter = std::min(_minStartJitter, jitter);
                    _maxStartJitter = std::max(_maxStartJitter, jitter);
                }
                _lastStartTime = start;

                _doService();

                auto end = std::chrono::high_resolution_clock::now();
                double execTime = std::chrono::duration<double, std::milli>(end - start).count();
                _minExecTime = std::min(_minExecTime, execTime);
                _maxExecTime = std::max(_maxExecTime, execTime);
                _totalExecTime += execTime;
                _executionCount++;
            }
        }
    }

    void logStatistics()
    {
        if (_executionCount == 0) return;
        double avgExecTime = _totalExecTime / _executionCount;
        double execJitter = _maxExecTime - _minExecTime;
        double startJitter = _maxStartJitter - _minStartJitter;

        std::cout << "\n[Service] " << service_name << "\n";
        std::cout << "Period: " << _period << " ms\n";
        std::cout << "  Min Exec Time: " << _minExecTime << " ms\n";
        std::cout << "  Max Exec Time: " << _maxExecTime << " ms\n";
        std::cout << "  Avg Exec Time: " << avgExecTime << " ms\n";
        std::cout << "  Exec Jitter  : " << execJitter << " ms\n";
        std::cout << "  Start Jitter : " << startJitter << " ms\n";
    }
};

class Sequencer
{
public:
    template<typename... Args>
    void addService(Args&&... args)
    {
        _services.emplace_back(std::make_unique<Service>(std::forward<Args>(args)...));
    }

    void startServices()
    {
        for (auto& svc : _services) {
            timer_t timerId;
            struct sigevent sev{};
            sev.sigev_notify = SIGEV_THREAD;
            sev.sigev_value.sival_ptr = svc.get();
            sev.sigev_notify_function = [](union sigval val) {
                auto* s = static_cast<Service*>(val.sival_ptr);
                s->release();
            };

            if (timer_create(CLOCK_REALTIME, &sev, &timerId) != 0) {
                perror("timer_create");
                continue;
            }

            struct itimerspec its{};
            int period_ms = svc->getPeriod();
            its.it_value.tv_sec = period_ms / 1000;
            its.it_value.tv_nsec = (period_ms % 1000) * 1'000'000;
            its.it_interval = its.it_value;

            if (timer_settime(timerId, 0, &its, nullptr) != 0) {
                perror("timer_settime");
                timer_delete(timerId);
                continue;
            }

            _timerIds.push_back(timerId);
        }
    }

    void stopServices()
    {
        for (auto& timer : _timerIds) {
            timer_delete(timer);
        }
        _timerIds.clear();

        for (auto& svc : _services) {
            svc->stop();
        }
    }

private:
    std::vector<std::unique_ptr<Service>> _services;
    std::vector<timer_t> _timerIds;
};
