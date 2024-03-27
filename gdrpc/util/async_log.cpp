#include "async_log.hpp"

#include <assert.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

#include "absl/memory/memory.h"
#include "time.hpp"

namespace gdrpc {
namespace gdlog {
AsyncLogging::AsyncLogging(const std::string& basename, off_t rollSize,
                           int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      file_(std::filesystem::path(basename).filename().string(), rollSize),
      thread_(std::bind(&AsyncLogging::threadFunc, this)),
      latch_(0),
      currentBuffer_(std::make_unique<Buffer>()),
      nextBuffer_(std::make_unique<Buffer>()) {
  buffers_.reserve(16);
  latch_.release();
}

void AsyncLogging::append(const char* logline, int len) {
  std::lock_guard lock(mutex_);
  if (currentBuffer_->avail() > len) {
    currentBuffer_->append(logline, len);
  } else {
    buffers_.push_back(std::move(currentBuffer_));

    if (nextBuffer_) {
      currentBuffer_ = std::move(nextBuffer_);
    } else {
      currentBuffer_ = std::make_unique<Buffer>();  // Rarely happens
    }
    currentBuffer_->append(logline, len);
    cond_.notify_one();
  }
}
void AsyncLogging::threadFunc() {
  latch_.acquire();  // 在初始化完前先阻塞
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  running_ = true;
  while (running_) {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      std::unique_lock lock(mutex_);
      if (buffers_.empty())  // unusual usage!
      {
        cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
      }
      buffers_.push_back(std::move(currentBuffer_));
      currentBuffer_ = std::move(newBuffer1);
      buffersToWrite.swap(buffers_);
      if (!nextBuffer_) {
        nextBuffer_ = std::move(newBuffer2);
      }
    }
    //死锁了？
    assert(!buffersToWrite.empty());
    // 日志异常过多，忽略
    if (buffersToWrite.size() > 25) {
      char buf[256];
      snprintf(buf, sizeof buf,
               "Dropped log messages at %s, %zd larger buffers\n",
               util::get_time().c_str(), buffersToWrite.size() - 2);
      fputs(buf, stderr);
      file_.append(buf, static_cast<int>(strlen(buf)));
      buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
    }

    for (const auto& buffer : buffersToWrite) {
      // use ::writev 可以减少系统调用，但就难以控制滚动日志的大小
      file_.append(buffer->data(), buffer->length());
    }

    // 留给两个newbuffer做缓冲
    // buffersToWrite.resize(2);
    if (!newBuffer1) {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer1->reset();
    }

    if (!newBuffer2) {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();
    file_.flush();
  }
  file_.flush();
}

AsyncLogging::LogFile::LogFile(const std::string& basename, off_t rollSize,
                               int flushInterval, int checkEveryN)
    : basename_(basename),
      rollSize_(rollSize),
      flushInterval_(flushInterval),
      checkEveryN_(checkEveryN),
      count_(0),
      lastRoll_(0),
      lastFlush_(0),
      cur_len_(0) {
  assert(basename.find('/') == std::string::npos);//只要程序名
  time_t now = ::time(nullptr);
  std::string filename = getLogFileName(basename_, &now);
  lastRoll_ = now;
  lastFlush_ = now;
  file_.open(filename.c_str());
}

void AsyncLogging::LogFile::append(const char* logline, int len) {
  file_.write(logline, len);
  cur_len_ += len;

  if (cur_len_ > rollSize_) {
    rollFile();
    cur_len_ = 0;
  } else {
    ++count_;
    if (count_ >= checkEveryN_) {
      count_ = 0;
      time_t now = ::time(nullptr);
      if (now - lastFlush_ > flushInterval_) {
        lastFlush_ = now;
        file_.flush();
      }
    }
  }
}

bool AsyncLogging::LogFile::rollFile() {
  // 按时间给个新名字，不滚动是因为时间没过一秒
  time_t now = ::time(nullptr);
  if (now > lastRoll_) {
    std::string filename = getLogFileName(basename_, &now);
    lastRoll_ = now;
    lastFlush_ = now;
    file_.close();
    file_.open(filename.c_str());
    return true;
  }
  return false;
}

std::string AsyncLogging::LogFile::getLogFileName(std::string_view basename,
                                                  time_t* now) {
  std::string filename;
  filename.reserve(basename.size() + 64);
  filename = basename;

  char timebuf[32];
  struct tm tm;
  gmtime_r(now, &tm);  // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
  filename += timebuf;

  char buf[256];
  if (::gethostname(buf, sizeof buf) == 0) {
    buf[sizeof(buf) - 1] = '\0';
  } else {
    memcpy(buf, "unknownhost", 13);
  }
  filename += buf;

  char pidbuf[32];
  snprintf(pidbuf, sizeof pidbuf, ".%d", getpid());
  filename += pidbuf;

  filename += ".log";

  return filename;
}

}  // namespace gdlog
}  // namespace gdrpc