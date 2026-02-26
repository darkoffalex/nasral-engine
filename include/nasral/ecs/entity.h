#pragma once
#include <cstddef>
#include <array>
#include <vector>
#include <type_traits>
#include <cassert>

namespace nasral::ecs
{
    /**
     * @brief Идентификатор Entity
     */
    struct EntityId
    {
        size_t index;
        size_t version;

        bool operator==(const EntityId& other) const{
            return index == other.index && version == other.version;
        }
    };

    /**
     * @brief Контейнер для идентификаторов Entity
     * @details Может использоваться для ссылок на другие Entity в рамках компонентов
     * @tparam InlineCapacity Размер inline буфера (для оптимизации кещ-локальности)
     */
    template <std::size_t InlineCapacity>
    class EntityIdVector
    {
    public:
        using value_type = EntityId;
        using size_type = std::size_t;

        static_assert(InlineCapacity > 0, "InlineCapacity must be > 0");
        static_assert(std::is_trivially_copyable_v<value_type>, "EntityIdVector expects trivially copyable EntityId");
        static_assert(std::is_trivially_destructible_v<value_type>, "EntityIdVector expects trivially destructible EntityId");

        EntityIdVector() = default;

        [[nodiscard]] size_type size() const noexcept
        {
            return using_heap_ ? heap_.size() : size_;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] value_type* data() noexcept
        {
            return using_heap_ ? heap_.data() : inline_.data();
        }

        [[nodiscard]] const value_type* data() const noexcept
        {
            return using_heap_ ? heap_.data() : inline_.data();
        }

        value_type* begin() noexcept { return data(); }
        value_type* end() noexcept { return data() + size_; }
        [[nodiscard]] const value_type* begin() const noexcept { return data(); }
        [[nodiscard]] const value_type* end() const noexcept { return data() + size_; }

        value_type& operator[](size_type i) noexcept
        {
            assert(i < size_);
            return using_heap_ ? heap_[i] : inline_[i];
        }

        const value_type& operator[](size_type i) const noexcept
        {
            assert(i < size_);
            return using_heap_ ? heap_[i] : inline_[i];
        }

        void clear() noexcept
        {
            if (using_heap_) heap_.clear();
            size_ = 0;
        }

        void reserve(const size_type n)
        {
            if (n <= InlineCapacity && !using_heap_){
                return;
            }

            if (using_heap_)
            {
                heap_.reserve(n);
                return;
            }

            ensure_heap(n);
        }

        value_type& push_back(const value_type& v)
        {
            if (!using_heap_ && size_ < InlineCapacity)
            {
                inline_[size_] = v;
                ++size_;
                return inline_[size_ - 1];
            }

            ensure_heap(size_ + 1);
            heap_.push_back(v);
            ++size_;
            return heap_.back();
        }

        value_type& push_back(value_type&& v)
        {
            if (!using_heap_ && size_ < InlineCapacity)
            {
                inline_[size_] = std::move(v);
                ++size_;
                return inline_[size_ - 1];
            }

            ensure_heap(size_ + 1);
            heap_.push_back(std::forward<value_type>(v));
            ++size_;
            return heap_.back();
        }

        value_type& emplace_back(const size_t index, const size_t version)
        {
            return push_back(value_type{index, version});
        }

        void pop_back() noexcept
        {
            assert(size_ > 0);
            if (using_heap_) heap_.pop_back();
            --size_;
        }

        bool erase_unordered(const value_type& v) noexcept
        {
            for (size_type i = 0; i < size_; ++i)
            {
                if ((*this)[i] == v)
                {
                    (*this)[i] = (*this)[size_ - 1];
                    pop_back();
                    return true;
                }
            }
            return false;
        }

        bool erase_ordered(const value_type& v) noexcept
        {
            for (size_type i = 0; i < size_; ++i)
            {
                if ((*this)[i] == v)
                {
                    for (size_type j = i + 1; j < size_; ++j){
                        (*this)[j - 1] = (*this)[j];
                    }

                    pop_back();
                    return true;
                }
            }
            return false;
        }

    private:
        void ensure_heap(const size_type min_capacity)
        {
            if (using_heap_) return;
            using_heap_ = true;

            heap_.reserve(min_capacity);
            heap_.insert(
                heap_.end(),
                inline_.begin(),
                inline_.begin() + static_cast<std::ptrdiff_t>(size_)
                );
        }

    protected:
        std::array<value_type, InlineCapacity> inline_{};
        std::vector<value_type> heap_{};
        size_type size_ = 0;
        bool using_heap_ = false;
    };
}
