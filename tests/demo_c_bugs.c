#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char* buf = malloc(100);
    int len = strlen(buf);
    printf("Length: %s\n", len);
    return 0;
}
