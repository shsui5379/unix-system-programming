/**
 * Title:         datelist.c
 * Description:   Prints future dates/times on a regular schedule
 * Purpose:       User provides a time interval and an optional count number.
 *                For that count number many times (10 if not provided),
 *                starting from the current time, the provided time interval
 *                will be added on, with each iteration's result time printed,
 *                one per line.
 *                Each iteration's resulting time will include the date,
 *                and also include the time only if the interval involves
 *                units that are less than a day.
 *                Format of dates and times will follow locale settings.
 * Usage:         $ datelist [-c <count>] <schedule>
 *                Omitting the count implies a count of 10.
 *                The schedule is made of a number, space, time unit,
 *                multiple of these can be supplied, each space seperated.
 *                Time units cannot repeat.
 */

#define _XOPEN_SOURCE
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define USAGE \
    "Usage: \
%s [-c <count>] <schedule>\nWhere schedule consists of one or \
more of: <number> year[s] | month[s] | week[s] | day[s] | hour[s] | \
minute[s]"

int main(int argc, char *argv[])
{
    // --------------------------- localization -------------------------------

    if (NULL == setlocale(LC_TIME, ""))
    {
        fprintf(stderr, "Failed to set locale\n" USAGE "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // ----------------------- command option processing ----------------------

    opterr = 0;                   // turn off getopt()'s error messages
    char option;                  // current option found by getopt()
    char *c_value = NULL;         // value for the c option
    bool c_value_defined = false; // whether c is defined

    while (true)
    {
        option = getopt(argc, argv, ":c:"); // get option
        if (-1 == option)
            break; // reached end of options

        switch (option)
        {
        case 'c': // count
            c_value_defined = true;
            int string_length = strlen(optarg);
            c_value = malloc(string_length * sizeof(char));
            if (NULL == c_value)
            {
                fprintf(stderr, "malloc(): failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }
            strncpy(c_value, optarg, string_length);
            break;
        case '?': // unknown option
            fprintf(stderr, "Unknown option: %c\n" USAGE "\n", optopt,
                    argv[0]);
            exit(EXIT_FAILURE);
            break;
        case ':': // missing arg
            fprintf(stderr, "Missing argument for %c\n" USAGE "\n", optopt,
                    argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    // extracting count

    long count;

    if (c_value_defined)
    {
        errno = 0;

        char *end_ptr;
        long number = strtol(c_value, &end_ptr, 0);

        if (0 != errno)
        {
            perror("error calling strtol:");
            exit(EXIT_FAILURE);
        }

        if (*end_ptr != '\0')
        { // non number
            fprintf(stderr, "A non-number was supplied.\n" USAGE "\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        if (number < 0)
        { // negative
            fprintf(stderr, "Negative count was supplied\n" USAGE "\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }

        count = number;
    }
    else
    { // default = 10
        count = 10;
    }

    free(c_value);

    // ------------------------- processing schedule -------------------------

    if (optind >= argc)
    { // schedule missing
        fprintf(stderr, "Missing schedule\n" USAGE "\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct tm time_adjustment = {0};
    bool date_portion_only = true;             // will set to false if to modify h/m/s
    bool units_seen[] = {0, 0, 0, 0, 0, 0, 0}; // y, m, w, d, h, m, s

    char *token = strtok(argv[optind], " \t");

    while (NULL != token)
    {
        // getting number

        errno = 0;

        char *end_ptr;
        long number = strtol(token, &end_ptr, 0);

        if (0 != errno)
        {
            perror("error calling strtol:");
            exit(EXIT_FAILURE);
        }

        if (*end_ptr != '\0')
        { // non number
            fprintf(stderr, "A non-number was supplied.\n" USAGE "\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        if (number < 0)
        { // negative
            fprintf(stderr, "Negative time was supplied\n" USAGE "\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        if (number > __INT_MAX__)
        { // out of int range
            fprintf(stderr, "Supplied number is out of range\n" USAGE "\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }

        // setting up time_adjustment

        token = strtok(NULL, " \t");

        if (NULL == token)
        { // missing units
            fprintf(stderr, "Missing time units.\n" USAGE "\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        if (strcmp(token, "year") == 0 || strcmp(token, "years") == 0)
        {
            if (units_seen[0])
            {
                fprintf(stderr, "Supplied year multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[0] = true;
            }
            time_adjustment.tm_year += number;
        }
        else if (strcmp(token, "month") == 0 ||
                 strcmp(token, "months") == 0)
        {
            if (units_seen[1])
            {
                fprintf(stderr, "Supplied month multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[1] = true;
            }
            time_adjustment.tm_mon += number;
        }
        else if (strcmp(token, "week") == 0 || strcmp(token, "weeks") == 0)
        {
            if (units_seen[2])
            {
                fprintf(stderr, "Supplied week multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[2] = true;
            }
            time_adjustment.tm_mday += 7 * number;
        }
        else if (strcmp(token, "day") == 0 || strcmp(token, "days") == 0)
        {
            if (units_seen[3])
            {
                fprintf(stderr, "Supplied day multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[3] = true;
            }
            time_adjustment.tm_mday += number;
        }
        else if (strcmp(token, "hour") == 0 || strcmp(token, "hours") == 0)
        {
            if (units_seen[4])
            {
                fprintf(stderr, "Supplied hour multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[4] = true;
            }
            time_adjustment.tm_hour += number;
            date_portion_only = false;
        }
        else if (strcmp(token, "minute") == 0 ||
                 strcmp(token, "minutes") == 0)
        {
            if (units_seen[5])
            {
                fprintf(stderr, "Supplied minute multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[5] = true;
            }
            time_adjustment.tm_min += number;
            date_portion_only = false;
        }
        else if (strcmp(token, "second") == 0 ||
                 strcmp(token, "seconds") == 0)
        {
            if (units_seen[6])
            {
                fprintf(stderr, "Supplied second multiple times\n" USAGE "\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                units_seen[6] = true;
            }
            time_adjustment.tm_sec += number;
            date_portion_only = false;
        }
        else
        {
            fprintf(stderr, "Invalid unit supplied\n" USAGE "\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, " \t");
    }

    // ---------------------- printing count # of times ----------------------

    // get current time

    errno = 0;
    time_t time_now = time(NULL);
    struct tm *current_time = localtime(&time_now);
    if (NULL == current_time)
    {
        perror("Error with localtime():");
        exit(EXIT_FAILURE);
    }

    // add, format, and print

    char date_string[1024];
    char *date_format;
    if (date_portion_only)
    {
        date_format = "%x";
    }
    else
    {
        date_format = "%x %X";
    }

    for (long i = 0; i < count; i++)
    {
        // add
        current_time->tm_year += time_adjustment.tm_year;
        current_time->tm_mon += time_adjustment.tm_mon;
        current_time->tm_mday += time_adjustment.tm_mday;
        current_time->tm_hour += time_adjustment.tm_hour;
        current_time->tm_min += time_adjustment.tm_min;
        current_time->tm_sec += time_adjustment.tm_sec;

        errno = 0;
        mktime(current_time);
        if (errno != 0)
        {
            perror("error with mktime() after adding time adjustment: ");
            exit(EXIT_FAILURE);
        }

        // formatting

        if (0 == strftime(date_string, sizeof(date_string), date_format,
                          current_time))
        {
            fprintf(stderr, "Failed to format date-time string\n" USAGE "\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }

        // print
        printf("%s\n", date_string);
    }

    return 0;
}