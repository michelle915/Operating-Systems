/* This header file provides the public declarations so that
 * others can use your library.
 */
#include <stdbool.h>

/* By convention, exposed library interfaces are prefixed
 * with the name of the library, in this case "tree_"
 */

struct
tree_options {
  bool all,
       dirsonly,
       perms,
       user,
       group,
       size;
  enum {NONE, ALPHA, RALPHA, TIME} sort;
  unsigned int indent;
};

extern int tree_print(char const *path, struct tree_options opts);
