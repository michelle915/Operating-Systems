// Previously attempted course in Fall 2023.

#define _POSIX_C_SOURCE 200809L   // Enables Portable Operating System Interface (POSIX) features, ensuring code can use POSIX system interfaces
#define _XOPEN_SOURCE 700         // POSIX extension. 

/* HEADER FILES: These provide function prototypes, macros, and type definitions that the program will use.*/
#include <dirent.h>           // Provides functions and a structure to access directory entries
#include <err.h>              // Provides functions for reporting errors.
#include <errno.h>            // Defines the integer variable errno, which is set by system calls and some library functions in the event of an error to indicate what went wrong. 
#include <fcntl.h>            // Provides functions and macros to handle files, such as opening, closing, and manipulating file descriptors.
#include <grp.h>              // Provides functions and a structure to retrieve group details.
#include <limits.h>           // Defines several properties of various variable types, such as the maximum value that can be held by an integer type.
#include <pwd.h>              // Provides functions and a structure to retrieve user account details.
#include <stdbool.h>          // Defines a Boolean data type
#include <stdint.h>           // Defines exact-width integer types, like int8_t, int16_t, 
#include <stdio.h>            // Provides functionalities for input/output stream operations
#include <stdlib.h>           // Provides general-purpose functions, including dynamic memory management, random number generation, and process control.
#include <string.h>           // Provides functions to manipulate and inspect string data
#include <sys/stat.h>         // Provides a structure (struct stat) for retrieving file attributes, and functions to retrieve those attributes.
#include <sys/types.h>        // Prereq for sys/stat.h. Defines a collection of data types used in system calls, primarily associated with file I/O.
#include <time.h>             // Provides functions and types to handle time.
#include <unistd.h>           // Provides access to the POSIX operating system API.

/* LOCAL HEADER FILES: Includes definitions from files in local directory.*/
#include "libtree.h"          // Provides 'tree_print' function

/* HELPER MACROS */
#define arrlen(a) (sizeof(a) / sizeof *(a))   // Gets the length of an array (number of elements)

/* CONDITIONAL HELPER MACRO for DEBUG PRINT
 * dprintf(...) can be used like printf to print diagnostic messages in the debug build. Does
 * nothing in release. This is how debugging with print statements is done -- conditional
 * compilation determined by a compile-time DEBUG macro. */
#ifdef DEBUG
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...) ((void)0)
#endif

/* STRUCTURE DEFINITIONS */
struct fileinfo {             // This structure encapsulates information about a file--path and attributes.
  char *path;
  struct stat st;
};

/* NOTE: Notice how all of these functions and file-scope identifiers are declared static. This
 * means they have no linkage. You should read the C language reference documents and the difference
 * between scope, linkage, and lifetime.
 */

/* HELPER FUNCTIONS */
/* A few helper functions to break up the program */
static int print_path_info(struct fileinfo finfo);                                        // Prints formatted file information
static char *mode_string(mode_t mode);                                                    // Converts a file's mode (its permissions and type) into a string

/* These functions are used to get a list of files in a directory and sort them */
static int read_file_list(DIR *dirp, struct fileinfo **file_list, size_t *file_count);    // Reads and lists all files in a directory.
static void free_file_list(struct fileinfo **file_list, size_t file_count);               // Frees memory for the file list.
static int filecmp(void const *lhs, void const *rhs);                                     // Compares files for sorting.

/* GLOBAL VARIABLES (AKA file-scoped objects) */
static int depth;                                                                         // Keeps track of the depth of recursion
static struct tree_options opts;                                                          // Stores options that determine tree printing
static int cur_dir = AT_FDCWD;                                                            // Stores the current directory's file descriptor (AT_FDCWD = At File Descriptor Current Working Directory)

/* MAIN FUNCTIONS
 * FORWARD DECLARATIONS: inform the compiler about the function signature before use */
extern int tree_print(char const *path, struct tree_options opts);                // externally linked function, accessible to users of the library
static int tree_print_recurse(struct fileinfo finfo);                             // internal recursive function

/* Simply sets up the initial recursion. Nothing for you to change here. */
extern int
tree_print(char const *path, struct tree_options _opts)
{
  opts = _opts;                                                                     // Set options
  depth = 0;
  struct fileinfo finfo;
  if ((finfo.path = strdup(path)) == NULL) goto exit;                               // if path is null, exit
  if (fstatat(cur_dir, path, &(finfo.st), AT_SYMLINK_NOFOLLOW) == -1) goto exit;    // if retrieving file information fails, exit
  if (tree_print_recurse(finfo) == -1) goto exit;                                   // if tree_print_recurse function fails, exit
exit:
  free(finfo.path);
  return errno ? -1 : 0;                                                            // -1 = error; 0 = no error
}

/* TODO: START HERE */
static int
tree_print_recurse(struct fileinfo finfo)
{
  int dir = -1, sav_dir = cur_dir;
  DIR *dirp = NULL;
  struct fileinfo *file_list = NULL;
  size_t file_count = 0;

  errno = 0;

  /* TODO: implement dirsonly (directories, no files) functionality here */
  if (opts.dirsonly && !S_ISDIR(finfo.st.st_mode)) goto exit;                       // if directories-only ("d") elected AND item is not a directory, exit 

  /* TODO: print indentation */
  for (int i = 0; i < opts.indent * depth; i++) putchar(' ');

  /* TODO: print the path info */
  if (print_path_info(finfo) == -1) goto exit;                                      // if print_path_info function fails, exit
  if (!S_ISDIR(finfo.st.st_mode)) {                                                 // if item is file, print new line
    if (putchar('\n') == EOF) goto exit; 
  }

  /* TODO: continue ONLY if path is a directory */
  if (!S_ISDIR(finfo.st.st_mode)) goto exit;                                        // if item is not directory, exit 

  if ((dir = openat(cur_dir, finfo.path, O_RDONLY | O_CLOEXEC)) == -1 ||            // if opening directory is unsuccessful OR file descriptor cannot be converted to directory stream, ... exit
      (dirp = fdopendir(dir)) == NULL) {
    if (errno == EACCES) {                                                          // if error is due to a permission issue (EACCES), ...
      errno = 0; /* not an error, so reset errno! */
      printf(" [could not open directory %s]\n", finfo.path);
    }
    goto exit;
  }

  cur_dir = dir;

  if (read_file_list(dirp, &file_list, &file_count) == -1) {                // if read_file_list (reads and lists all files in a directory) returns error, exit
    if (errno == EACCES) {                                                  // if error is due to a permission issue (EACCES), ...
      errno = 0; /* not an error, so reset errno! */ 
      printf(" [could not open directory %s]\n", finfo.path);
    }
    goto exit;
  }

  if (putchar('\n') == EOF) goto exit;                                      // if error with new line, exit

  /* See QSORT(3) for info about this function. It's not super important. It just sorts the list of
   * files using the filesort() function, which is the part you need to finish. */
  qsort(file_list, file_count, sizeof *file_list, filecmp);

  ++depth;
  for (size_t i = 0; i < file_count; ++i) {
    if (tree_print_recurse(file_list[i]) == -1) goto exit; /*  Recurse */
  }
  --depth;

exit:;
  /* TODO: Free any allocated resources.
   * Hint: look for realloc, malloc, and calloc calls for memory allocation
   *       look for open*() function calls for file related allocations
   */
  if (dirp) closedir(dirp);                                                 // If directory stream pointer is not null, free directory stream pointer
  if (file_list) free_file_list(&file_list, file_count);                    // If file_list is not null, free dynamically allocated file list (array of fileinfo objects)                 

  cur_dir = sav_dir;
  return errno ? -1 : 0;
}

/**
 * @brief Helper function that prints formatted output of the modestring, username, groupname, file
 * size, and link target (for links).
 */
static int
print_path_info(struct fileinfo finfo)
{
  char sep = '[';
  if (opts.perms) {
    if (printf("%c%s", sep, mode_string(finfo.st.st_mode)) < 0) goto exit;    /* TODO: Print permission info */
    sep = ' ';
  }
  if (opts.user) {
    struct passwd *pwd = getpwuid(finfo.st.st_uid);                           // on Unix, user information has been traditionally stored in the passwd file
    if (pwd && printf("%c%s", sep, pwd->pw_name) < 0) goto exit;              /* TODO */             
    sep = ' ';
  }
  if (opts.group) {
    struct group *grp = getgrgid(finfo.st.st_gid);
    if (grp && printf("%c%s", sep, grp->gr_name) < 0) goto exit;              /* TODO */
    sep = ' ';
  }
  if (opts.size) {
    if (printf("%c%jd", sep, (intmax_t)finfo.st.st_size) < 0) goto exit;      /* TODO */
    sep = ' ';
  }
  if (sep != '[')
    if (printf("] ") < 0) goto exit;
  if (printf("%s", finfo.path) < 0) goto exit;
  if (S_ISLNK(finfo.st.st_mode)) {
    char rp[PATH_MAX + 1] = {0};
    if (readlinkat(cur_dir, finfo.path, rp, PATH_MAX) == -1) goto exit;
    if (printf(" -> %s", rp) < 0) goto exit;
  }
exit:
  return errno ? -1 : 0;
}

/**
 * @brief File comparison function, used by qsort
 */
static int
filecmp(void const *_lhs, void const *_rhs)
{
  struct fileinfo const *lhs = _lhs, *rhs = _rhs;
  struct timespec const lt = lhs->st.st_mtim, rt = rhs->st.st_mtim;
  int retval = 0;
  switch (opts.sort) {
    case NONE:
      retval = 0; /*  Well that was easy */
      break;
    case ALPHA:
      retval = strcoll(lhs->path, rhs->path);
      break;                                                                  /* TODO */
    case RALPHA:
      retval = strcoll(rhs->path, lhs->path);
      break;
    case TIME:
      /*  I did this one for you :) */
      if (rt.tv_sec != lt.tv_sec) {
        retval = rt.tv_sec - lt.tv_sec;
      } else {
        retval = rt.tv_nsec - lt.tv_nsec;
      }
      break;
  }
  return retval;
}

/**
 * @brief Reads all files in a directory and populates a fileinfo array
 */
static int
read_file_list(DIR *dirp, struct fileinfo **file_list, size_t *file_count)
{
  for (;;) {
    errno = 0;
    struct dirent *de = readdir(dirp);
    if (de == NULL) break;

    /* Skip the "." and ".." subdirectories */
    if (strcoll(de->d_name, ".") == 0 || strcoll(de->d_name, "..") == 0) continue;

    /* TODO: Skip hidden files? */
    if (!opts.all && de->d_name[0] == '.') continue;

    ++(*file_count);
    (*file_list) = realloc((*file_list), sizeof *(*file_list) * (*file_count));
    (*file_list)[(*file_count) - 1].path = strdup(de->d_name);
    if (fstatat(cur_dir, de->d_name, &(*file_list)[(*file_count) - 1].st, AT_SYMLINK_NOFOLLOW) ==
        -1)
      break;
  }
  return errno ? -1 : 0;
}

/**
 * @brief Frees dynamically allocated file list (array of fileinfo objects)
 */
static void
free_file_list(struct fileinfo **file_list, size_t file_count)
{
  for (size_t i = 0; i < file_count; ++i) {
    free((*file_list)[i].path);
  }
  free(*file_list);
}

/**
 * @brief Returns a 9-character modestring for the given mode argument.
 */
static char *
mode_string(mode_t mode)
{
  static char str[11];
  if (S_ISREG(mode))
    str[0] = '-';
  else if (S_ISDIR(mode))
    str[0] = 'd';
  else if (S_ISBLK(mode))
    str[0] = 'b';
  else if (S_ISCHR(mode))
    str[0] = 'c';
  else if (S_ISLNK(mode))
    str[0] = 'l';
  else if (S_ISFIFO(mode))
    str[0] = 'p';
  else if (S_ISSOCK(mode))
    str[0] = 's';
  else
    str[0] = '.';
  str[1] = mode & S_IRUSR ? 'r' : '-';
  str[2] = mode & S_IWUSR ? 'w' : '-';
  str[3] = (mode & S_ISUID ? (mode & S_IXUSR ? 's' : 'S') : (mode & S_IXUSR ? 'x' : '-'));
  str[4] = mode & S_IRGRP ? 'r' : '-';
  str[5] = mode & S_IWGRP ? 'w' : '-';
  str[6] = (mode & S_ISGID ? (mode & S_IXGRP ? 's' : 'S') : (mode & S_IXGRP ? 'x' : '-'));
  str[7] = mode & S_IROTH ? 'r' : '-';
  str[8] = mode & S_IWOTH ? 'w' : '-';
  str[9] = (mode & S_ISVTX ? (mode & S_IXOTH ? 't' : 'T') : (mode & S_IXOTH ? 'x' : '-'));
  str[10] = '\0';
  return str;
}
