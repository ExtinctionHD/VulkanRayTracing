#pragma once

#include "DataHelpers.hpp"

namespace Numbers
{
    constexpr float kPi = glm::pi<float>();
    constexpr float kInversePi = 1.0f / kPi;

    constexpr uint64_t kMaxUint = std::numeric_limits<uint64_t>::max();

    constexpr float kMili = 0.001f;
    constexpr float kMicro = 0.001f * kMili;
    constexpr float kNano = 0.001f * kMicro;

    constexpr uint32_t kKilobyte = 1024;
    constexpr uint32_t kMegabyte = 1024 * kKilobyte;
    constexpr uint32_t kGigabyte = 1024 * kMegabyte;
}

namespace Matrix4
{
    constexpr glm::mat4 kIdentity = glm::mat4(1.0f);
}

namespace Vector3
{
    constexpr glm::vec3 kZero = glm::vec3(0.0f, 0.0f, 0.0f);
    constexpr glm::vec3 kX = glm::vec3(1.0f, 0.0f, 0.0f);
    constexpr glm::vec3 kY = glm::vec3(0.0f, 1.0f, 0.0f);
    constexpr glm::vec3 kZ = glm::vec3(0.0f, 0.0f, 1.0f);
}

std::string Format(const char* fmt, ...);

template <class T>
void CombineHash(std::size_t& s, const T& v)
{
    std::hash<T> h;
    s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

template <class TSrc, class TDst>
std::vector<TDst> CopyVector(const std::vector<TSrc>& src)
{
    return std::vector<TDst>(reinterpret_cast<const TDst*>(src.data()),
            reinterpret_cast<const TDst*>(src.data() + src.size()));
}

template <class T>
std::vector<T> Repeat(T value, size_t count)
{
    return std::vector<T>(count, value);
}

template <class TInst, class TFunc>
auto MakeFunction(TInst* instance, TFunc&& function)
{
    return [instance, function](auto&&... args)
        {
            return (instance->*function)(std::forward<decltype(args)>(args)...);
        };
}

Bytes GetBytes(const std::vector<ByteView>& byteViews);

template <class... Types>
Bytes GetBytes(Types ... values)
{
    Bytes bytes;

    uint32_t offset = 0;
    const auto functor = [&](const auto& value)
        {
            const uint32_t size = static_cast<uint32_t>(sizeof(value));

            bytes.resize(offset + size);
            std::memcpy(bytes.data() + offset, &value, size);

            offset += size;
        };

    (functor(values), ...);

    return bytes;
}

template <class T>
bool Contains(const std::vector<T>& vector, const T& value)
{
    return std::find(vector.begin(), vector.end(), value) != vector.end();
}

template <class K, class T>
bool Contains(const std::map<K, T>& map, const K& key)
{
    return map.find(key) != map.end();
}
