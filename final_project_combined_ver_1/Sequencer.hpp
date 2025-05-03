#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdint>
#include <functional>

class Service_Statistic {
public:
    uint32_t execCount{0};
    uint32_t missedDeadlines{0};
};

class Service {
public:
    template<typename T>
    Service(T&& func, uint8_t priority, uint8_t criticality, uint32_t period_ms, Service_Statistic* stats, uint32_t service_id)
        : _func(std::forward<T>(func)), _priority(priority), _criticality(criticality), _period(std::chrono::milliseconds(period_ms)), _stats(stats), _service_id(service_id)
    {
        _service = std::thread(&Service::_provideService, this);
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _running = false;
            _ready = true;
        }
        _cv.notify_one();
        if (_service.joinable()) {
            _service.join();
        }
    }

    void release()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _ready = true;
        }
        _cv.notify_one();
    }

    uint8_t priority() const { return _priority; }
    uint8_t criticality() const { return _criticality; }

private:
    void _provideService()
    {
        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this]() { return _ready || !_running; });
                if (!_running) break;
                _ready = false;
            }
            auto start = std::chrono::steady_clock::now();
            _func();
            auto end = std::chrono::steady_clock::now();

            if (_stats)
            {
                _stats->execCount++;
                if (end - start > _period)
                    _stats->missedDeadlines++;
            }
        }
    }

    std::function<void()> _func;
    uint8_t _priority;
    uint8_t _criticality;
    std::chrono::milliseconds _period;
    Service_Statistic* _stats;
    uint32_t _service_id;

    std::thread _service;
    std::mutex _mutex;
    std::condition_variable _cv;
    bool _ready{false};
    bool _running{true};
};

class Sequencer {
public:
    void addService(std::function<void()> func, uint8_t priority, uint8_t criticality, uint32_t period_ms, Service_Statistic* stats, uint32_t service_id)
    {
        _services.emplace_back(std::make_unique<Service>(std::move(func), priority, criticality, period_ms, stats, service_id));
    }

    void startServices()
    {
        _running = true;
        _tickThread = std::thread([this]() {
            while (_running)
            {
                for (auto& service : _services)
                {
                    service->release();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20)); // tick every 20ms
            }
        });
    }

    void stopServices()
    {
        _running = false;
        if (_tickThread.joinable()) {
            _tickThread.join();
        }
        for (auto& service : _services)
        {
            service->stop();
        }
    }

private:
    std::vector<std::unique_ptr<Service>> _services;
    std::thread _tickThread;
    std::atomic<bool> _running{false};
};