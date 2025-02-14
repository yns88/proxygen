/*
 *  Copyright (c) 2019-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "proxygen/lib/statistics/ResourceStats.h"

#include <glog/logging.h>

namespace {
// tries to acquire a read lock on a given mutex
// consider contributing it to folly
class ReadTryGuard {
  public:
    explicit ReadTryGuard(
        folly::SharedMutex& lock)
      : lock_(&lock) {
      bool locked = lock_->try_lock();
      if (!locked) {
        lock_ = nullptr;
      }
    }
    ReadTryGuard(const ReadTryGuard&) = delete;
    ReadTryGuard& operator=(const ReadTryGuard&) = delete;

    explicit operator bool() const {
      return lock_ != nullptr;
    }
    void release() {
      if (lock_) {
        lock_->unlock();
        lock_ = nullptr;
      }
    }
    ~ReadTryGuard() {
      release();
    }
  private:
    folly::SharedMutex* lock_ = nullptr;
};
}

namespace proxygen {

ResourceStats::ResourceStats(std::unique_ptr<Resources> resources)
    : resources_(std::move(resources)), data_(resources_->getCurrentData()) {
}

ResourceStats::~ResourceStats() {
  stopRefresh();
}

void ResourceStats::refreshWithPeriod(std::chrono::milliseconds periodMs) {
  CHECK_GE(periodMs.count(), 0);
  std::lock_guard<std::mutex> guard(schedulerMutex_);
  refreshPeriodMs_ = periodMs;
  if (!scheduler_) {
    scheduler_.reset(new folly::FunctionScheduler());
    scheduler_->setThreadName("sys_stats");
    // Steady here implies that scheduling will be fixed as opposed to
    // offsetting from the current time which is desired to ensure minimal
    // use of synchronization for getCurrentLoadData()
    scheduler_->setSteady(true);

    std::function<void()> updateFunc(
        std::bind(&ResourceStats::updateCachedData, this));
    std::function<std::chrono::milliseconds()> intervalFunc(
        std::bind(&ResourceStats::getRefreshIntervalMs, this));

    scheduler_->addFunctionGenericDistribution(updateFunc,
                                               intervalFunc,
                                               "sys_stats",
                                               "sys_stats_interval",
                                               std::chrono::milliseconds(0));

    scheduler_->start();
  }
}

void ResourceStats::stopRefresh() {
  std::lock_guard<std::mutex> guard(schedulerMutex_);
  scheduler_.reset();
}

const ResourceData& ResourceStats::getCurrentLoadData() const {
  thread_local ResourceData tlData;
  // Note - we're checking data last update time without lock.
  // We are ok with scarifying some accuracy, not detecting
  // the data was updated, for performance.
  // See D15915152
  if (tlData.getLastUpdateTime() != data_.getLastUpdateTime()) {
      std::chrono::milliseconds currentTime = ResourceData::getEpochTime();
      if (tlData.getLastUpdateTime() == std::chrono::milliseconds(0) ||
          tlData.getLastUpdateTime() +
                  std::chrono::milliseconds(tlData.getUpdateInterval()) <=
              currentTime) {
        ReadTryGuard g(dataMutex_);
        if (g) {
          // Should be fine using the default assignment operator the compiler
          // gave us I think...this will stop being true if data starts storing
          // pointers.
          tlData = data_;
        }
    }
  }
  return tlData;
}

void ResourceStats::updateCachedData() {
  auto data = resources_->getCurrentData();

  data.setUpdateInterval(refreshPeriodMs_);
  {
    folly::SharedMutex::WriteHolder g(dataMutex_);

    // Reset the last update time in case there was a delay acquiring the lock.
    // Not explicitly necessary as the function scheduler is set to use a
    // steady clock but we want to make sure we never get in a situation
    // where lock contention burns us.
    data.refreshLastUpdateTime();

    data_ = data;
  }
}

} // namespace proxygen
