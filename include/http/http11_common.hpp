/**
 * @file http11_common.hpp
 * @brief HTTP/1.1解析器通用类型定义
 * 
 * 本文件定义了HTTP/1.1解析器中使用的通用回调函数类型，
 * 包括元素回调和字段回调函数类型定义。
 * 这些回调函数用于在解析过程中通知上层应用解析到的数据。
 */

#pragma once

#include <sys/types.h>

/**
 * @brief 元素回调函数类型定义
 * 
 * 该回调函数用于处理HTTP解析过程中的单个元素，
 * 如HTTP方法、URI、版本等。
 * 
 * @param data 用户自定义数据指针
 * @param at 指向元素数据的指针
 * @param length 元素数据的长度
 */
typedef void (*element_cb)(void* data, const char* at, size_t length);

/**
 * @brief 字段回调函数类型定义
 * 
 * 该回调函数用于处理HTTP解析过程中的键值对字段，
 * 如HTTP头部字段。
 * 
 * @param data 用户自定义数据指针
 * @param field 指向字段名的指针
 * @param flen 字段名的长度
 * @param value 指向字段值的指针
 * @param vlen 字段值的长度
 */
typedef void (*field_cb)(void* data, const char* field, size_t flen, const char* value,
                         size_t vlen);