#include <stdio.h>

int main(void)
{
    fputs("Unit tests now run inside maint. Build with ENABLE_INTERNAL_TESTS and use --run-unit-tests.\n", stderr);
    return 1;
}
