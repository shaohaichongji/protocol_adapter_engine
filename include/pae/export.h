#pragma once

#if defined(_WIN32) && !defined(PAE_STATIC_DEFINE)
#if defined(PAE_BUILDING_LIBRARY)
#define PAE_API __declspec(dllexport)
#else
#define PAE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && !defined(PAE_STATIC_DEFINE)
#define PAE_API __attribute__((visibility("default")))
#else
#define PAE_API
#endif
