#ifndef GBNH_NATIVE_H
#define GBNH_NATIVE_H

#if defined(_WIN32)
#  if defined(GBNH_NATIVE_BUILD)
#    define GBNH_NATIVE_API __declspec(dllexport)
#  else
#    define GBNH_NATIVE_API __declspec(dllimport)
#  endif
#else
#  define GBNH_NATIVE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

GBNH_NATIVE_API const char* gbnh_native_version(void);
GBNH_NATIVE_API int gbnh_native_mgba_available(void);

#ifdef __cplusplus
}
#endif

#endif
