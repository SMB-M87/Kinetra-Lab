//
// Created by Priory on 3/9/2024.
//

#pragma once

#include <concepts>
#include <cstddef>

namespace dotio {
template <typename T>
concept IsActive = requires(T t) {
    { t.IsActive } -> std::convertible_to<bool>;
};

template <typename T>
/// Fixed-pool view with circular slot reuse; instances must not outlive their owning pool.
struct Tracker {
  private:
    T *instances_{};
    size_t size_{};
    size_t index_{};
    T *instance_;
    explicit Tracker(T *instance) : instance_(instance) {}

  public:
    explicit Tracker(T *instances, const size_t size, const size_t index)
        : instances_(instances), size_(size), index_(index), instance_(&instances_[0]) {}

    T &operator[](const int index) { return get_instances()[index % size_]; }

    void add_instance(T instance)
        requires IsActive<T>
    {
        T &i = instances_[index_++] = instance;
        i.IsActive = true;

        if (index_ == size_)
            index_ = 0;
    }

    T *get_instances() const { return instances_; }

    [[nodiscard]] size_t get_size() const { return size_; }

    T &operator*() { return *instance_; }

    Tracker &operator++() {
        ++instance_;
        return *this;
    }

    bool operator!=(const Tracker &other) const { return instance_ != other.instance_; }

    Tracker begin() { return Tracker(&instances_[0]); }
    Tracker end() { return Tracker(&instances_[size_]); }

    [[nodiscard]] size_t get_capacity() const { return size_; };
};

template <typename T, size_t size> struct TrackerData {
  private:
    size_t capacity_{size};
    T objects_[size]{};
    size_t index_{};
    Tracker<T> tracker_;

  public:
    T &operator[](const int index) { return tracker_[index % size]; }

    void operator+=(T instance) { return tracker_.add_instance(instance); }

    TrackerData() : tracker_(objects_, size, 0) {}

    [[nodiscard]] Tracker<T> &get_tracker() { return tracker_; }

    void add_instance(T instance) { tracker_.AddInstance(instance); };

    [[nodiscard]] T *get_instances() const { return tracker_.GetInstances(); }

    [[nodiscard]] size_t get_capacity() const { return tracker_.GetCapacity(); }
};
} // namespace dotio
