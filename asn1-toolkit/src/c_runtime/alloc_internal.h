/* Private allocation seam. Production always uses the C library allocator.
 * Only the dedicated fault-test executable defines ASN1_C_RUNTIME_TEST_ALLOCATOR.
 * Include after <stdlib.h>; this header is never installed as public API. */
#ifndef ASN1_C_ALLOC_INTERNAL_H
#define ASN1_C_ALLOC_INTERNAL_H
#include <stddef.h>
#ifdef ASN1_C_RUNTIME_TEST_ALLOCATOR
void* asn1_test_malloc(size_t size);
void* asn1_test_calloc(size_t count, size_t size);
void* asn1_test_realloc(void* ptr, size_t size);
void asn1_test_free(void* ptr);
#define malloc asn1_test_malloc
#define calloc asn1_test_calloc
#define realloc asn1_test_realloc
#define free asn1_test_free
#endif
#endif
