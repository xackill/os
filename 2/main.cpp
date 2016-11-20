#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
//====================================================================
const int MAX_TASKS_COUNT = 10;
const int MAX_FAILED_LAUNCHES = 50;
const int READ_BUFF_SIZE = 256;

struct Task
{
    int i;
    pid_t pid;

    char **args;
    bool isRespawn;
    char *pathToPidFile;

    int s,f;
    time_t fstarts[MAX_FAILED_LAUNCHES];
};


time_t lastLaunch = 0;
int activeTasksCount = 0;
bool configShouldBeUpdated = true;
Task tasks[MAX_TASKS_COUNT];
//====================================================================
bool isHelpArgument(char *arg)
{
    return !strcmp(arg, "-?") || !strcmp(arg, "-h") || !strcmp(arg, "--help");
}

void printUsage()
{
    printf("Usage:\n"
           "    Logger (FILE)\n"
           "    Logger [-?|-h|--help]\n"
           "\n"
           "    Reads from FILE tasks like 'app [args] (w|r)' and manages them\n");
}

void checkForUsageLaunch(int argc, char *argv[])
{
    if (argc == 1 || isHelpArgument(argv[1]))
    {
        printUsage();
        exit(EXIT_SUCCESS);
    }
}

void closeAllAndExit(int status)
{
    syslog(LOG_INFO, "%s", "Logger closed");
    closelog();
    exit(status);
}

void closeAllWithError()
{
    syslog(LOG_ERR, "%s", strerror(errno));
    closeAllAndExit(EXIT_FAILURE);
}

void becameDaemonOrDieTrying()
{
    pid_t cpid = fork();
    if (cpid > 0)
        exit(EXIT_SUCCESS);

    openlog("Logger", 0, LOG_INFO | LOG_WARNING | LOG_ERR);
    syslog(LOG_INFO, "\%s", "Start");

    if ((chdir("/")) < 0)
        closeAllWithError();
}

void sigHupHandler(int signum)
{
    configShouldBeUpdated = true;
}

void setSighupHandler()
{
    signal(SIGHUP, sigHupHandler);
}
//====================================================================
void clearTask(int i)
{
    Task *task = &tasks[i];

    if (remove(task->pathToPidFile))
        syslog(LOG_WARNING, "%s", strerror(errno));

    for(int j(0); j < MAX_FAILED_LAUNCHES; ++j)
        task->fstarts[j] = 0;

    free(task->args);
    free(task->pathToPidFile);

    task->args = NULL;
    task->pathToPidFile = NULL;
    task->f = task->s = task->pid = task->isRespawn = 0;

    --activeTasksCount;
}

void clearAllTasks()
{
    for (int i = 0; i < MAX_TASKS_COUNT; ++i)
    {
        if (!tasks[i].pid)
            continue;

        kill(tasks[i].pid, SIGKILL);
        clearTask(i);
    }
}

char* getPidFilePathByIdx(int i)
{
    char *path = new char[256];
    strcpy(path, "/tmp/");
    char *c = strcpy(path + 251, ".pid");
    do { *--c = '0' + i % 10; } while (i /= 10);
    strcpy(path + 5, c);

    return path;
}

bool parseLineAndSaveTask(int i, char *line)
{
    int len = strlen(line);
    if (line[len - 1] == '\n') line[--len] = 0;

    char *taskType = line + len - 2;
    if (strcmp(taskType, " w") && strcmp(taskType, " r"))
    {
        syslog(LOG_WARNING, "Cannot parse '%s'", line);
        return false;
    }

    Task *t = &tasks[i];
    t->isRespawn = !strcmp(taskType, " r");

    char *toSplit = new char[--len];
    strncpy(toSplit, line, --len);
    toSplit[len] = 0;

    int argc = 0;
    for(char *beg = toSplit; *beg; ++beg)
        if (*beg == ' ' && *(beg + 1) != ' ') ++argc;

    t->args = new char* [argc + 2];
    t->args[0] = strtok (toSplit, " ");
    for(int i(1); i <= argc; ++i)
        t->args[i] = strtok (NULL, " ");
    t->args[argc + 1] = NULL;

    t->pathToPidFile = getPidFilePathByIdx(i);

    return true;
}

void readConfig(char *path)
{
    syslog(LOG_INFO, "\%s", "Config reading...");

    FILE *file = fopen(path, "r");
    if (!file)
        closeAllWithError();

    int fsize = READ_BUFF_SIZE;
    char *fbuff = new char[READ_BUFF_SIZE + 1];
    char *rbuff = fbuff;
    for (int i(0); i < MAX_TASKS_COUNT; )
    {
        bool isEOF = !fgets(rbuff, READ_BUFF_SIZE + 1, file);
        if (isEOF && rbuff == fbuff) break;

        if (isEOF || strstr(rbuff, "\n"))
        {
            if (parseLineAndSaveTask(i, fbuff)) ++i;
            rbuff = fbuff;
        }
        else
        {
            rbuff += READ_BUFF_SIZE;

            if (rbuff - fbuff == fsize)
            {
                fbuff = (char*)realloc(fbuff, sizeof(char) * (fsize + READ_BUFF_SIZE));
                rbuff = fbuff + fsize;
                fsize += READ_BUFF_SIZE;
            }
        }
    }

    fclose(file);
    free (fbuff);
    syslog(LOG_INFO, "\%s", "...Done!");
}

void saveTaskPidToFile(int i)
{
    Task *task = &tasks[i];
    FILE *file = fopen(task->pathToPidFile, "w+");
    if (file)
    {
        fprintf(file, "%d", task->pid);
        fclose(file);
    }
    else
    {
        syslog(LOG_WARNING, "Cannot save task pid %d to file %s", task->pid, task->pathToPidFile);
    }
}

void launchNewTask(int i, bool delayed = false)
{
    Task *task = &tasks[i];

    pid_t cpid = fork();
    switch (cpid)
    {
        case -1:
            syslog(LOG_ERR, "%s", "Fork failed; cpid == -1");
            return;

        case 0:
            if (delayed)
            {
                sleep(60 * 60);
                syslog(LOG_INFO, "Child number %d pid %d respawn", i, getpid());
            }

            execv(task->args[0], task->args);
            exit(EXIT_FAILURE);

        default:
            if (!task->pid)
            {
                activeTasksCount++;
                syslog(LOG_INFO, "Child number %d pid %d started", i, cpid);
            }
            else if (!delayed)
                    syslog(LOG_INFO, "Child number %d pid %d respawn", i, cpid);

            task->pid = cpid;
            saveTaskPidToFile(i);
    }
}

void updateConfig(char *confpath)
{
    clearAllTasks();
    readConfig(confpath);
}

void launchTasks()
{
    for (int i (0); i < MAX_TASKS_COUNT; ++i)
        if (!tasks[i].pid && tasks[i].args)
            launchNewTask(i);

    lastLaunch = time(NULL);
}

void addFailedLaunch(Task *task)
{
    task->fstarts[task->f] = time(NULL);
    ++task->f %= MAX_FAILED_LAUNCHES;
}

bool isTooMuchLaunches(Task *task)
{
    if (task->f != task->s)
        return false;

    ++task->s %= MAX_FAILED_LAUNCHES;
    return task->fstarts[task->f] - task->fstarts[task->s] <= 5;
}

void processTask(pid_t cpid, int exitStatus)
{
    for (int i = 0; i < MAX_TASKS_COUNT; ++i)
    {
        if (tasks[i].pid != cpid)
            continue;

        Task *task = &tasks[i];
        if (!task->isRespawn)
        {
            clearTask(i);
            syslog(LOG_INFO, "Child number %d pid %d finished", i, cpid);
            break;
        }

        if (WEXITSTATUS(exitStatus) == 1)
        {
            addFailedLaunch(task);

            if (isTooMuchLaunches(task))
            {
                syslog(LOG_ERR, "Task %d cannot be executed", i);
                launchNewTask(i, true);
                break;
            }
        }

        launchNewTask(i);
        break;
    }
}
//====================================================================
void mainLoop(char *confpath)
{
    while (activeTasksCount || configShouldBeUpdated)
    {
        if (configShouldBeUpdated)
        {
            updateConfig(confpath);
            configShouldBeUpdated = false;
            lastLaunch = 0;
        }

        if (time(NULL) - lastLaunch > 60)
            launchTasks();

        int status;
        pid_t cpid = waitpid(-1, &status, WNOHANG);
        if (!cpid || !WIFEXITED(status))
            continue;

        processTask(cpid, status);
    }
}
//====================================================================
int main(int argc, char *argv[])
{
    checkForUsageLaunch(argc, argv);
    becameDaemonOrDieTrying();
    setSighupHandler();
    mainLoop(argv[1]);

    closeAllAndExit(EXIT_SUCCESS);
}
//====================================================================
