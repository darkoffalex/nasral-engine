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
     * @details Может также использоваться как часть (поле) компонента (для ссылок на другие Entity)
     */
    struct EntityId
    {
        size_t index = 0;
        size_t version = 0;

        static constexpr EntityId invalid() {
            return {
                static_cast<size_t>(-1),
                static_cast<size_t>(-1)
            };
        }

        bool operator==(const EntityId& other) const{
            return index == other.index && version == other.version;
        }
    };

    /**
     * @brief Контейнер для идентификаторов Entity
     * @tparam N Объем контейнера, выделяемого на стеке (для оптимизации кеш-локальности)
     * @details Может также использоваться как часть (поле) компонента (для ссылок на другие Entity)
     */
    template<std::size_t N>
    class EntityIds
    {
    public:
        using value_type = EntityId;
        using size_type = std::size_t;

        static_assert(N > 0, "Inline capacity must be greater than zero");
        static_assert(std::is_trivially_copyable_v<value_type>, "EntityId must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<value_type>, "EntityId must be trivially destructible");

        EntityIds() = default;

        [[nodiscard]] size_type size() const noexcept{
            return is_heap_ ? heap_.size() : size_;
        }

        [[nodiscard]] bool empty() const noexcept{
            return size() == 0;
        }

        [[nodiscard]] value_type* data() noexcept{
            return is_heap_ ? heap_.data() : stack_.data();
        }

        [[nodiscard]] const value_type* data() const noexcept{
            return is_heap_ ? heap_.data() : stack_.data();
        }

        [[nodiscard]] value_type* begin() noexcept{
            return data();
        }

        [[nodiscard]] const value_type* begin() const noexcept{
            return data();
        }

        [[nodiscard]] value_type* end() noexcept{
            return data() + size_;
        }

        [[nodiscard]] const value_type* end() const noexcept{
            return data() + size_;
        }

        value_type& operator[](size_type i) noexcept{
            assert(i < size_);
            return is_heap_ ? heap_[i] : stack_[i];
        }

        const value_type& operator[](size_type i) const noexcept{
            assert(i < size_);
            return is_heap_ ? heap_[i] : stack_[i];
        }

        void clear() noexcept{
            if (is_heap_) heap_.clear();
            size_ = 0;
        }

        void reserve(const size_type n){
            if (n <= N && !is_heap_){
                return;
            }

            if (is_heap_){
                heap_.reserve(n);
                return;
            }

            ensure_heap(n);
        }

        value_type& push_back(const value_type& v){
            if (!is_heap_ && size_ < N){
                stack_[size_] = v;
                ++size_;
                return stack_[size_ - 1];
            }

            ensure_heap(size_ + 1);
            heap_.push_back(v);
            ++size_;
            return heap_.back();
        }

        value_type& push_back(value_type&& v){
            if (!is_heap_ && size_ < N)
            {
                stack_[size_] = std::move(v);
                ++size_;
                return stack_[size_ - 1];
            }

            ensure_heap(size_ + 1);
            heap_.push_back(std::forward<value_type>(v));
            ++size_;
            return heap_.back();
        }

        value_type& emplace_back(const size_t index, const size_t version){
            return push_back(value_type{index, version});
        }

        void pop_back() noexcept{
            assert(size_ > 0);
            if (is_heap_) heap_.pop_back();
            --size_;
        }

        /**
         * @brief Удаление по значению через swap & pop (быстро, но без сохранения порядка)
         * @param v Значение
         * @return Статус операции
         */
        bool erase_unordered(const value_type& v) noexcept{
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

        /**
         * @brief Удаление по значению, с сохранением порядка (медленно)
         * @param v Значение
         * @return Статус операции
         */
        bool erase_ordered(const value_type& v) noexcept{
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
        void ensure_heap(const size_type min_capacity){
            if (is_heap_) return;
            is_heap_ = true;

            heap_.reserve(min_capacity);
            heap_.insert(
                heap_.end(),
                stack_.begin(),
                stack_.begin() + static_cast<std::ptrdiff_t>(size_));
        }

    protected:
        std::array<value_type, N> stack_{};
        std::vector<value_type> heap_{};
        size_type size_ = 0;
        bool is_heap_ = false;
    };
}