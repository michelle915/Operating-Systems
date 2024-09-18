/* This file serves as a testbench for your library. You do not 
 * need to modify it.
 */
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "libtree.h"

int
main(int argc, char *argv[])
{
  struct tree_options opts = {.indent = 2, .sort=ALPHA};
  char const *optstring = "+adpugsrtUhi:";
  for (char c; (c = getopt(argc, argv, optstring)) != -1;) {
    switch (c) {
      case 'a':
        opts.all = true;
        break;
      case 'd':
        opts.dirsonly = true;
        break;
      case 'p':
        opts.perms = true;
        break;
      case 'u':
        opts.user = true;
        break;
      case 'g':
        opts.group = true;
        break;
      case 's':
        opts.size = true;
        break;
      case 'r':
        opts.sort = RALPHA;
        break;
      case 't':
        opts.sort = TIME;
        break;
      case 'U':
        opts.sort = NONE;
        break;
      case 'i': {
        char *end = optarg;
        long int i = strtol(optarg, &end, 10);
        if (*optarg != '\0' && *end == '\0')
          opts.indent = i;
        else
          err(errno = EINVAL, "%s", optarg);
        break;
      }
      case 'h':
      case '?':
        fprintf(stderr, "Usage: %s [-adpugsrtUh] [path...]\n", argv[0]);
        exit(1);
    }
  }

#ifdef DEBUG
#define boolstr(b) (b ? "true" : "false")
  fprintf(stderr,
          "opts = {\n"
          "  .all      = %5s, /* print hidden '.' files */\n"
          "  .dirsonly = %5s, /* list directories only */\n"
          "  .perms    = %5s, /* print file type and permissions */ \n"
          "  .user     = %5s, /* print the username of the file */\n"
          "  .group    = %5s, /* print the group name of file */\n"
          "  .size     = %5s, /* print file size in bytes */\n"
          "  .sort     = %5s, /* sorting method to use */\n"
          "  .indent   = %5d, /* indent size */"
          "};\n",
          boolstr(opts.all), boolstr(opts.dirsonly), boolstr(opts.perms), boolstr(opts.user),
          boolstr(opts.group), boolstr(opts.size),
          (char *[]){"NONE", "ALPHA", "RALPHA", "TIME"}[opts.sort], opts.indent);
#endif

  if (optind < argc) {
    for (int i = optind; i < argc; ++i) {
      if (tree_print(argv[i], opts) == -1) err(errno, "printing tree for %s", argv[i]);
    }
  } else {
    if (tree_print("./", opts) == -1) err(errno, "printing tree for ./");
  }
}
