#pragma once
#include "wo_global_setting.hpp"

#include <stdint.h>
#include <stddef.h>

void womem_init(size_t virtual_pre_alloc_size);
void womem_shutdown(void);

typedef uint8_t womem_attrib_t;

void* womem_alloc(size_t size, womem_attrib_t attrib);
void womem_free(void* memptr);
void womem_tidy_pages(bool full);

void* womem_verify(void* memptr, womem_attrib_t** attrib);

void* womem_enum_pages(size_t* page_count, size_t* page_size);
void* womem_get_unit_buffer(void* page, size_t* unit_count, size_t* unit_size);
void* womem_get_unit_page(void* unit);
void* womem_get_unit_ptr_attribute(void* unit, womem_attrib_t** attrib);

namespace wo
{
    void* alloc64(size_t memsz, womem_attrib_t attrib);
    void free64(void* ptr);
}
