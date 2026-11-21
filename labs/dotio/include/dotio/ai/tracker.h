//
// Created by Priory on 3/9/2024.
//

#pragma once

#include <concepts>
#include <cstddef>

namespace dotio {
/// A pooled object: it has an `is_active` flag that marks a slot in use.
template <typename T>
concept Activatable = requires(T t) {
    { t.is_active } -> std::convertible_to<bool>;
};

template <typename T>
/// A view of a fixed pool with circular slot reuse, and an iterator over all of its slots,
/// active or not. Borrowed: it must not outlive the TrackerData that owns the slots.
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

    /// Slot `index`, wrapped around the pool size.
    T &operator[](const std::size_t index) { return get_instances()[index % size_]; }

    /// Copies `instance` into the next slot in circular order, whether that slot is in use or
    /// not, and activates it.
    void add_instance(T instance)
        requires Activatable<T>
    {
        T &i = instances_[index_++] = instance;
        i.is_active = true;

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

/// Owns a fixed pool of `size` slots and the Tracker that views it. Slot addresses never
/// change, so other objects may keep pointers to them.
template <typename T, size_t size> struct TrackerData {
  private:
    size_t capacity_{size};
    T objects_[size]{};
    size_t index_{};
    Tracker<T> tracker_;

  public:
    T &operator[](const std::size_t index) { return tracker_[index % size]; }

    void operator+=(T instance) { return tracker_.add_instance(instance); }

    TrackerData() : tracker_(objects_, size, 0) {}

    [[nodiscard]] Tracker<T> &get_tracker() { return tracker_; }

    void add_instance(T instance) { tracker_.AddInstance(instance); };

    [[nodiscard]] T *get_instances() const { return tracker_.GetInstances(); }

    [[nodiscard]] size_t get_capacity() const { return tracker_.GetCapacity(); }
};
} // namespace dotio
