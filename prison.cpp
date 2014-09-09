#include <cstdio>
#include <cstdlib>

#include <stdint.h>

void test()
{
    throw 1;
}

int32_t main()
{
    try {
        test();
    } catch (int32_t e) {
        printf("Excpetion: %d\n", e);
    }

    return 0;
}
