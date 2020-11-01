#include "types.h"
#include "stat.h"
#include "user.h"

int main()
{
    volatile float a = 0, b = 1.43, c = 1.35;

    for (volatile int i = 0; i < 10000000; i++)
    {
        for (volatile int z = 0; z < 50; z++)
            a = (a * b + c);
    }

    exit();
}