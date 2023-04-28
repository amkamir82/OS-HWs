/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>

int main(int argc, char **argv) {
    int *data;
    data = (int *) mm_malloc(1);
    data[0] = 1;
    data = mm_realloc(data, 4);
    printf("%d", data[0]);
    mm_free(data);
    return 0;
}
