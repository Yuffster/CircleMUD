/* ************************************************************************
*  file: purgeplay.c                                    Part of CircleMUD * 
*  Usage: purge useless chars from playerfile                             *
*  All Rights Reserved                                                    *
*  Copyright (C) 1992, 1993 The Trustees of The Johns Hopkins University  *
************************************************************************* */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"


void purge(char *filename)
{
  FILE *fl;
  FILE *outfile;
  struct char_file_u player;
  int okay, num = 0;
  long timeout, size;
  char *ptr, reason[80];

  if (!(fl = fopen(filename, "r+"))) {
    printf("Can't open %s.", filename);
    exit(1);
  }
  fseek(fl, 0L, SEEK_END);
  size = ftell(fl);
  rewind(fl);
  if (size % sizeof(struct char_file_u)) {
    fprintf(stderr, "\aWARNING:  File size does not match structure, recompile purgeplay.\n");
    fclose(fl);
    exit(1);
  }

  outfile = fopen("players.new", "w");
  printf("Deleting: \n");

  for (;;) {
    fread(&player, sizeof(struct char_file_u), 1, fl);
    if (feof(fl)) {
      fclose(fl);
      fclose(outfile);
      printf("Done.\n");
      exit(0);
    }
    okay = 1;
    *reason = '\0';

    for (ptr = player.name; *ptr; ptr++)
      if (!isalpha(*ptr) || *ptr == ' ') {
	okay = 0;
	strcpy(reason, "Invalid name");
      }
    if (player.level == 0) {
      okay = 0;
      strcpy(reason, "Never entered game");
    }
    if (player.level < 0 || player.level > LVL_IMPL) {
      okay = 0;
      strcpy(reason, "Invalid level");
    }
    /* now, check for timeouts.  They only apply if the char is not
       cryo-rented.   Lev 32-34 do not time out.  */

    timeout = 1000;

    if (okay && player.level <= LVL_IMMORT) {

      if (!(player.char_specials_saved.act & PLR_CRYO)) {
	if (player.level == 1)		timeout = 4;	/* Lev   1 : 4 days */
	else if (player.level <= 4)	timeout = 7;	/* Lev 2-4 : 7 days */
	else if (player.level <= 10)	timeout = 30;	/* Lev 5-10: 30 days */
	else if (player.level <= LVL_IMMORT - 1)
	  timeout = 60;		/* Lev 11-30: 60 days */
	else if (player.level <= LVL_IMMORT)
	  timeout = 90;		/* Lev 31: 90 days */
      } else
	timeout = 90;

      timeout *= SECS_PER_REAL_DAY;

      if ((time(0) - player.last_logon) > timeout) {
	okay = 0;
	sprintf(reason, "Level %2d idle for %3ld days", player.level,
		((time(0) - player.last_logon) / SECS_PER_REAL_DAY));
      }
    }
    if (player.char_specials_saved.act & PLR_DELETED) {
      okay = 0;
      sprintf(reason, "Deleted flag set");
    }

    /* Don't delete for *any* of the above reasons if they have NODELETE */
    if (!okay && (player.char_specials_saved.act & PLR_NODELETE)) {
      okay = 2;
      strcat(reason, "; NOT deleted.");
    }
    if (okay)
      fwrite(&player, sizeof(struct char_file_u), 1, outfile);
    else
      printf("%4d. %-20s %s\n", ++num, player.name, reason);

    if (okay == 2)
      fprintf(stderr, "%-20s %s\n", player.name, reason);
  }
}



int main(int argc, char *argv[])
{
  if (argc != 2)
    printf("Usage: %s playerfile-name\n", argv[0]);
  else
    purge(argv[1]);

  return (0);
}
