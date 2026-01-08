// out_value.h
// 输出值包装：用于禁止参数中出现 int* / int& / bool* / bool& 等基础类型指针/引用。
#pragma once

#include <cstddef>

namespace ktv::utils {

template <typename T>
struct OutValue {
  T value{};
};

using OutInt = OutValue<int>;
using OutLong = OutValue<long>;
using OutDouble = OutValue<double>;
using OutBool = OutValue<bool>;
using OutSizeT = OutValue<size_t>;
using OutU32 = OutValue<unsigned int>;

}  // namespace ktv::utils


