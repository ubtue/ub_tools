/*
    Copyright (C) 2016, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef PIPELINE_MONITOR_H
#define PIPELINE_MONITOR_H

#include <iostream>
#include <map>
#include <string>
#include "WallClockTimer.h"

class PipelineMonitorTimer {
private:
    WallClockTimer * const timer_;
public:
    PipelineMonitorTimer(WallClockTimer * const timer) : timer_(timer) { timer_->start(); }
    ~PipelineMonitorTimer() { timer_->stop(); }
};

class PipelineMonitor {
private:
    std::map<const std::string, WallClockTimer *> timers;
    std::map<const std::string, unsigned> counters;

    inline std::string toKey(const std::string domain, const std::string key) {
        return key + "." + domain;
    }

public:
    PipelineMonitor() {}

    virtual ~PipelineMonitor() {
        std::cout << "CCCCCC\n";
        for (auto &entry : counters)
            std::cout << entry.first << "=" << entry.second << "\n";
        std::cout << "TTTTTT\n";
        for (auto &entry : timers) {
            std::cout << entry.first << "=" << entry.second->getTimeInMilliseconds() << "\n";
            delete entry.second;
        }
        std::cout << "EEEEEE\n";
    }

    void setCounter(const std::string domain, const std::string key, unsigned value) {
        counters[toKey(domain, key)] = value;
    }

    PipelineMonitorTimer startTiming(const std::string domain, const std::string key) {
        const std::string timer_key = toKey(domain, key);
        if (timers.find(timer_key) == timers.end())
            timers[timer_key] = new WallClockTimer(WallClockTimer::CUMULATIVE, timer_key);

        return PipelineMonitorTimer(timers[timer_key]);
    }
};


#endif