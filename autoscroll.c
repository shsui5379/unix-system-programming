/**
 * @file      autoscroll.c
 * @brief     Displays text file content,
 *            autoscrolling one line once every defined time interval.
 * @details   Display on the terminal window the contents of a text file.
 *            By default, the content will autoscroll one line every second,
 *            unless a time interval was specified by the user.
 *            On the status bar, the current time (HH:MM:SS)
 *            and the start and end line numbers of current display is shown
 *
 *            CTRL-Z will pause the scrolling (but not the time)
 *            CTRL-C will resume the scrolling
 *            CTRL-\ or any terminating signals will clear the screen and exit
 *            Reaching the end of the file will also clear screen and exit.
 *
 *            Lines longer than terminal width will wrap,
 *            and only display if there's enough free line space.
 *
 * Usage:     $ autoscroll [-s secs] textfile
 *            where secs is a positive integer < 60
 *
 * Build with: gcc -o autoscroll autoscroll.c -lm
 */

#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <math.h> // must build with -lm
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ESC "\033"

#define USAGE \
    "Usage:\n\
$ %s [-s secs] textfile\n\
where secs is a positive integer < 60\n"

struct Line
{
    char *content;
    size_t length; // includes \n per getline()'s behavior
    struct Line *next;
};

int main(int argc, char *argv[])
{
    // -------------------------------- setup --------------------------------

    // mask
    sigset_t signal_mask;
    sigfillset(&signal_mask);

    // remove non terminating, non alarm, non ctrl-c/z
    sigdelset(&signal_mask, SIGCHLD);
    sigdelset(&signal_mask, SIGCLD);
    sigdelset(&signal_mask, SIGCONT);
    sigdelset(&signal_mask, SIGTTIN);
    sigdelset(&signal_mask, SIGTTOU);
    sigdelset(&signal_mask, SIGURG);
    sigdelset(&signal_mask, SIGWINCH);

    errno = 0;

    sigprocmask(SIG_SETMASK, &signal_mask, NULL);

    if (errno != 0)
    {
        perror("sigprocmask()");
        exit(EXIT_FAILURE);
    }

    // must be tty
    if (!isatty(STDIN_FILENO))
    {
        fprintf(stderr, "Not a terminal\n");
        exit(EXIT_FAILURE);
    }

    // ----------------------- command line processing -----------------------

    opterr = 0;            // turn off getopt()'s error messages
    char option;           // current option from getopt()
    bool s_option = false; // whether -s option was seen
    char *s_value = NULL;  // value of -s option

    while (true)
    {
        option = getopt(argc, argv, ":s:");
        if (option == -1)
            break; // end of options

        switch (option)
        {
        case 's':
            if (s_option)
            { // -s got redefined
                fprintf(stderr, "Duplicate values for -s\n" USAGE, argv[0]);
                exit(EXIT_FAILURE);
            }

            s_option = true;
            // save arg
            s_value = calloc(sizeof(char), strlen(optarg));
            if (s_value == NULL)
            {
                fprintf(stderr, "calloc(): failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }
            strncpy(s_value, optarg, strlen(optarg));
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

    // saving path
    if (argc == optind)
    { // file not provided
        fprintf(stderr, "File path not provided.\n" USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }

    char *file_path = NULL;
    file_path = calloc(sizeof(char), strlen(argv[optind]));
    if (file_path == NULL)
    {
        fprintf(stderr, "calloc(): failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    strncpy(file_path, argv[optind], strlen(argv[optind]));

    // extracting seconds
    int seconds = 1; // default

    if (s_option)
    { // -s
        errno = 0;

        char *end_ptr;
        seconds = strtol(s_value, &end_ptr, 0);

        if (0 != errno)
        {
            perror("strtol():");
            exit(EXIT_FAILURE);
        }

        if (*end_ptr != '\0')
        { // non-integer
            fprintf(stderr, "Non-integer seconds was supplied.\n" USAGE,
                    argv[0]);
            exit(EXIT_FAILURE);
        }

        if (seconds < 1 ||
            seconds > 59)
        { // not a positive integer less than 60
            fprintf(stderr,
                    "Seconds must be a positive integer less than 60.\n" USAGE,
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    free(s_value);
    s_value = NULL;

    // ----------------------------- file reading ----------------------------

    // file stream
    errno = 0;
    FILE *file_content = fopen(file_path, "r");
    if (errno != 0)
    {
        fprintf(stderr, "fopen()");
        exit(EXIT_FAILURE);
    }

    free(file_path);
    file_path = NULL;

    // a linked list of lines

    struct Line *head = NULL;
    struct Line *tail = NULL;

    char *content = NULL;
    size_t size = 0;
    ssize_t length = 0;

    errno = 0;

    while (-1 != (length = getline(&content, &size, file_content)))
    {
        struct Line *current_line = NULL;
        current_line = malloc(sizeof(struct Line));
        if (current_line == NULL)
        {
            fprintf(stderr, "malloc(): failed to allocate memory\n");
            exit(EXIT_FAILURE);
        }

        current_line->content = NULL;
        current_line->content = malloc(size);
        if (current_line == NULL)
        {
            fprintf(stderr, "malloc(): failed to allocate memory\n");
            exit(EXIT_FAILURE);
        }

        strncpy(current_line->content, content, size / sizeof(char));
        current_line->length = length;

        free(content);
        content = NULL;
        size = 0;

        // insertions

        if (head == NULL)
        { // first
            head = current_line;
            head->next = NULL;
        }
        else if (head != NULL && tail == NULL)
        { // second
            tail = current_line;
            head->next = tail;
            tail->next = NULL;
        }
        else
        {
            tail->next = current_line;
            tail = current_line;
            tail->next = NULL;
        }
    }

    if (errno != 0)
    { // EOF was not reason for -1 ret code
        perror("getline()");
        exit(EXIT_FAILURE);
    }

    // ----------------------------- tracking data ----------------------------

    // dimensions
    struct winsize terminal_dimensions; // .ws_row, .ws_col

    errno = 0;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_dimensions);
    if (errno != 0)
    {
        perror("ioctl()");
        exit(EXIT_FAILURE);
    }

    // print tracking
    int start_line_number = 1; // line number of first line on screen

    int lines_printed = 0; // count printed on screen in curent cycle

    int physical_lines_printed = 0; // count that accounts terminal
                                    // space taken by wrapped lines

    // If the number of lines in the file is R-1 or less, it terminates after
    // displaying these lines
    bool display_and_exit = false;

    // timing

    bool paused = false;          // hit by ctrl+z
    int time_to_scroll = seconds; // countdown for when to scroll

    // ------------------- wait for and respond to signals -------------------

    raise(SIGALRM); // trigger initial

    int signal_number;

    while (true)
    {
        errno = 0;
        sigwait(&signal_mask, &signal_number);
        if (errno != 0)
        {
            perror("sigwait()");
            exit(EXIT_FAILURE);
        }

        switch (signal_number)
        {
        case SIGALRM: // update the screen
            // wipe screen, history, and move to home
            printf(ESC "[2J" ESC "[3J" ESC "[H");
            fflush(stdout);

            if (!paused)
            { // not ctrl-z'ed
                time_to_scroll--;

                if (time_to_scroll == 0 && start_line_number > 1 ||
                    time_to_scroll == -1)
                { // account for decriment right
                  // on program start

                    if (display_and_exit)
                    { // file length <= R-1
                        raise(SIGQUIT);
                        break;
                    }

                    // scroll

                    struct Line *next_line = head->next;

                    free(head->content);
                    head->content = NULL;
                    free(head);
                    head = next_line;

                    // check that not at end of file
                    if (head == NULL)
                    {
                        raise(SIGQUIT);
                        break;
                    }

                    start_line_number++;

                    time_to_scroll = seconds; // reset countdown
                }
            }

            // print text

            struct Line *line_walker = head;

            while (physical_lines_printed <
                   terminal_dimensions.ws_row - 1)
            {
                // while there's space for more rows, print more

                // calculate how many rows next line will need
                int rows_needed = ceil((line_walker->length - 1.0) /
                                       terminal_dimensions.ws_col);

                if (line_walker->length == 1)
                {                    // empty line case
                    rows_needed = 1; // \n only
                }

                if (physical_lines_printed + rows_needed <=
                    terminal_dimensions.ws_row - 1)
                {
                    // if the line can fit, print it

                    printf("%s", line_walker->content);

                    // update tracking
                    lines_printed++;
                    physical_lines_printed += rows_needed;

                    line_walker = line_walker->next;

                    if (line_walker == NULL)
                    { // EOF
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if ((line_walker == NULL || (physical_lines_printed ==
                                         terminal_dimensions.ws_row - 1) &&
                                            line_walker->next == NULL) &&
                start_line_number == 1)
            {
                // case: file has R-1 or less lines, display and exit, per
                // specs
                display_and_exit = true;
            }

            // print status bar: time and line count

            // go to left most of bottom row
            printf(ESC "[%d;1f", terminal_dimensions.ws_row);
            fflush(stdout);

            // get time string

            errno = 0;
            time_t current_time = time(NULL);
            struct tm *current_time_struct = localtime(&current_time);

            if (current_time_struct == NULL)
            {
                perror("localtime()");
                exit(EXIT_FAILURE);
            }

            char *time_string = NULL; // HH:MM:SS\0

            time_string = calloc(sizeof(char), 9);
            if (time_string == NULL)
            {
                fprintf(stderr, "calloc(): failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }

            if (0 == strftime(time_string, 9, "%T", current_time_struct))
            {
                fprintf(stderr,
                        "strftime(): failed to format time string\n");
                exit(EXIT_FAILURE);
            }

            printf("%s Lines: %d-%d", time_string, start_line_number,
                   start_line_number + lines_printed - 1);

            // park cursor at (R, C - 2)

            printf(ESC "[%dG", terminal_dimensions.ws_col - 2);

            fflush(stdout);

            // reset for next iteration

            free(time_string);
            time_string = NULL;

            lines_printed = 0;
            physical_lines_printed = 0;
            alarm(1);

            break;

        case SIGTSTP: // ctrl-z: pause
            paused = true;
            break;

        case SIGINT: // ctrl-c: unpause
            paused = false;
            break;

        default: // terminating signal: clean up and close
            // wipe screen, history, and move to home
            printf(ESC "[2J" ESC "[3J" ESC "[H");

            errno = 0;
            fclose(file_content);
            if (errno != 0)
            {
                fprintf(stderr, "fclose(): error closing input file\n");
                exit(EXIT_FAILURE);
            }

            exit(EXIT_SUCCESS);
        }
    }

    return 0;
}