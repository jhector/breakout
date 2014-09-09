#include <cstdio>
#include <cstdlib>

#include <stdint.h>
#include <string.h>
#include <unistd.h>

uint32_t read_until(int32_t fd, char term, char *out, uint32_t max)
{
    uint32_t count = 0;
    char input;

    while (read(fd, &input, 1) == 1 &&
            input != term &&
            count < max) {
        out[count] = input;
        count++;
    }

    out[count] = '\0';
    return count;
}

void help()
{
    puts("Available commands:");
    puts("help - shows this help");
    puts("exit - leave");
}

void interact()
{
    char cmd[8] = {0};

    write(1, "> ", 2);

    read(0, cmd, sizeof(cmd)-1);

    if (!strncmp(cmd, "help", 4)) {
        help();
    } else if (!strncmp(cmd, "exit", 4)) {
        throw 2;
    } else {
        throw 1;
    }
}

int32_t main()
{
    while (1) {
        try {
            interact();
        } catch (int32_t e) {
            if (e == 1)
                puts("unknown command");
            else if (e == 2)
                break;
        }
    }

    return 0;
}
