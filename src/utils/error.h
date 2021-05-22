#ifndef __ERROR_H__
#define __ERROR_H__

// some of these were inspired by Godot:
// https://github.com/godotengine/godot/blob/7c721859170881a0e9f1e314837ba9fe28dc5110/core/error/error_macros.h

#include "log.h"
#include <stdarg.h>

enum Error { OK, FAIL };

#define OK Error::OK
#define FAIL Error::FAIL

/**
 * if val != OK, log err and return it
 */
#define ERR_TRY(val)                                                           \
	if (true) {                                                                \
		auto ret = val;                                                        \
		ERR_FAIL_COND_V(ret != OK, ret);                                       \
	} else                                                                     \
		((void)0)

#define ERR_FAIL_COND(condition)                                               \
	if (unlikely(condition)) {                                                 \
		LOG_ERR("\"%s\" is true.", _STR(condition));                           \
		return;                                                                \
	} else                                                                     \
		((void)0)

#define ERR_FAIL_COND_V(condition, return_val)                                 \
	if (unlikely(condition)) {                                                 \
		LOG_ERR("\"%s\" is true.", _STR(condition));                           \
		return return_val;                                                     \
	} else                                                                     \
		((void)0)

#define ERR_FAIL_COND_MSG(condition, ...)                                      \
	if (unlikely(condition)) {                                                 \
		LOG_ERR(__VA_ARGS__);                                                  \
		return;                                                                \
	} else                                                                     \
		((void)0)

#define ERR_FAIL_COND_V_MSG(condition, return_val, ...)                        \
	if (unlikely(condition)) {                                                 \
		LOG_ERR(__VA_ARGS__);                                                  \
		return return_val;                                                     \
	} else                                                                     \
		((void)0)

#define ERR_BREAK(condition)                                                   \
	if (unlikely(condition)) {                                                 \
		LOG_ERR("\"%s\" is true. Breaking.", _STR(condition));                 \
		break;                                                                 \
	} else                                                                     \
		((void)0)

#define ERR_BREAK_MSG(condition, ...)                                          \
	if (unlikely(condition)) {                                                 \
		LOG_ERR(__VA_ARGS__);                                                  \
		break;                                                                 \
	} else                                                                     \
		((void)0)

#endif // __ERROR_H__