#include <cstdio>
#include <cstdlib>

#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define CWD ""

typedef struct prisoner_s {
    char *name;
    char *aka;
    uint32_t age;
    uint32_t cell;
    char *sentence;
    char *note;
    uint32_t note_size;
    struct prisoner_s *next;
} Prisoner;

static Prisoner *head = NULL;

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

void init()
{
    Prisoner *curr = NULL;
    int32_t fd = -1;
    uint32_t counter = 0;

    char buffer[128] = {0};
    char *err = NULL;
    char *iter = NULL;
    char *ptr = NULL;

    fd = open(CWD"prisoner", S_IRUSR);
    if (fd < 0) {
        err = (char*)"! failed to open prisoner database";
        goto fail;
    }

    while (read_until(fd, '\n', buffer, sizeof(buffer))) {
       head = (Prisoner*)calloc(1, sizeof(Prisoner));
       if (!head) {
           err = (char*)"! failed to allocate memory";
           goto fail;
       }

       head->next = curr;
       curr = head;

       iter = buffer;
       if ((ptr = strchr(iter, ':')))
           *ptr = '\0';

       asprintf(&curr->name, "%s", iter);

       iter = ptr + 1;
       if ((ptr = strchr(iter, ':')))
           *ptr = '\0';

       asprintf(&curr->aka, "%s", iter);

       iter = ptr + 1;
       if ((ptr = strchr(iter, ':')))
           *ptr = '\0';

       curr->age = atoi(iter);

       iter = ptr + 1;

       asprintf(&curr->sentence, "%s", iter);

       curr->cell = counter;
       counter++;
    }

    close(fd);
    return;

fail:
    puts(err);
    abort();
}

void help()
{
    puts("Available commands:");
    puts("help - shows this help");
    puts("list - lists all prisoner");
    puts("exit - leave");
}

void list()
{
    Prisoner *iter = head;
    while (iter) {
        printf("Prisoner: %s (%s)\n" \
               "Age: %d\n" \
               "Cell: %d\n" \
               "Sentence: %s\n",
               iter->name ? iter->name : "",
               iter->aka ? iter->aka : "",
               iter->age,
               iter->cell,
               iter->sentence ? iter->sentence : "");

        write(1, "Note: ", 6);
        write(1, iter->note, iter->note_size);

        puts("\n");

        iter = iter->next;
    }
}

void interact()
{
    char cmd[8] = {0};

    write(1, "> ", 2);

    read(0, cmd, sizeof(cmd)-1);

    if (!strncmp(cmd, "help", 4)) {
        help();
    } else if (!strncmp(cmd, "list", 4)) {
        list();
    } else if (!strncmp(cmd, "exit", 4)) {
        throw 2;
    } else {
        throw 1;
    }
}

int32_t main()
{
    init();

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
