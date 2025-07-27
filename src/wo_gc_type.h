// wo_gc_type.h
#pragma once

#include "wo.h"

typedef struct _wo_value_instance wo_value_instance;
typedef struct _wo_gc_string wo_gc_string;
typedef struct _wo_gc_array wo_gc_array;
typedef struct _wo_gc_dict wo_gc_dict;
typedef struct _wo_gc_struct wo_gc_struct;

struct _wo_value_instance
{
    union data_t
    {
        wo_gc_struct* m_struct;
        wo_gc_string* m_string;
        wo_gc_array* m_array;
        wo_gc_dict* m_dict;
    };
};

struct _wo_gc_string
{
    const char* m_str;
    size_t m_len;
};

void wo_gc_string_init(
    const char* data, size_t len, wo_gc_string* out_str);
void wo_gc_string_add(
    const wo_gc_string* in_a, const wo_gc_string* in_b, wo_gc_string* out_str);
void wo_gc_string_free(
    wo_gc_string* free_str);

struct _wo_gc_array
{
    wo_value_instance* m_data;
    size_t m_len;
    size_t m_capacity;
};

void wo_gc_array_init(size_t size, wo_gc_array* out_arr);
void wo_gc_array_reserve(size_t size, wo_gc_array* out_arr);

struct _wo_gc_dict_bucket_t
{
    struct _wo_gc_dict_bucket_t* next;
    wo_value_instance m_key;
    wo_value_instance m_value;
};

struct _wo_gc_dict
{
    struct _wo_gc_dict_bucket_t* m_free_buckets;
    struct _wo_gc_dict_bucket_t* m_buckets;
    size_t m_len;
    size_t m_mask;
};

struct _wo_gc_struct
{
    wo_value_instance* m_data;
    size_t m_len;
};
