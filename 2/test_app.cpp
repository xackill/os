#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <cstdlib>

bool isHelpArgument(char *arg)
{
    return !strcmp(arg, "-?") || !strcmp(arg, "-h") || !strcmp(arg, "--help");
}

void printUsage()
{
    printf("Usage:\n"
           "    Test [1|2|3]\n"
           "    Test [-?|-h|--help]\n"
           "\n"
           "    Just sleep 1, 2 or 3 seconds\n");
}

const int MAX_TASKS_COUNT = 10;
const int MAX_FAILED_LAUNCHES = 50;
const int READ_BUFF_SIZE = 10;

int main(int argc, char *argv[])
{
    if (argc == 1 || isHelpArgument(argv[1]))
    {
        printUsage();
        return 0;
    }

    if (!strcmp(argv[1], "1"))
            sleep(1);
    else if (!strcmp(argv[1], "2"))
            sleep(2);
    else if (!strcmp(argv[1], "3"))
            sleep(3);
    else printUsage();

    return 0;
}
