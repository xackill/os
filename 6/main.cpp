#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 100
#define MAX_NUMBERS_COUNT 100

int index = 0;
int numbers[MAX_NUMBERS_COUNT];

bool isDigit(char character)
{
    return character >= '0' && character <= '9';
}

void addNumber(int number)
{
    numbers[index++] = number;

    if (index > MAX_NUMBERS_COUNT)
    {
        printf("ERROR! Too many numbers\n");
        exit(2);
    }
}

void readFile(char* filename)
{
    FILE* in = fopen(filename, "r");
    if (in == NULL)
    {
        printf("ERROR! Cannot open file %s\n", filename);
        return;
    }

    int number = 0;
    bool readIt = false;
    char buffer[BUFFER_SIZE];

    for (int len(1); len = fread(buffer, 1, BUFFER_SIZE, in); )
    {
        for (int i(0); i < len; ++i)
            if (isDigit(buffer[i]))
            {
                number = number * 10 + (buffer[i] - '0');
                readIt = true;
            }
            else
            {
                if (readIt)
                {
                    addNumber(number);

                    number = 0;
                    readIt = false;
                }
            }
    }

    fclose(in);

    if (readIt)
        addNumber(number);
}

int comp (const void *f, const void *s)
{
    return *(int *) f - *(int *) s;
}

bool isSortSuccess()
{
    for (int i(1); i < index; ++i)
        if (numbers[i - 1] > numbers[i])
            return false;
    return true;
}

void saveNumbersToFile(char * filename)
{
    FILE* out = fopen(filename, "w");
    if (out == NULL)
    {
        printf("ERROR! Cannot open file %s\n", filename);
        return;
    }

    for (int i(0); i < index; ++i)
        if (!fprintf(out, "%d\n", numbers[i]))
        {
            printf("ERROR! Cannot write to file %s\n", filename);
            exit(4);
        }

    fclose(out);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("USAGE: file1 [file2] ... [file n] output\n");
        return 1;
    }

    for (int i(1); i < argc - 1; ++i)
        readFile(argv[i]);

    qsort(numbers, index, sizeof(int), comp);

    if (!isSortSuccess())
    {
        printf("ERROR! Sort is broken!\n");
        exit(3);
    }

    saveNumbersToFile(argv[argc-1]);

    return 0;
}
