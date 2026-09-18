// ============================================================================
// neko/vector.hpp
//
// Tests: tests/test_vector.cpp
// ============================================================================
#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>

#include "neko/config.hpp"
#include "neko/type_traits.hpp"
#include "neko/utility.hpp"

namespace neko {

template <typename T>
class vector {
public:
    // The container contract. Generic code names these -- `typename
    // C::value_type` is the only way to ask a container what it holds. The code
    // below uses the concrete types directly; these exist to be read from the
    // outside.
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    // --- construction / destruction ----------------------------------------
    vector() = default;

    explicit vector(std::size_t count) {
        init(count);
        try {
            for (; end_ != cap_; ++end_) ::new (static_cast<void*>(end_)) T();
        } catch (...) {
            tidy();
            throw;
        }
    }
    vector(std::size_t count, const T& value) {
        init(count);
        try {
            for (; end_ != cap_; ++end_)
                ::new (static_cast<void*>(end_)) T(value);
        } catch (...) {
            tidy();
            throw;
        }
    }

    vector(const vector& other) {
        init(other.size());
        try {
            for (; end_ < cap_; ++end_) {
                ::new (static_cast<void*>(end_))
                    T(*(other.begin() + (end_ - begin_)));
            }
        } catch (...) {
            tidy();
            throw;
        }
    }
    vector(vector&& other) noexcept {
        begin_ = neko::exchange(other.begin_, nullptr);
        end_ = neko::exchange(other.end_, nullptr);
        cap_ = neko::exchange(other.cap_, nullptr);
    }

    vector& operator=(const vector& other) {
        if (this != &other) {
            vector tmp(other);
            swap(tmp);
        }
        return *this;
    }
    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            vector tmp(neko::move(other));
            swap(tmp);
        }
        return *this;
    }

    ~vector() { tidy(); }

    // --- capacity -----------------------------------------------------------
    std::size_t size() const noexcept { return end_ - begin_; }
    std::size_t capacity() const noexcept { return cap_ - begin_; }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    void reserve(std::size_t new_cap) {
        if (new_cap <= capacity()) return;
        reallocate(new_cap);
    }
    void shrink_to_fit() { reallocate(size()); }

    // --- element access -----------------------------------------------------
    T& operator[](std::size_t i) { return *(begin_ + i); }
    const T& operator[](std::size_t i) const { return *(begin_ + i); }
    T& at(std::size_t i) {
        if (i >= size()) throw std::out_of_range("neko::vector::at");
        return *(begin_ + i);
    }
    const T& at(std::size_t i) const {
        if (i >= size()) throw std::out_of_range("neko::vector::at");
        return *(begin_ + i);
    }

    T& front() { return *begin_; }
    const T& front() const { return *begin_; }
    T& back() { return *(end_ - 1); }
    const T& back() const { return *(end_ - 1); }

    T* data() noexcept { return begin_; }
    const T* data() const noexcept { return begin_; }

    // --- iterators ----------------------------------------------------------
    T* begin() noexcept { return begin_; }
    const T* begin() const noexcept { return begin_; }
    T* end() noexcept { return end_; }
    const T* end() const noexcept { return end_; }

    // --- modifiers ----------------------------------------------------------
    void push_back(const T& value) {
        if (end_ == cap_) grow();
        ::new (static_cast<void*>(end_)) T(value);
        end_++;
    }
    void push_back(T&& value) {
        if (end_ == cap_) grow();
        ::new (static_cast<void*>(end_)) T(neko::move(value));
        end_++;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (end_ == cap_) grow();
        ::new (static_cast<void*>(end_)) T(neko::forward<Args>(args)...);
        end_++;
        return *(end_ - 1);
    }

    void pop_back() {
        end_--;
        end_->~T();
    }
    void clear() {
        destroyRange(begin_, end_);
        end_ = begin_;
    }
    void resize(std::size_t count) {
        if (count == size()) return;
        while (count < size()) pop_back();
        if (count > size()) {
            if (count > capacity()) reallocate(count);
            T* p = end_;
            try {
                for (; p != begin_ + count; ++p) new (p) T();
            } catch (...) {
                while (p != end_) (--p)->~T();
                throw;
            }
            end_ = p;
        }
    }
    void resize(std::size_t count, const T& value) {
        if (count == size()) return;
        while (count < size()) pop_back();
        if (count > size()) {
            if (count > capacity()) reallocate(count);
            T* p = end_;
            try {
                for (; p != begin_ + count; ++p) new (p) T(value);
            } catch (...) {
                while (p != end_) (--p)->~T();
                throw;
            }
            end_ = p;
        }
    }

    T* insert(const T* pos, const T& value) {
        std::size_t i = pos - begin_;
        if (end_ == cap_) grow();

        ::new (static_cast<void*>(end_)) T(value);
        end_++;

        T* pos_ = begin_ + i;
        T* it = end_ - 1;
        while (pos_ != it) {
            std::swap(*(it - 1), *it);
            it--;
        }
        return pos_;
    }
    T* erase(const T* pos) {
        std::size_t i = pos - begin_;

        T* it = begin_ + i;
        while (it + 1 != end_) {
            std::swap(*it, *(it + 1));
            it++;
        }

        pop_back();
        return begin_ + i;
    }
    T* erase(const T* first, const T* last) {
        std::size_t f = first - begin_, l = last - begin_;
        if (first == last) return begin_ + f;

        T* it = begin_ + f;
        while (it + l - f != end_) {
            std::swap(*it, *(it + l - f));
            it++;
        }

        for (; f < l; --l) pop_back();
        return begin_ + f;
    }

    void swap(vector& other) {
        neko::swap(begin_, other.begin_);
        neko::swap(end_, other.end_);
        neko::swap(cap_, other.cap_);
    }

private:
    T* begin_ = nullptr;
    T* end_ = nullptr;
    T* cap_ = nullptr;

    static T* allocate(std::size_t n) {
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    static void deallocate(T* p) { ::operator delete(p); }

    void init(std::size_t count) {
        begin_ = allocate(count);
        end_ = begin_;
        cap_ = begin_ + count;
    }

    void reallocate(std::size_t new_cap) {
        T* new_begin_ = allocate(new_cap);
        T* new_end_ = new_begin_;
        T* new_cap_ = new_begin_ + new_cap;
        try {
            for (; std::size_t(new_end_ - new_begin_) < size(); ++new_end_) {
                new (new_end_) T(neko::move_if_noexcept(
                    *(begin_ + (new_end_ - new_begin_))));
            }
        } catch (...) {
            T* it = new_end_;
            while (it != new_begin_) {
                it--;
                it->~T();
            }
            deallocate(new_begin_);
            throw;
        }
        tidy();
        begin_ = neko::exchange(new_begin_, nullptr);
        end_ = neko::exchange(new_end_, nullptr);
        cap_ = neko::exchange(new_cap_, nullptr);
    }

    void destroyRange(const T* first, const T* last) {
        std::size_t f = first - begin_, l = last - begin_;
        for (T* it = begin_ + f; it != begin_ + l; it++) {
            it->~T();
        }
    }

    void tidy() {
        clear();
        deallocate(begin_);
    }

    void grow() {
        std::size_t new_cap = cap_ == nullptr ? 1 : capacity() * 2;
        reallocate(new_cap);
    }
};

template <typename T>
bool operator==(const vector<T>& a, const vector<T>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (*(a.begin() + i) != *(b.begin() + i)) return false;
    }
    return true;
}

}  // namespace neko
