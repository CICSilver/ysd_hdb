#ifndef YSD_HDB_COMMON_H
#define YSD_HDB_COMMON_H

#include "../common/HdbCommon.h"

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define HDB_SNPRINTF _snprintf
#define HDB_INT64_FORMAT "%I64d"
#else
#define HDB_SNPRINTF snprintf
#define HDB_INT64_FORMAT "%lld"
#endif

#endif
