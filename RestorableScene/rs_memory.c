#ifdef __cplusplus
#   error "This source should be compiled by C."
#endif

#include <stdlib.h>


void* _rs_aligned_alloc(size_t allocsz, size_t allign)
{
    VC
    aligned_alloc
}
void _rs_alligned_free(void* memptr);