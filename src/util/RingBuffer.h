#pragma once
#include <list>
#include <mutex>
#include <memory>
#include <functional>

template <typename T>
class RingBuffer {
public:
    using Ptr = std::shared_ptr<RingBuffer>;

    RingBuffer(size_t max_size = 512, size_t max_gop = 2)
            : _max_size(max_size), _max_gop_size(max_gop) {
        _gop_cache.emplace_back();
    }

    void write(T data, bool is_key = false) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (is_key) {
            _have_key = true;
            if (!_gop_cache.back().empty()) {
                _gop_cache.emplace_back();
            }
            while (_gop_cache.size() > _max_gop_size) {
                _size -= _gop_cache.front().size();
                _gop_cache.pop_front();
            }
        }

        if (!_have_key) return;

        _gop_cache.back().emplace_back(std::move(data));
        ++_size;

        while (_size > _max_size && _gop_cache.size() > 1) {
            _size -= _gop_cache.front().size();
            _gop_cache.pop_front();
        }

        if (_on_data) _on_data(_gop_cache.back().back());
    }

    void setOnData(std::function<void(const T&)> cb) {
        std::lock_guard<std::mutex> lock(_mutex);
        _on_data = std::move(cb);
        // 回放缓存
        for (const auto& gop : _gop_cache) {
            for (const auto& item : gop) {
                _on_data(item);
            }
        }
    }

    size_t size() const { return _size; }
    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _gop_cache.clear();
        _gop_cache.emplace_back();
        _size = 0;
        _have_key = false;
    }

private:
    std::mutex _mutex;
    size_t _max_size;
    size_t _max_gop_size;
    size_t _size = 0;
    bool _have_key = false;
    std::list<std::list<T>> _gop_cache;
    std::function<void(const T&)> _on_data;
};