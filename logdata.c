/**
 * Title:         logdata.c
 * Description:   Reports users' total login time since record-keeping started
 * Purpose:       For each provided username (or just current user if omitted),
 *                print the total time spent logged in since records started.
 *                One per line. Time units pluralized where needed.
 *                Units with zero value are omitted. Usernames without a record
 *                get 0 seconds. If record-keeping file not found, notify user.
 *                Login sessions that are still in progress are excluded.
 * Usage:         $ logdata [-a] [-s] [-f <file>] [[username] ...]
 *                -a to print times for all users present in the WTMP file
 *                -s to print the combined login time of all users printed
 *                -f <file> to specify a path to log file other then _PATH_WTMP
 *                List usernames of users to query the total login time for.
 *                Omit to query only the current user.
 * Build with:    gcc -o logdata logdata.c
 */

#define USAGE \
    "Usage:\n\
$ %s [-a] [-s] [-f <file>] [[username] ...]\n\
-a to print times for all users present in the WTMP file\n\
-s to print the combined login time of all users printed\n\
-f <file> to specify a path to log file other then _PATH_WTMP\n\
List usernames of users to query the total login time for.\n\
Omit to query only the current user.\n"

#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#define UT_USER_LENGTH 32

struct line
{
    struct line *next;
    char *name;
    time_t start;
};

struct user
{
    char *username;
    time_t total_time; // in seconds
    struct user *next;
    struct line *lines;
};

int main(int argc, char *argv[])
{
    // ---------------------- command option parsing -------------------------

    opterr = 0;                    // turn off getopt()'s error messages
    char option;                   // current option from getopt()
    bool all_users = false;        // -a option
    bool sum_totals = false;       // -s option
    bool custom_file = false;      // -f option
    char *custom_file_path = NULL; // -f argument

    while (true)
    {
        option = getopt(argc, argv, ":asf:");
        if (-1 == option)
            break; // reached end of options

        switch (option)
        {
        case 'a':
            all_users = true;
            break;
        case 's':
            sum_totals = true;
            break;
        case 'f':
            custom_file = true;
            custom_file_path = malloc(sizeof(char) * strlen(optarg));
            if (custom_file_path == NULL)
            {
                fprintf(stderr, "malloc() couldn't allocate memory\n");
                exit(EXIT_FAILURE);
            }
            strncpy(custom_file_path, optarg, strlen(optarg));
            break;
        case ':': // missing argument
            fprintf(stderr, "Missing argument for %c\n" USAGE, optopt,
                    argv[0]);
            exit(EXIT_FAILURE);
            break;
        case '?': // unknown option
            fprintf(stderr, "Unknown option: %c\n" USAGE, optopt, argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    bool usernames_supplied =
        optind < argc; // false indicates to query only the current user

    // ---------------------------- file processing ---------------------------

    /**
     * Use getutxent(3) to read the data structures
     * Keep track of users on a linked list
     *     Tracking: username, total time, and a linked list of lines
     *         Lines linked list tracking line name, login time stamp
     * On:
     *     login:    find the username
     *               insert node representing the line
     *     logout:   find the line node
     *               update user's total time
     *               delete line node
     *     shutdown: go through list
     *               close out all the lines (update total & delete)
     *
     * If -a is not set: user list will be predefined
     * If -a is set:     user list will be built as record file is processed
     */

    // setting which record file to read
    char *wtmp_path;
    if (custom_file)
    {
        wtmp_path = custom_file_path;
    }
    else
    {
        wtmp_path = _PATH_WTMPX;
    }

    // opening file
    if (utmpxname(wtmp_path) == -1)
    {
        fprintf(stderr, "utmpxname(): Error setting path to record file\n");
    }
    setutxent();

    free(custom_file_path);
    custom_file_path = NULL;

    // setting up user list

    struct user *user_list = NULL;

    if (!all_users)
    { // predefining list of users
        if (usernames_supplied)
        { // usernames given in command line
            for (int i = optind; i < argc; i++)
            {
                struct user *new_user = malloc(sizeof(struct user));
                if (new_user == NULL)
                {
                    fprintf(stderr, "malloc(): error allocating memory\n");
                    exit(EXIT_FAILURE);
                }

                // set up user

                new_user->lines = NULL;
                new_user->total_time = 0;
                new_user->username = calloc(sizeof(char), strlen(argv[i]));

                if (new_user->username == NULL)
                {
                    fprintf(stderr, "calloc(): error allocating memory\n");
                    exit(EXIT_FAILURE);
                }
                strncpy(new_user->username, argv[i], strlen(argv[i]));

                // insert user

                new_user->next = user_list;
                user_list = new_user;
            }
        }
        else
        { // query only current user
            // set up user

            struct user *new_user = malloc(sizeof(struct user));

            new_user->lines = NULL;
            new_user->total_time = 0;

            const char *username = getenv("LOGNAME");
            if (username == NULL)
            {
                fprintf(stderr, "getenv(): error getting username\n");
                exit(EXIT_FAILURE);
            }

            new_user->username = calloc(sizeof(char), strlen(username));
            strncpy(new_user->username, username, strlen(username));

            // insert user

            new_user->next = user_list;
            user_list = new_user;
        }
    } // else user list will be built as it goes through the records file

    // data processing

    errno = 0;
    struct utmpx *utmp_entry = getutxent();
    if (errno != 0)
    {
        perror(
            "getutxent(): error getting entries. Record-keeping file might not "
            "exist?");
        exit(EXIT_FAILURE);
    }

    while (utmp_entry != NULL)
    {
        // shutdowns:  line is ~ and username is shutdown

        // case: shutdown or reboot
        if (((strncmp(utmp_entry->ut_line, "~", 1) == 0) &&
             (strncmp(utmp_entry->ut_user, "shutdown", 8)) == 0) ||
            (utmp_entry->ut_type == BOOT_TIME))
        { // close out all active lines
            struct user *current_user = user_list;

            while (current_user != NULL)
            {
                struct line *current_line = current_user->lines;

                while (current_line != NULL)
                {
                    // update total time
                    current_user->total_time +=
                        utmp_entry->ut_tv.tv_sec - current_line->start;

                    // delete line
                    free(current_line->name);
                    current_line->name = NULL;
                    struct line *next_line = current_line->next;
                    free(current_line);
                    current_line = NULL;

                    // walk
                    current_line = next_line;
                    current_user->lines = current_line;
                }

                current_user = current_user->next;
            }
        }
        else if (utmp_entry->ut_type == USER_PROCESS)
        { // login - save line
            bool found_user = false;
            struct user *target_user = NULL;

            struct user *current_user = user_list;

            while (current_user != NULL)
            {
                if (strncmp(current_user->username, utmp_entry->ut_user,
                            sizeof(current_user->username)) == 0)
                {
                    // found target
                    target_user = current_user;
                    found_user = true;
                    break;
                }

                current_user = current_user->next;
            }

            if (!found_user && all_users)
            { // create new user if -a flag set
                target_user = malloc(sizeof(struct user));
                if (target_user == NULL)
                {
                    fprintf(stderr, "malloc(): failed to allocate memory\n");
                    exit(EXIT_FAILURE);
                }

                // set up user
                target_user->lines = NULL;
                target_user->total_time = 0;
                target_user->username = calloc(sizeof(char), UT_USER_LENGTH);
                if (target_user->username == NULL)
                {
                    fprintf(stderr, "calloc(): failed to allocate memory\n");
                    exit(EXIT_FAILURE);
                }
                strncpy(target_user->username, utmp_entry->ut_user,
                        UT_USER_LENGTH);

                // insert user
                target_user->next = user_list;
                user_list = target_user;
            }

            if (found_user || all_users)
            { // tracked users only
                // saving line
                struct line *new_line = malloc(sizeof(struct line));
                if (new_line == NULL)
                {
                    fprintf(stderr, "malloc(): failed to allocate memory\n");
                    exit(EXIT_FAILURE);
                }

                // set up line
                new_line->name = calloc(sizeof(char), UT_USER_LENGTH);
                if (new_line->name == NULL)
                {
                    fprintf(stderr, "calloc(): failed to allocate memory\n");
                }
                strncpy(new_line->name, utmp_entry->ut_line, UT_USER_LENGTH);
                new_line->start = utmp_entry->ut_tv.tv_sec;

                // insert line
                new_line->next = target_user->lines;
                target_user->lines = new_line;
            }
        }
        else if (utmp_entry->ut_type == DEAD_PROCESS)
        { // logout
            // find line and update user

            // skip if there's no line
            if (utmp_entry->ut_line[0] != 0)
            {
                // find corresponding login
                struct user *target_user = NULL;
                struct line *target_line = NULL;
                struct line *prev_line = NULL;
                bool line_found = false;

                struct user *current_user = user_list;

                while (current_user != NULL)
                {
                    struct line *current_line = current_user->lines;
                    struct line *trailer = NULL;

                    while (current_line != NULL)
                    {
                        if (strncmp(current_line->name, utmp_entry->ut_line,
                                    strlen(current_line->name)) == 0)
                        {
                            // line found

                            target_line = current_line;
                            prev_line = trailer;
                            target_user = current_user;
                            line_found = true;
                            break;
                        }

                        trailer = current_line;
                        current_line = current_line->next;
                    }

                    if (line_found)
                        break;

                    current_user = current_user->next;
                }

                if (line_found)
                { // only if record is for a tracked user
                    // update user

                    target_user->total_time +=
                        utmp_entry->ut_tv.tv_sec - target_line->start;

                    // delete line

                    free(target_line->name);
                    target_line->name = NULL;

                    if (prev_line == NULL)
                    { // deleting head
                        target_user->lines = target_line->next;
                    }
                    else
                    {
                        prev_line->next = target_line->next;
                    }

                    free(target_line);
                    target_line = NULL;
                }
            }
        }

        errno = 0;
        utmp_entry = getutxent();
        if (errno != 0)
        {
            perror("getutxent(): error getting next entry:");
            exit(EXIT_FAILURE);
        }
    }

    endutxent();

    // delete any leftover lines
    // those would be the sessions still in progress
    struct user *current_user = user_list;

    while (current_user != NULL)
    {
        struct line *current_line = current_user->lines;

        while (current_line != NULL)
        {
            current_user->lines = current_line->next;
            free(current_line->name);
            current_line->name = NULL;
            free(current_line);
            current_line = NULL;
        }

        current_user = current_user->next;
    }

    // ----------------------- formatting & printing -------------------------

    int sum = 0;

    current_user = user_list;

    while (current_user != NULL)
    {
        if (sum_totals)
        { // -s option
            sum += current_user->total_time;
        }

        // calculate day, hour, minute, second
        int seconds = current_user->total_time;
        int minutes = (seconds / 60) % 60;
        int hours = (seconds / 3600) % 24;
        int days = seconds / 86400;

        seconds -= 60 * minutes + 3600 * hours + 86400 * days;

        // print
        printf("%-32s ", current_user->username);

        if (days != 0)
        {
            printf("%5d day%c  ", days, (days == 1) ? ' ' : 's');
        }

        if (hours != 0)
        {
            printf("%5d hour%c ", hours, (hours == 1) ? ' ' : 's');
        }

        if (minutes != 0)
        {
            printf("%5d min%c  ", minutes, (minutes == 1) ? ' ' : 's');
        }

        if (seconds != 0 || (days == 0 && hours == 0 && minutes == 0))
        {
            printf("%5d sec%c\n", seconds, (seconds == 1) ? ' ' : 's');
        }

        // free and walk
        user_list = current_user->next;
        free(current_user->username);
        current_user->username = NULL;
        free(current_user);
        current_user = user_list;
    }

    if (sum_totals)
    { // -s flag
        int seconds = sum;
        int minutes = (seconds / 60) % 60;
        int hours = (seconds / 3600) % 24;
        int days = seconds / 86400;

        seconds -= 60 * minutes + 3600 * hours + 86400 * days;

        printf("%-32s ", "TOTAL:");

        if (days != 0)
        {
            printf("%5d day%c  ", days, (days == 1) ? ' ' : 's');
        }

        if (hours != 0)
        {
            printf("%5d hour%c ", hours, (hours == 1) ? ' ' : 's');
        }

        if (minutes != 0)
        {
            printf("%5d min%c  ", minutes, (minutes == 1) ? ' ' : 's');
        }

        if (seconds != 0 || (days == 0 && hours == 0 && minutes == 0))
        {
            printf("%5d sec%c\n", seconds, (seconds == 1) ? ' ' : 's');
        }
    }

    return 0;
}