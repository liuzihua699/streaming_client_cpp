#pragma once
#include <memory>
#include <list>
#include <functional>
#include <mutex>

template <typename T>
class RingBuffer {
public:
    using Ptr = std::shared_ptr<RingBuffer>;

    RingBuffer(size_t max_size = 256, size_t max_gop = 2)
        : _max_size(max_size), _max_gop_size(max_gop) {}

    void write(const T& data, bool is_key = false) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (is_key) {
            _have_key = true;
            _gop_cache.emplace_back();

            while (_gop_cache.size() > _max_gop_size) {
                _size -= _gop_cache.front().size();
                _gop_cache.pop_front();
            }
        }

        if (_have_key && !_gop_cache.empty()) {
            _gop_cache.back().push_back(data);
            _size++;

            while (_size > _max_size && _gop_cache.size() > 1) {
                _size -= _gop_cache.front().size();
                _gop_cache.pop_front();
            }
        }

        if (_on_data) _on_data(data);
    }

    void setOnData(std::function<void(const T&)> cb) {
        std::lock_guard<std::mutex> lock(_mutex);
        _on_data = cb;

        // 回放缓存
        for (auto& gop : _gop_cache) {
            for (auto& pkt : gop) {
                _on_data(pkt);
            }
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _size;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _gop_cache.clear();
        _size = 0;
        _have_key = false;
    }

private:
    mutable std::mutex _mutex;
    size_t _max_size;
    size_t _max_gop_size;
    size_t _size = 0;
    bool _have_key = false;
    std::list<std::list<T>> _gop_cache;
    std::function<void(const T&)> _on_data;
};
