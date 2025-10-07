#include <stdio.h>

int main(void)
{
    fputs("End-to-end tests now run inside maint. Build with ENABLE_INTERNAL_TESTS and use --run-e2e-tests.\n", stderr);
    return 1;
}
