#pragma once
#define LG_VERSION "0.13.0-beta.6"
#define LG_WIDE_IMPL(value) L##value
#define LG_WIDE(value) LG_WIDE_IMPL(value)
#define LG_VERSION_W LG_WIDE(LG_VERSION)
