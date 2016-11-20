#include <stdlib.h>
#include <stdio.h>
#include <cstdlib>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>

#define MAX_LINE_LENGTH 255

struct
{
    bool isDebugMode;
    bool isUsageLaunch;

    int handlersCount = 4;
    char *input;
    char *output;
} args;

struct
{
    int rfds[10];
    int wfds[10];
} fds[2];

struct
{
    bool isHandler;
    bool isSource;
    bool isDestanition;

    int hdlIdx;
} procDscr;

struct
{
    fd_set fdset;
    struct timeval tv;

} selectVars;

void printUsage()
{
    printf("Usage:\n"
           "    Reverser [-d] [-w COUNT] (-if FILE) (-of FILE)\n"
           "    Reverser [-?|-h|--help]\n"
           "\n"
           "    -if             Input filename\n"
           "    -of             Output filename\n"
           "    -d              Debug mode\n"
           "    -w COUNT        Handlers count, from 1 to 10 (4 by default)\n"
           "    -?|-h|--help    Show help\n\n");
}

void parseArgs(int argc, char *argv[])
{
    for (int i(1); i < argc; ++i)
    {
        if (!strcmp(argv[i], "-d"))
        {
            args.isDebugMode = true;
            continue;
        }

        if (!strcmp(argv[i], "-?") || !strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            args.isUsageLaunch = true;
            continue;
        }

        if (!strcmp(argv[i], "-w") && i + 1 < argc)
        {
            args.handlersCount = std::atoi(argv[++i]);
            if (args.handlersCount < 1)
                args.isUsageLaunch = true;
            continue;
        }

        if (!strcmp(argv[i], "-if") && i + 1 < argc)
        {
            args.input = argv[++i];
            continue;
        }

        if (!strcmp(argv[i], "-of") && i + 1 < argc)
        {
            args.output = argv[++i];
            continue;
        }

        args.isUsageLaunch = true;
    }

    args.isUsageLaunch |= !args.input || !args.output;

    if (args.isUsageLaunch)
    {
        printUsage();
        exit (1);
    }
}

void createFileDescriptors()
{
    int pipes[2];
    for (int i(0); i < 2; ++i)
    {
        for (int j(0); j < args.handlersCount; ++j)
        {
            if (!pipe(pipes))
            {
                fds[i].rfds[j] = pipes[0];
                fds[i].wfds[j] = pipes[1];
            }
            else
            {
                printf("Not enough free file descriptors\n");
                exit(2);
            }
        }
    }
}

void createProcesses()
{
    pid_t pid;
    for (int i(0); i < args.handlersCount + 1; ++i)
    {
        pid = fork();
        if (pid < 0)
        {
            printf("Fork failed\n");
            exit (3);
        }
        if (!pid)
        {
            if (i < args.handlersCount)
            {
                procDscr.isHandler = true;
                procDscr.hdlIdx = i;
                return;
            }
            else
            {
                procDscr.isDestanition = true;
                return;
            }
        }
    }

    procDscr.isSource = true;
}

void closeUnusedFileDescriptors()
{
    for(int i(0); i < args.handlersCount; ++i)
    {
        if (!procDscr.isSource)
            close(fds[0].wfds[i]);
        if (!procDscr.isDestanition)
            close(fds[1].rfds[i]);

        if (!procDscr.isHandler || procDscr.hdlIdx != i)
        {
            close(fds[0].rfds[i]);
            close(fds[1].wfds[i]);
        }
    }
}

void createStructure()
{
    createFileDescriptors();
    createProcesses();
    closeUnusedFileDescriptors();
}

void initSelectVars(int fdsCount, int fd[], int seconds)
{
    selectVars.tv.tv_sec = seconds;
    selectVars.tv.tv_usec = 0;

    FD_ZERO(&selectVars.fdset);
    for(int i(0); i < fdsCount; ++i)
        FD_SET(fd[i], &selectVars.fdset);
}

int callReadSelect()
{
    return select(FD_SETSIZE, &selectVars.fdset, NULL, NULL, &selectVars.tv);
}

int callWriteSelect()
{
    return select(FD_SETSIZE, NULL, &selectVars.fdset, NULL, &selectVars.tv);
}

void startSource()
{
    char buffer[MAX_LINE_LENGTH];

    FILE* in = fopen(args.input, "r");

    for(int i(0); EOF != fscanf(in, "%s", buffer); ++i %= args.handlersCount)
    {
        if (args.isDebugMode)
            printf("Source: read %s\n", buffer);

        write(fds[0].wfds[i], buffer, sizeof(char) * MAX_LINE_LENGTH);
    }

    fclose(in);
    printf("Source: finished\n");
}

void startHandler()
{
    char buffer[MAX_LINE_LENGTH];

    int rPipe = fds[0].rfds[procDscr.hdlIdx];
    int wPipe = fds[1].wfds[procDscr.hdlIdx];

    while(true)
    {
        initSelectVars(1, &rPipe, 1);
        if (callReadSelect() == -1) {
            printf("Handler %i: read select failed\n", procDscr.hdlIdx + 1);
            break;
        }

        if (!read(rPipe, buffer, sizeof(char) * MAX_LINE_LENGTH))
            break;

        if (args.isDebugMode)
            printf("Handler %i: read %s\n", procDscr.hdlIdx + 1, buffer);

        for (int i(0); buffer[i]; ++i)
            buffer[i] = buffer[i] == '>'? '<' : '>';

        initSelectVars(1, &wPipe, 1);
        if (callWriteSelect() == -1) {
            printf("Handler %i: write select failed\n", procDscr.hdlIdx);
            break;
        }

        write(wPipe, buffer, sizeof(char) * MAX_LINE_LENGTH);

        if (args.isDebugMode)
            printf("Handler %i: write %s\n", procDscr.hdlIdx + 1, buffer);
    }

    printf("Handler %i finished\n", procDscr.hdlIdx + 1);
}

void startDestanition()
{
    char buffer[MAX_LINE_LENGTH];

    FILE* out = fopen(args.output, "w");

    for(bool wasRead(true); wasRead; )
    {
        wasRead = false;

        initSelectVars(args.handlersCount, fds[1].rfds, 2);
        if (callReadSelect() == -1) {
            printf("Destanition: select failed\n");
            break;
        }

        for (int i(0); i < args.handlersCount; ++i)
            if (read(fds[1].rfds[i], buffer, sizeof(char) * MAX_LINE_LENGTH))
            {
                fprintf(out, "%s\n", buffer);
                wasRead = true;

                if (args.isDebugMode)
                    printf("Destanition: write %s\n", buffer);
            }
    }

    fclose(out);

    printf("Destanition: finished\n");
}

void startWork()
{
    if (procDscr.isSource)
        startSource();

    if (procDscr.isHandler)
        startHandler();

    if (procDscr.isDestanition)
        startDestanition();
}

int main(int argc, char *argv[])
{
    parseArgs(argc, argv);
    createStructure();
    startWork();

    return 0;
}
