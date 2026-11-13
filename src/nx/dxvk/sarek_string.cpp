#include "util/util_string.h"
#include <codecvt>
#include <locale>
#include <algorithm>

namespace dxvk::str {
std::string fromws(const WCHAR *text) {
    return text ? std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(text) : "";
}
std::wstring tows(const char *text) {
    return text ? std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(text) : L"";
}
void tows(const char *text, WCHAR *output, size_t capacity) {
    if (!capacity) return;
    const auto converted = tows(text);
    const auto count = std::min(capacity - 1, converted.size());
    std::copy_n(converted.data(), count, output);
    output[count] = 0;
}
}
