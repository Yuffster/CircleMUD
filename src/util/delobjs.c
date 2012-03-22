/* ************************************************************************
*  file:  delobjs.c                                   Part of CircleMud   *
*  Usage: deleting object files for players who are not in the playerfile *
*  Written by Jeremy Elson 4/2/93                                         *
*  All Rights Reserved                                                    *
*  Copyright (C) 1993 The Trustees of The Johns Hopkins University        *
************************************************************************* */

/*
   I recommend you use the script in the lib/plrobjs directory instead of
   invoking this program directly; however, you can use this program thusly:

   usage: switch into an obj directory; type: delobjs <plrfile> <obj wildcard>
 */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"

struct name_element {
  char name[20];
  struct name_element *next;
};

struct name_element *name_list = 0;

void do_purge(int argc, char **argv)
{
  int x;
  struct name_element *tmp;
  char name[1024];
  int found;

  for (x = 2; x < argc; x++) {
    found = 0;
    strcpy(name, argv[x]);
    *(strchr(name, '.')) = '\0';
    for (tmp = name_list; !found && tmp; tmp = tmp->next)
      if (!strcmp(tmp->name, name))
	found = 1;
    if (!found) {
      remove(argv[x]);
      printf("Deleting %s\n", argv[x]);
    }
  }
}




int main(int argc, char **argv)
{
  char *ptr;
  struct char_file_u player;
  int okay;
  struct name_element *tmp;
  FILE *fl;

  if (argc < 3) {
    printf("Usage: %s <playerfile-name> <file1> <file2> ... <filen>\n",
	   argv[0]);
    exit(1);
  }
  if (!(fl = fopen(argv[1], "rb"))) {
    perror("Unable to open playerfile for reading");
    exit(1);
  }
  while (1) {
    fread(&player, sizeof(player), 1, fl);

    if (feof(fl)) {
      fclose(fl);
      do_purge(argc, argv);
      exit(0);
    }
    okay = 1;

    for (ptr = player.name; *ptr; ptr++)
      *ptr = LOWER(*ptr);

    if (player.char_specials_saved.act & PLR_DELETED)
      okay = 0;

    if (okay) {
      tmp = (struct name_element *) malloc(sizeof(struct name_element));
      tmp->next = name_list;
      strcpy(tmp->name, player.name);
      name_list = tmp;
    }
  }
}
