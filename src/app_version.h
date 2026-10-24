#pragma once
#define LG_VERSION "0.14.0-beta.1"
#define LG_WIDE_IMPL(value) L##value
#define LG_WIDE(value) LG_WIDE_IMPL(value)
#define LG_VERSION_W LG_WIDE(LG_VERSION)
