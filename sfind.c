/**
 * Title:         sfind.c
 * Description:   List files satisfying test from given directory hierarchy
 * Purpose:       Searches through given directories (CWD if omitted)
 *                and prints out relative paths to files that passes given test
 *                See below (Usage) for possible tests
 * Usage:         $ sfind [dir1 dir2 ...] [test]
 *                Can omit dirs to imply current working directory
 *                Tests (only one per command):
 *                -s filename: match if file is a hardlink to `filename`
 *                -m fileglob: match if file's name matches `fileglob`
 * Build with:    gcc -o sfind sfind.c
 */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fnmatch.h>
#include <ftw.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define USAGE \
    "Usage:\n\
$ %s [dir1 dir2 ...] [test]\n\
Can omit dirs to imply current working directory\n\
Tests (only one per command):\n\
-s filename: match if file is a hardlink to `filename`\n\
-m fileglob: match if file's name matches `fileglob`\n"

static ino_t target_inode;          // for -s test matching
static char *target_pattern = NULL; // for -m test matching

// --------------------------------- tests ----------------------------------

/**
 * Callback function for nftw.
 * Prints out relative pathnames of entries with the same inode number as
 * target_inode. (-s test)
 * @pre Assumes nftw will not cross mountpoints.
 * @param fpath Path of current entry
 * @param sb stat structure of current entry
 * @param typeflag Type of file of the current entry
 * @param ftwbuf Basename offset and tree level info
 * @returns 0 to continue walk, non-zero to end walk
 */
int hardlink_test(const char *fpath, const struct stat *sb, int typeflag,
                  struct FTW *ftwbuf)
{
    // if stat data is valid and inode number matches target, print fpath
    if (typeflag != FTW_NS && sb->st_ino == target_inode)
    {
        printf("%s\n", fpath);
    }

    return 0; // nftw: continue walk
}

/**
 * Callback function for nftw.
 * Prints out relative pathnames of entries with a basename that matches
 * target_pattern by fnmatch(3). (-m test)
 * @param fpath Path of current entry
 * @param sb stat structure of current entry
 * @param typeflag Type of file of the current entry
 * @param ftwbuf Basename offset and tree level info
 * @returns 0 to continue walk, non-zero to end walk
 */
int basename_test(const char *fpath, const struct stat *sb, int typeflag,
                  struct FTW *ftwbuf)
{
    // print if basename match pattern via fnmatch(3)
    if (0 == fnmatch(target_pattern, fpath + ftwbuf->base, 0))
    {
        printf("%s\n", fpath);
    }

    return 0; // nftw: continue walk
}

int main(int argc, char *argv[])
{
    // ----------------------- command line processing -----------------------

    opterr = 0;                 // turn off getopt()'s error messages
    char option;                // current option from getopt()
    bool s_test = false;        // -s option
    char *s_arg = NULL;         // -s argument
    bool m_test = false;        // -m option
    char *m_arg = NULL;         // -m argument
    bool supplied_dirs = false; // whether directories were supplied

    while (true)
    {
        option = getopt(argc, argv, ":s:m:");
        if (option == -1)
            break; // reached end of options

        switch (option)
        {
        case 's':
            s_test = true;
            // save arg
            s_arg = calloc(sizeof(char), strlen(optarg));
            if (s_arg == NULL)
            {
                fprintf(stderr, "calloc(): failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }
            strncpy(s_arg, optarg, strlen(optarg));
            break;
        case 'm':
            m_test = true;
            // save arg
            m_arg = calloc(sizeof(char), strlen(optarg));
            if (m_arg == NULL)
            {
                fprintf(stderr, "calloc(): failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }
            strncpy(m_arg, optarg, strlen(optarg));
            break;
        case ':': // missing argument
            fprintf(stderr, "Missing argument for %c\n" USAGE, optopt,
                    argv[0]);
            exit(EXIT_FAILURE);
            break;
        case '?': // unknown option
            fprintf(stderr, "Unknown option %c\n" USAGE, optopt, argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    // must have exactly one test
    if (!s_test && !m_test || s_test && m_test)
    {
        fprintf(stderr, "Must provide exactly one test\n" USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }

    supplied_dirs = optind < argc; // true if directories were supplied

    // ----------------------- walking the directories -----------------------

    // setup
    if (s_test)
    { // -s: hardlink test
        // get inode number to match
        struct stat stat_result;
        if (-1 == lstat(s_arg, &stat_result))
        {
            perror("lstat()");
            exit(EXIT_FAILURE);
        }
        target_inode = stat_result.st_ino;
        free(s_arg);
        s_arg = NULL;
    }
    else if (m_test)
    { // -m: basename test
        target_pattern = m_arg;
    }

    // walk
    do
    {
        if (s_test)
        { // -s: hardlink test
            // don't follow symlinks
            // don't cross mountpoints
            // default to `.` if no directories supplied
            if (0 != nftw((supplied_dirs) ? argv[optind] : ".", hardlink_test,
                          20, FTW_MOUNT | FTW_PHYS))
            {
                perror("nftw()");
                exit(EXIT_FAILURE);
            }
        }
        else if (m_test)
        { // -m: basename test
            // don't follow symlinks
            // default to `.` if no directories supplied
            if (0 != nftw((supplied_dirs) ? argv[optind] : ".", basename_test,
                          20, FTW_PHYS))
            {
                perror("nftw()");
                exit(EXIT_FAILURE);
            }
        }

        optind++;
    } while (optind < argc);

    if (m_test)
    {
        free(m_arg);
        m_arg = NULL;
        target_pattern = NULL;
    }

    return 0;
}