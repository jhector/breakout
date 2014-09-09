#include <cstdio>
#include <cstdlib>

#include <sys/mman.h>
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

typedef struct maprange_s {
    uint64_t start;
    uint64_t end;
} Maprange;

static Prisoner *head = NULL;

static Maprange *dst_whitelist = NULL;

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

void get_mapped_area(const char *section, const char *perm,
        uint64_t *start, uint64_t *end)
{
    char buffer[256] = {0};
    char *ptr = NULL;

    int32_t maps = open("/proc/self/maps", S_IRUSR);
    if (maps < 0)
        goto fail;

    while (read_until(maps, '\n', buffer, sizeof(buffer)-1)) {
        if (!strstr(buffer, section) || !strstr(buffer, perm))
            continue;

        if ((ptr = strchr(buffer, '-')) == NULL)
            goto fail_close;

        *ptr = '\0';

        if ((ptr = strchr(ptr+1, ' ')) == NULL)
            goto fail_close;

        *ptr = '\0';
        *start = strtoul(buffer, NULL, 16);

        ptr = buffer + strlen(buffer) + 1;
        *end = strtoul(ptr, NULL, 16);
    }

fail_close:
    close(maps);
fail:
    return;
}

void secure_self()
{
    uint64_t start = 0;
    uint64_t end = 0;

    dst_whitelist = (Maprange*)mmap(0, 4096, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    if (dst_whitelist == MAP_FAILED)
        abort();

    get_mapped_area("heap", "rw-p", &start, &end);
    if (!start || !end)
        abort();

    dst_whitelist[0].start = start;
    dst_whitelist[0].end = end;

    start = end = 0;
    get_mapped_area("libgcc", "rw-p", &start, &end);
    if (!start || !end)
        abort();

    dst_whitelist[1].start = start;
    dst_whitelist[1].end = start + 0x1100;

    if (mprotect(dst_whitelist, 4096, PROT_READ) < 0)
        abort();
}

void init()
{
    Prisoner *curr = NULL;
    int32_t fd = -1;
    uint32_t counter = 0;

    char buffer[128] = {0};
    char *iter = NULL;
    char *ptr = NULL;

    fd = open(CWD"prisoner", S_IRUSR);
    if (fd < 0)
        abort();

    while (read_until(fd, '\n', buffer, sizeof(buffer))) {
       head = (Prisoner*)calloc(1, sizeof(Prisoner));
       if (!head)
           abort();

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

    secure_self();
    return;
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
