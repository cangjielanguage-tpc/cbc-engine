#pragma once

#include "utils/vector.h"
#include <assert.h> // assert
#include <stddef.h> // size_t
#include <stdlib.h> // malloc, realloc, free, abort
#include <utility>

namespace Utils {

// template <typename T>
// using Vector = std::vector<T>;

template <typename T> class Vector {
public:
    using iterator       = T*;
    using const_iterator = const T*;
    using value_type     = T;
    using size_type      = size_t;

private:
    T* data_         = nullptr;
    size_t size_     = 0;
    size_t capacity_ = 0;

public:
    // --- Constructors & Destructor ---
    Vector() = default;

    ~Vector()
    {
        clear();
        ::free(data_);
    }

    explicit Vector(size_t size) : Vector() { reserve(size); }

    Vector(Vector const& other) : Vector(other.size())
    {
        for (auto const& v : other) {
            push_back(v);
        }
    }

    // Move Constructor
    Vector(Vector&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_)
    {
        other.data_     = nullptr;
        other.size_     = 0;
        other.capacity_ = 0;
    }

    // Move Assignment
    Vector& operator=(Vector&& other) noexcept
    {
        if (this != &other) {
            clear();
            ::free(data_);
            data_           = other.data_;
            size_           = other.size_;
            capacity_       = other.capacity_;
            other.data_     = nullptr;
            other.size_     = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    Vector& operator=(Vector const& other)
    {
        clear();
        reserve(other.size());
        for (auto const& v : other) {
            push_back(v);
        }
        return *this;
    }

    // --- Iterators ---
    iterator begin() noexcept { return data_; }

    iterator end() noexcept { return data_ + size_; }

    const_iterator begin() const noexcept { return data_; }

    const_iterator end() const noexcept { return data_ + size_; }

    const_iterator cbegin() const noexcept { return data_; }

    const_iterator cend() const noexcept { return data_ + size_; }

    // --- Element Access ---
    T& operator[](size_t index) noexcept { return data_[index]; }

    const T& operator[](size_t index) const noexcept { return data_[index]; }

    // No-exception bounds-checked access
    T& at(size_t index)
    {
        if (index >= size_) {
            assert(false && "Vector::at index out of bounds");
            ::abort(); // Immediate termination instead of std::out_of_range exception
        }
        return data_[index];
    }

    const T& at(size_t index) const
    {
        if (index >= size_) {
            assert(false && "Vector::at index out of bounds");
            ::abort();
        }
        return data_[index];
    }

    T* data() noexcept { return data_; }

    const T* data() const noexcept { return data_; }

    // --- Capacity ---
    size_t size() const noexcept { return size_; }

    size_t capacity() const noexcept { return capacity_; }

    bool empty() const noexcept { return size_ == 0; }

    void resize(size_t count)
    {
        if (count < size_) {
            // Shrinking: Destroy excess elements
            if constexpr (!__is_trivially_destructible(T)) {
                for (size_t i = count; i < size_; ++i) {
                    data_[i].~T();
                }
            }
            size_ = count;
        } else if (count > size_) {
            // Growing: Ensure capacity and value-initialize new elements
            if (count > capacity_) {
                reserve(count);
            }
            for (size_t i = size_; i < count; ++i) {
                new (data_ + i) T(); // Placement new default constructor
            }
            size_ = count;
        }
    }

    void resize(size_t count, const T& value)
    {
        if (count < size_) {
            if constexpr (!__is_trivially_destructible(T)) {
                for (size_t i = count; i < size_; ++i) {
                    data_[i].~T();
                }
            }
            size_ = count;
        } else if (count > size_) {
            if (count > capacity_) {
                reserve(count);
            }
            for (size_t i = size_; i < count; ++i) {
                new (data_ + i) T(value); // Placement new copy constructor
            }
            size_ = count;
        }
    }

    void reserve(size_t new_cap)
    {
        if (new_cap <= capacity_)
            return;

        // Use realloc for POD/trivially copyable types (No loops generated)
        if constexpr (__is_trivially_copyable(T)) {
            T* new_data = static_cast<T*>(::realloc(data_, new_cap * sizeof(T)));
            if (!new_data)
                ::abort();
            data_ = new_data;
        } else {
            T* new_data = static_cast<T*>(::malloc(new_cap * sizeof(T)));
            if (!new_data)
                ::abort();

            for (size_t i = 0; i < size_; ++i) {
                new (new_data + i) T(std::move(data_[i]));
                data_[i].~T();
            }
            ::free(data_);
            data_ = new_data;
        }
        capacity_ = new_cap;
    }

    // --- Modifiers ---
    void push_back(const T& val)
    {
        if (size_ == capacity_)
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        new (data_ + size_) T(val);
        ++size_;
    }

    void push_back(T&& val)
    {
        if (size_ == capacity_)
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        new (data_ + size_) T(std::move(val));
        ++size_;
    }

    template <typename... Args> T& emplace_back(Args&&... args)
    {
        if (size_ == capacity_)
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        T* ptr = new (data_ + size_) T(std::forward<Args>(args)...);
        ++size_;
        return *ptr;
    }

    void pop_back()
    {
        if (size_ > 0) {
            --size_;
            if constexpr (!__is_trivially_destructible(T)) {
                data_[size_].~T();
            }
        }
    }

    void clear() noexcept
    {
        if constexpr (!__is_trivially_destructible(T)) {
            for (size_t i = 0; i < size_; ++i) {
                data_[i].~T();
            }
        }
        size_ = 0;
    }

    T& back() noexcept
    {
        assert(size_ > 0 && "Utils::Vector::back called on empty vector");
        return data_[size_ - 1];
    }

    const T& back() const noexcept
    {
        assert(size_ > 0 && "Utils::Vector::back called on empty vector");
        return data_[size_ - 1];
    }

    T& front() noexcept
    {
        assert(size_ > 0 && "Utils::Vector::front called on empty vector");
        return data_[0];
    }

    const T& front() const noexcept
    {
        assert(size_ > 0 && "Utils::Vector::front called on empty vector");
        return data_[0];
    }
};

} // namespace Utils
