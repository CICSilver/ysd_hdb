#ifndef YSD_HDB_COMMON_H
#define YSD_HDB_COMMON_H

#include "../common/HdbCommon.h"

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define HDB_SNPRINTF _snprintf // VS2010 snprintf 兼容入口
#define HDB_INT64_FORMAT "%I64d" // MSVC long long 格式
#else
#define HDB_SNPRINTF snprintf // POSIX snprintf
#define HDB_INT64_FORMAT "%lld" // GCC long long 格式
#endif

#endif
