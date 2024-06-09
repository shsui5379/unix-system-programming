/**
 * Title:         basics.c
 * Description:   Prints information about the user's environment.
 * Purpose:       The first line will include the user's username, UID, and
 *                absolute path to home directory.
 *                The second line will include the path to the user's shell
 *                if the UID is even. Else, include the value of the DISPLAY
 *                env var.
 * Usage:         ./basics
 * Build with:    gcc -o basics basics.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
    int return_code;

    // to print: My username is <username>, my userid is <UID>, and my
    //           home directory is <path>.

    char *username = getenv("LOGNAME");
    const uid_t uid = getuid();
    char *home_directory_absolute_path = getenv("HOME");

    if (NULL == username)
        username = "NULL";
    if (NULL == home_directory_absolute_path)
        home_directory_absolute_path = "NULL";

    return_code = printf(
        "My username is %s, my userid is %d, and my home directory is %s.\n",
        username, uid, home_directory_absolute_path);

    if (return_code < 0)
    {
        printf("The call to printf() failed.\n");
        return 1;
    }

    // ---- line 2 ----

    if ((uid & 1) == 0)
    { // even UID
        // to print: My shell is <path>.
        char *shell_path = getenv("SHELL");

        if (NULL == shell_path)
            shell_path = "NULL";

        return_code = printf("My SHELL is %s.\n", shell_path);

        if (return_code < 0)
        {
            printf("The call to printf() failed.\n");
            return 1;
        }
    }
    else
    { // odd UID
        // to print: The value of my DISPLAY variable is <value>.
        char *display_value = getenv("DISPLAY");

        if (NULL == display_value)
            display_value = "NULL";

        return_code =
            printf("The value of my DISPLAY variable is %s.\n", display_value);

        if (return_code < 0)
        {
            printf("The call to printf() failed.\n");
            return 1;
        }
    }

    return 0;
}