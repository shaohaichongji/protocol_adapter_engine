#pragma once

// 仅控制符号导入/导出：静态消费由目标提供 PAE_STATIC_DEFINE，DLL 构建方设置
// PAE_BUILDING_LIBRARY；调用方不要用这些宏推断运行能力或稳定 ABI。
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
