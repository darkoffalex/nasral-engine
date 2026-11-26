#pragma once
#include <nasral/common/types.h>

namespace nasral
{
    class ProgressTracker
    {
    public:
        ProgressTracker()
            : finished_(false)
            , total_(0)
            , completed_(0)
            , on_finished_(nullptr)
        {}

        explicit ProgressTracker(const uint32_t total)
            : finished_(false)
            , total_(total)
            , completed_(0)
            , on_finished_(nullptr)
        {}

        void reset(){
            finished_.store(false, std::memory_order_release);
            completed_.store(0, std::memory_order_release);
        }

        void set_total(const uint32_t total, const bool do_reset = false){
            total_.store(total, std::memory_order_release);
            if (do_reset) reset();
        }

        void inc_completed(const uint32_t amount){
            completed_.fetch_add(amount, std::memory_order_acq_rel);
            if (finished_.load(std::memory_order_acquire)) return;
            if (completed_.load(std::memory_order_acquire) >= total_.load(std::memory_order_acquire)){
                finished_.store(true, std::memory_order_release);
                if (on_finished_){
                    on_finished_();
                }
            }
        }

        void set_on_finished(std::function<void()>&& on_finished){
            on_finished_ = std::move(on_finished);
        }

        [[nodiscard]] bool is_finished() const{
            return finished_.load(std::memory_order_acquire);
        }

        [[nodiscard]] float get_progress() const{
            const auto done = static_cast<float>(completed_.load(std::memory_order_acquire));
            const auto total = static_cast<float>(total_.load(std::memory_order_acquire));
            if (done == 0 && total == 0){
                return 1.0f;
            }
            return done / std::max(total, 1.0f);
        }

    private:
        std::atomic_bool finished_;
        std::atomic_uint32_t total_;
        std::atomic_uint32_t completed_;
        std::function<void()> on_finished_;
    };
}