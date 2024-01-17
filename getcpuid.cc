#include <stdio.h>
#include <string.h>

#define cpuid(in,a,b,c,d)  asm("cpuid": "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (in));
 
static int getcpuid (char *id, size_t max)
{
    int i;
    unsigned long li, maxi, maxei, eax, ebx, ecx, edx, unused;
 
    cpuid (0, maxi, unused, unused, unused);
    maxi &= 0xffff;
 
    if (maxi < 3)
    {
        return -1;
    }
 
    cpuid (3, eax, ebx, ecx, edx);
 
    snprintf (id, max, "%08lx %08lx %08lx %08lx", eax, ebx, ecx, edx);
    fprintf (stdout, "get cpu id: %s/n", id);
 
    return 0;
}