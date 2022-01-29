#include <cstdlib>

// From https://www.cnblogs.com/deatharthas/p/14502337.html

void* _rs_aligned_alloc(size_t allocsz, size_t allign)
{
	size_t offset = allign - 1 + sizeof(void*);
	void* originalP = malloc(allocsz + offset);
	size_t originalLocation = reinterpret_cast<size_t>(originalP);
	size_t realLocation = (originalLocation + offset) & ~(allign - 1);
	void* realP = reinterpret_cast<void*>(realLocation);
	size_t originalPStorage = realLocation - sizeof(void*);
	*reinterpret_cast<void**>(originalPStorage) = originalP;
	return realP;
}
void _rs_aligned_free(void* memptr)
{
	size_t originalPStorage = reinterpret_cast<size_t>(memptr) - sizeof(void*);
	free(*reinterpret_cast<void**>(originalPStorage));
}
