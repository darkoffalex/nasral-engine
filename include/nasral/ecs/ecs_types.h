#pragma once
#include <memory>
#include <vector>
#include <atomic>
#include <nasral/core_types.h>

#define MAX_UNIQUE_COMPONENTS 32
#define MAX_ENTITIES 1000

namespace nasral::ecs
{
    struct ComponentBase
    {
        bool is_used = false;
    };

    struct ComponentPoolBase
    {
        typedef std::unique_ptr<ComponentPoolBase> Ptr;
    };

    template<typename Component>
    class ComponentPool : public ComponentPoolBase
    {
    public:
        typedef std::unique_ptr<ComponentPool> Ptr;
        explicit ComponentPool(const size_t pool_size): data_(pool_size) {}
        ~ComponentPool() = default;
        ComponentPool(const ComponentPool&) = delete;
        ComponentPool& operator=(const ComponentPool&) = delete;

        [[nodiscard]] Component* data() { return data_.data(); }
        [[nodiscard]] Component* component(const size_t index){return data_.data() + index;}
        [[nodiscard]] size_t pool_size() const { return data_.size(); }
        [[nodiscard]] size_t size() const { return sizeof(Component) * data_.size(); }

    protected:
        std::vector<Component> data_;
    };

    struct AtomicComponentMask
    {
        std::atomic_uint32_t mask{0};
        void set(const uint32_t index, const bool value){
            if (value) mask.fetch_or(1 << index);
            else mask.fetch_and(~(1 << index));
        }
        void reset(){
            mask.store(0, std::memory_order_release);
        }
        [[nodiscard]] bool get(const uint32_t index) const{
            return (mask.load(std::memory_order_acquire) & (1 << index)) != 0;
        }
    };

    struct EntityId
    {
        size_t index = 0;
        size_t version = 0;
    };

    struct EntitySlot
    {
        EntityId id = {};
        AtomicComponentMask used_components = {};
        std::atomic<bool> deleted{false};
    };

    class ECSError final : public EngineError
    {
    public:
        explicit ECSError(const std::string& message)
        : EngineError(message) {}
    };
}