#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <cassert>
#include <numeric>
#include <nasral/core/types.h>

namespace nasral::core
{
    template<typename T = uint32_t>
    class IndexPool
    {
    public:
        explicit IndexPool(size_t size){
            indices_.reserve(size);
            indices_.resize(indices_.capacity());
            std::iota(indices_.rbegin(), indices_.rend(), T(0));
        }

        T acquire_unsafe(){
            assert(!indices_.empty() && "Index pool is empty");
            T index = indices_.back();
            indices_.pop_back();
            return index;
        }

        T acquire(){
            std::lock_guard lock(mutex_);
            return acquire_unsafe();
        }

        void release_unsafe(const T index){
            if constexpr (kDebugBuild){
                assert(indices_.size() < indices_.capacity() && "Index pool is full");
                if (std::find(indices_.begin(), indices_.end(), index) != indices_.end()){
                    assert(false && "Double release of index!");
                }
            }
            indices_.emplace_back(index);
        }

        void release(const T index){
            std::lock_guard lock(mutex_);
            release_unsafe(index);
        }


        void reset_unsafe(){
            indices_.clear();
            indices_.resize(indices_.capacity());
            std::iota(indices_.rbegin(), indices_.rend(), T(0));
        }

        void reset()
        {
            std::lock_guard lock(mutex_);
            reset_unsafe();
        }

        [[nodiscard]] size_t size() const noexcept{
            return indices_.size();
        }

        [[nodiscard]] bool empty() const noexcept{
            return indices_.empty();
        }

        [[nodiscard]] bool full() const noexcept{
            return indices_.size() == indices_.capacity();
        }

        [[nodiscard]] size_t capacity() const noexcept{
            return indices_.capacity();
        }

        ~IndexPool() = default;

    protected:
        std::vector<T> indices_;
        std::mutex mutex_;
    };
}
