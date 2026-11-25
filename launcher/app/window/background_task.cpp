#include "window/background_task.h"

#include <QThread>

namespace lom::launcher {

namespace {
constexpr int sample_interval_ms = 50;

}

BackgroundTask::BackgroundTask(QObject* parent) : QObject(parent) {
    timer_.setInterval(sample_interval_ms);
    connect(&timer_, &QTimer::timeout, this, &BackgroundTask::sample);
}

BackgroundTask::~BackgroundTask() {
    cancel();

    if (thread_ != nullptr) {
        thread_->disconnect(this);
        thread_->wait();
        delete thread_;
    }
}

bool BackgroundTask::start(Work work) {
    if (running()) {
        return false;
    }

    cancel_ = false;
    stage_ = 0;
    done_ = 0;
    total_ = 0;
    last_done_ = ~std::uint64_t(0);
    last_stage_ = -1;

    thread_ = QThread::create([this, work = std::move(work)]() {
        const Progress progress(
            [this](Progress::Stage stage, std::uint64_t done, std::uint64_t total) {
                stage_.store(int(stage), std::memory_order_relaxed);
                total_.store(total, std::memory_order_relaxed);
                done_.store(done, std::memory_order_relaxed);
            },
            &cancel_);
        work(progress);
    });

    connect(thread_, &QThread::finished, this, [this]() {
        timer_.stop();
        sample();
        thread_->deleteLater();
        thread_ = nullptr;
        emit finished();
    });

    timer_.start();
    thread_->start();
    return true;
}

void BackgroundTask::cancel() {
    cancel_ = true;
}

bool BackgroundTask::running() const {
    return thread_ != nullptr;
}

void BackgroundTask::wait() {
    if (thread_ != nullptr) {
        thread_->wait();
    }
}

void BackgroundTask::sample() {
    const int stage = stage_.load(std::memory_order_relaxed);
    const std::uint64_t done = done_.load(std::memory_order_relaxed);

    if (stage == last_stage_ && done == last_done_) {
        return;
    }

    last_stage_ = stage;
    last_done_ = done;
    emit progressed(stage, done, total_.load(std::memory_order_relaxed));
}

}
