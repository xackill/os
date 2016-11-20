#include <stdlib.h>
#include <stdio.h>

#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <string.h>
#include <time.h>

struct
{
    bool isDebugMode;
    bool isUsageLaunch;

    int sBlock;
    int eBlock;

    char *filename;
    char lckFile[256];
    char lcklckFile[256];
} args;

FILE *lckFile, *lcklckFile;

void printUsage()
{
    printf("Usage:\n"
           "    Saver [-d] (FILE) [-s NUMBER -e NUMBER]\n"
           "    Saver [-?|-h|--help]\n"
           "\n"
           "    FILE            Output filename\n"
           "    -d              Debug mode\n"
           "    -s NUMBER       First blocked byte\n"
           "    -e NUMBER       Last blocked byte\n"
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

        if (!strcmp(argv[i], "-s") && i + 1 < argc)
        {
            args.sBlock = std::atoi(argv[++i]);
            continue;
        }

        if (!strcmp(argv[i], "-e") && i + 1 < argc)
        {
            args.eBlock = std::atoi(argv[++i]);
            continue;
        }

        if (!args.filename)
        {
            args.filename = argv[i];
            strcpy(args.lcklckFile, args.filename);

            int size = strlen(args.filename);
            strcpy(args.lcklckFile + size, ".lck.lck");

            *(args.lcklckFile + size + 4) = 0;
            strcpy(args.lckFile, args.lcklckFile);

            *(args.lcklckFile + size + 4) = '.';
            continue;
        }

        args.isUsageLaunch = true;
    }

    args.isUsageLaunch |= !args.filename;

    if (args.isUsageLaunch)
    {
        printUsage();
        exit (0);
    }
}

bool isIntersect(int sBlock, int eBlock)
{
    return ((sBlock <= args.sBlock && args.sBlock <= eBlock) || (sBlock <= args.eBlock && args.eBlock <= eBlock) ||
            (args.sBlock <= sBlock && sBlock <= args.eBlock) || (args.sBlock <= eBlock && eBlock <= args.eBlock));
}

bool isRequiredPartFree(FILE *lckFile)
{
    char bType;
    int pid, sBlock, eBlock;

    while (fscanf(lckFile, "%d %d %d %c", &pid, &sBlock, &eBlock, &bType) != EOF)
    {
        if (bType == 'w' && isIntersect(sBlock, eBlock))
            return false;
    }

    return true;
}

bool clearRequiredPart(FILE *lckFile)
{
    char bType;
    int pid, sBlock, eBlock;

    int mypid = getpid();
    bool isActive = false;

    while (fscanf(lckFile, "%d %d %d %c", &pid, &sBlock, &eBlock, &bType) != EOF)
    {
        if (pid == mypid) {
            fseek(lckFile, -1, SEEK_CUR);
            fputc('e', lckFile);
        }
        else
        {
            if (bType == 'w') isActive = true;
        }
    }

    return isActive;
}

void updateFile()
{
    FILE *file;
    if (access(args.filename, F_OK) != -1)
        file = fopen(args.filename, "r+b");
    else
        file = fopen(args.filename, "w+b");

    fseek(file, args.sBlock, SEEK_SET);

    double size = args.eBlock - args.sBlock;

    for (int i(0); i <= size; ++i) {
        fprintf(file, "%c", 65 + abs(rand()) % 26);
        printf("\rUpdating: %d%%", (int) (i / size * 100));

        usleep(10000e);
    }

    printf("\n");
    fclose(file);
}

void locklck()
{
    bool isWaited = false;
    while (access(args.lcklckFile, F_OK) != -1)
    {
        if (args.isDebugMode && !isWaited) {
            printf("Waiting... %s is locked by another program\n", args.lckFile);
            isWaited = true;
        }

        usleep(500);
    }

    lcklckFile = fopen(args.lcklckFile, "w+b");
    fprintf(lcklckFile, "%d w", getpid());

    if (args.isDebugMode)
        printf("%s is locked\n", args.lckFile);
}

void unlocklck()
{
    fclose(lcklckFile);
    remove(args.lcklckFile);

    if (args.isDebugMode)
        printf("%s is unlocked\n", args.lckFile);
}

void lock()
{
    bool isLocked = false;
    do
    {
        locklck();

        if (access(args.lckFile, F_OK) == -1)
            lckFile = fopen(args.lckFile, "w+b");
        else
            lckFile = fopen(args.lckFile, "a+b");

        if (!isRequiredPartFree(lckFile))
        {
            if (args.isDebugMode)
                printf("Waiting... %s is locked by another program\n", args.filename);
        }
        else
        {
            fprintf(lckFile, "%d %d %d w\n", getpid(), args.sBlock, args.eBlock);
            if (args.isDebugMode)
                printf("%s is locked\n", args.filename);

            isLocked = true;
        }

        fclose(lckFile);
        unlocklck();

        if (!isLocked)
            usleep(500);
    }
    while (!isLocked);
}

void unlock()
{
    locklck();

    lckFile = fopen(args.lckFile, "r+b");
    bool isActive = clearRequiredPart(lckFile);
    if (args.isDebugMode)
        printf("%s is unlocked\n", args.filename);

    fclose(lckFile);
    if (!isActive) remove(args.lckFile);

    unlocklck();
}

int main(int argc, char *argv[])
{
    parseArgs(argc, argv);

    time_t t;
    srand((unsigned) time(&t));

    lock();
    updateFile();
    unlock();

    return 0;
}
