#include <stdlib.h>
#include <stdio.h>

#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>

struct
{
    bool isDebugMode;
    bool isUsageLaunch;
    char *filename;
} args;

int file;
char buff[BUFSIZ];
bool isZeroByte = true;
size_t rCount, fCount, dStart, dCount, bStart, bCount;

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

        if (!args.filename)
        {
            args.filename = argv[i];
            continue;
        }

        args.isUsageLaunch = true;
    }

    args.isUsageLaunch |= !args.filename;
}

void printUsage()
{
    printf("Usage:\n"
           "    Saver [-d] (FILE)\n"
           "    Saver [-?|-h|--help]\n"
           "\n"
           "    FILE            Output filename\n"
           "    -d              Debug mode\n"
           "    -?|-h|--help    Show help\n\n");
}

void writeToFile(bool debug)
{
    if (dCount)
    {
        if (isZeroByte) {
            lseek(file, dCount - 1, SEEK_CUR);
            write(file, "", 1);
            if (debug) printf("Zero-bytes (%lu - %lu)\n", dStart, dStart + dCount - 1);
        } else {
            write(file, buff + bStart, bCount);
            if (debug) printf("Non-zero-bytes (%lu - %lu)\n", dStart, dStart + dCount - 1);
        }
    }
}

int main(int argc, char *argv[])
{
    parseArgs(argc, argv);
    if (args.isUsageLaunch)
    {
        printUsage();
        return 0;
    }

    file = open(args.filename, O_CREAT | O_WRONLY, 1023);
    if (file < 0)
    {
        printf("%s", strerror(errno));
        return 1;
    }

    while ((rCount = fread(buff, 1, BUFSIZ, stdin)) > 0)
    {
        for (size_t i(0); i < rCount; ++i)
        {
            if ((isZeroByte && buff[i] == 0) || (!isZeroByte && buff[i] != 0))
            {
                ++dCount;
                ++bCount;
            }
            else
            {
                writeToFile(args.isDebugMode);

                bStart = i;
                dStart = fCount + i;
                bCount = dCount = 1;
                isZeroByte = (buff[i] == 0);
            }
        }

        if (!isZeroByte) {
            writeToFile(false);
            bStart = bCount = 0;
        }

        fCount += rCount;
    }

    writeToFile(args.isDebugMode);
    close(file);

    return 0;
}
