#pragma once
#include <memory>
#include <string>
#include <cstring>

namespace toolkit {

// Buffer接口（参考ZLToolKit）
class Buffer {
public:
    using Ptr = std::shared_ptr<Buffer>;
    virtual ~Buffer() = default;
    virtual char* data() const = 0;
    virtual size_t size() const = 0;
};

// 字符串Buffer
class BufferString : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferString>;

    BufferString() = default;
    BufferString(std::string str) : _data(std::move(str)) {}
    BufferString(const char* data, size_t len) : _data(data, len) {}

    char* data() const override { return const_cast<char*>(_data.data()); }
    size_t size() const override { return _data.size(); }

    std::string& ref() { return _data; }
    void assign(const char* data, size_t len) { _data.assign(data, len); }
    void append(const char* data, size_t len) { _data.append(data, len); }
    void clear() { _data.clear(); }

private:
    std::string _data;
};

// 原始内存Buffer
class BufferRaw : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferRaw>;

    static Ptr create() { return std::make_shared<BufferRaw>(); }

    BufferRaw(size_t capacity = 0) {
        if (capacity > 0) {
            setCapacity(capacity);
        }
    }

    ~BufferRaw() override {
        if (_data) {
            delete[] _data;
        }
    }

    char* data() const override { return _data; }
    size_t size() const override { return _size; }

    void setSize(size_t size) { _size = size; }
    size_t getCapacity() const { return _capacity; }

    void setCapacity(size_t capacity) {
        if (_data) {
            delete[] _data;
        }
        _data = new char[capacity];
        _capacity = capacity;
    }

    void assign(const char* data, size_t len) {
        if (len > _capacity) {
            setCapacity(len);
        }
        memcpy(_data, data, len);
        _size = len;
    }

private:
    char* _data = nullptr;
    size_t _size = 0;
    size_t _capacity = 0;
};

} // namespace toolkit
