/* ************************************************************************
*   File: modify.c                                      Part of CircleMUD *
*  Usage: Run-time modification of game variables                         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"


#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "spells.h"
#include "mail.h"
#include "boards.h"

void show_string(struct descriptor_data *d, char *input);

extern struct spell_info_type spell_info[];
extern const char *MENU;
extern const char *unused_spellname;	/* spell_parser.c */

/* local functions */
void smash_tilde(char *str);
ACMD(do_skillset);
char *next_page(char *str);
int count_pages(char *str);
void paginate_string(char *str, struct descriptor_data *d);

const char *string_fields[] =
{
  "name",
  "short",
  "long",
  "description",
  "title",
  "delete-description",
  "\n"
};


/* maximum length for text field x+1 */
int length[] =
{
  15,
  60,
  256,
  240,
  60
};


/* ************************************************************************
*  modification of malloc'ed strings                                      *
************************************************************************ */

/*
 * Put '#if 1' here to erase ~, or roll your own method.  A common idea
 * is smash/show tilde to convert the tilde to another innocuous character
 * to save and then back to display it. Whatever you do, at least keep the
 * function around because other MUD packages use it, like mudFTP.
 *   -gg 9/9/98
 */
void smash_tilde(char *str)
{
#if 0
  /*
   * Erase any ~'s inserted by people in the editor.  This prevents anyone
   * using online creation from causing parse errors in the world files.
   * Derived from an idea by Sammy <samedi@dhc.net> (who happens to like
   * his tildes thank you very much.), -gg 2/20/98
   */
    while ((str = strchr(str, '~')) != NULL)
      *str = ' ';
#endif
}

/*
 * Basic API function to start writing somewhere.
 *
 * 'data' isn't used in stock CircleMUD but you can use it to pass whatever
 * else you may want through it.  The improved editor patch when updated
 * could use it to pass the old text buffer, for instance.
 */
void string_write(struct descriptor_data *d, char **writeto, size_t len, long mailto, void *data)
{
  if (d->character && !IS_NPC(d->character))
    SET_BIT(PLR_FLAGS(d->character), PLR_WRITING);

  if (data)
    mudlog(BRF, LVL_IMMORT, TRUE, "SYSERR: string_write: I don't understand special data.");

  d->str = writeto;
  d->max_str = len;
  d->mail_to = mailto;
}

/* Add user input to the 'current' string (as defined by d->str) */
void string_add(struct descriptor_data *d, char *str)
{
  int terminator;

  /* determine if this is the terminal string, and truncate if so */
  /* changed to only accept '@' at the beginning of line - J. Elson 1/17/94 */

  delete_doubledollar(str);

  if ((terminator = (*str == '@')))
    *str = '\0';

  smash_tilde(str);

  if (!(*d->str)) {
    if (strlen(str) + 3 > d->max_str) { /* \r\n\0 */
      send_to_char(d->character, "String too long - Truncated.\r\n");
      strcpy(&str[d->max_str - 3], "\r\n");	/* strcpy: OK (size checked) */
      CREATE(*d->str, char, d->max_str);
      strcpy(*d->str, str);	/* strcpy: OK (size checked) */
      terminator = 1;
    } else {
      CREATE(*d->str, char, strlen(str) + 3);
      strcpy(*d->str, str);	/* strcpy: OK (size checked) */
    }
  } else {
    if (strlen(str) + strlen(*d->str) + 3 > d->max_str) { /* \r\n\0 */
      send_to_char(d->character, "String too long.  Last line skipped.\r\n");
      terminator = 1;
    } else {
      RECREATE(*d->str, char, strlen(*d->str) + strlen(str) + 3); /* \r\n\0 */
      strcat(*d->str, str);	/* strcat: OK (size precalculated) */
    }
  }

  if (terminator) {
    if (STATE(d) == CON_PLAYING && (PLR_FLAGGED(d->character, PLR_MAILING))) {
      store_mail(d->mail_to, GET_IDNUM(d->character), *d->str);
      d->mail_to = 0;
      free(*d->str);
      free(d->str);
      write_to_output(d, "Message sent!\r\n");
      if (!IS_NPC(d->character))
	REMOVE_BIT(PLR_FLAGS(d->character), PLR_MAILING | PLR_WRITING);
    }
    d->str = NULL;

    if (d->mail_to >= BOARD_MAGIC) {
      Board_save_board(d->mail_to - BOARD_MAGIC);
      d->mail_to = 0;
    }
    if (STATE(d) == CON_EXDESC) {
      write_to_output(d, "%s", MENU);
      STATE(d) = CON_MENU;
    }
    if (STATE(d) == CON_PLAYING && d->character && !IS_NPC(d->character))
      REMOVE_BIT(PLR_FLAGS(d->character), PLR_WRITING);
  } else
    strcat(*d->str, "\r\n");	/* strcat: OK (size checked) */
}



/* **********************************************************************
*  Modification of character skills                                     *
********************************************************************** */

ACMD(do_skillset)
{
  struct char_data *vict;
  char name[MAX_INPUT_LENGTH];
  char buf[MAX_INPUT_LENGTH], help[MAX_STRING_LENGTH];
  int skill, value, i, qend;

  argument = one_argument(argument, name);

  if (!*name) {			/* no arguments. print an informative text */
    send_to_char(ch, "Syntax: skillset <name> '<skill>' <value>\r\n"
		"Skill being one of the following:\r\n");
    for (qend = 0, i = 0; i <= TOP_SPELL_DEFINE; i++) {
      if (spell_info[i].name == unused_spellname)	/* This is valid. */
	continue;
      send_to_char(ch, "%18s", spell_info[i].name);
      if (qend++ % 4 == 3)
	send_to_char(ch, "\r\n");
    }
    if (qend % 4 != 0)
      send_to_char(ch, "\r\n");
    return;
  }

  if (!(vict = get_char_vis(ch, name, NULL, FIND_CHAR_WORLD))) {
    send_to_char(ch, "%s", NOPERSON);
    return;
  }
  skip_spaces(&argument);

  /* If there is no chars in argument */
  if (!*argument) {
    send_to_char(ch, "Skill name expected.\r\n");
    return;
  }
  if (*argument != '\'') {
    send_to_char(ch, "Skill must be enclosed in: ''\r\n");
    return;
  }
  /* Locate the last quote and lowercase the magic words (if any) */

  for (qend = 1; argument[qend] && argument[qend] != '\''; qend++)
    argument[qend] = LOWER(argument[qend]);

  if (argument[qend] != '\'') {
    send_to_char(ch, "Skill must be enclosed in: ''\r\n");
    return;
  }
  strcpy(help, (argument + 1));	/* strcpy: OK (MAX_INPUT_LENGTH <= MAX_STRING_LENGTH) */
  help[qend - 1] = '\0';
  if ((skill = find_skill_num(help)) <= 0) {
    send_to_char(ch, "Unrecognized skill.\r\n");
    return;
  }
  argument += qend + 1;		/* skip to next parameter */
  argument = one_argument(argument, buf);

  if (!*buf) {
    send_to_char(ch, "Learned value expected.\r\n");
    return;
  }
  value = atoi(buf);
  if (value < 0) {
    send_to_char(ch, "Minimum value for learned is 0.\r\n");
    return;
  }
  if (value > 100) {
    send_to_char(ch, "Max value for learned is 100.\r\n");
    return;
  }
  if (IS_NPC(vict)) {
    send_to_char(ch, "You can't set NPC skills.\r\n");
    return;
  }

  /*
   * find_skill_num() guarantees a valid spell_info[] index, or -1, and we
   * checked for the -1 above so we are safe here.
   */
  SET_SKILL(vict, skill, value);
  mudlog(BRF, LVL_IMMORT, TRUE, "%s changed %s's %s to %d.", GET_NAME(ch), GET_NAME(vict), spell_info[skill].name, value);
  send_to_char(ch, "You change %s's %s to %d.\r\n", GET_NAME(vict), spell_info[skill].name, value);
}



/*********************************************************************
* New Pagination Code
* Michael Buselli submitted the following code for an enhanced pager
* for CircleMUD.  All functions below are his.  --JE 8 Mar 96
*
*********************************************************************/

/* Traverse down the string until the begining of the next page has been
 * reached.  Return NULL if this is the last page of the string.
 */
char *next_page(char *str)
{
  int col = 1, line = 1, spec_code = FALSE;

  for (;; str++) {
    /* If end of string, return NULL. */
    if (*str == '\0')
      return (NULL);

    /* If we're at the start of the next page, return this fact. */
    else if (line > PAGE_LENGTH)
      return (str);

    /* Check for the begining of an ANSI color code block. */
    else if (*str == '\x1B' && !spec_code)
      spec_code = TRUE;

    /* Check for the end of an ANSI color code block. */
    else if (*str == 'm' && spec_code)
      spec_code = FALSE;

    /* Check for everything else. */
    else if (!spec_code) {
      /* Carriage return puts us in column one. */
      if (*str == '\r')
	col = 1;
      /* Newline puts us on the next line. */
      else if (*str == '\n')
	line++;

      /* We need to check here and see if we are over the page width,
       * and if so, compensate by going to the begining of the next line.
       */
      else if (col++ > PAGE_WIDTH) {
	col = 1;
	line++;
      }
    }
  }
}


/* Function that returns the number of pages in the string. */
int count_pages(char *str)
{
  int pages;

  for (pages = 1; (str = next_page(str)); pages++);
  return (pages);
}


/* This function assigns all the pointers for showstr_vector for the
 * page_string function, after showstr_vector has been allocated and
 * showstr_count set.
 */
void paginate_string(char *str, struct descriptor_data *d)
{
  int i;

  if (d->showstr_count)
    *(d->showstr_vector) = str;

  for (i = 1; i < d->showstr_count && str; i++)
    str = d->showstr_vector[i] = next_page(str);

  d->showstr_page = 0;
}


/* The call that gets the paging ball rolling... */
void page_string(struct descriptor_data *d, char *str, int keep_internal)
{
  char actbuf[MAX_INPUT_LENGTH] = "";

  if (!d)
    return;

  if (!str || !*str)
    return;

  d->showstr_count = count_pages(str);
  CREATE(d->showstr_vector, char *, d->showstr_count);

  if (keep_internal) {
    d->showstr_head = strdup(str);
    paginate_string(d->showstr_head, d);
  } else
    paginate_string(str, d);

  show_string(d, actbuf);
}


/* The call that displays the next page. */
void show_string(struct descriptor_data *d, char *input)
{
  char buffer[MAX_STRING_LENGTH], buf[MAX_INPUT_LENGTH];
  int diff;

  any_one_arg(input, buf);

  /* Q is for quit. :) */
  if (LOWER(*buf) == 'q') {
    free(d->showstr_vector);
    d->showstr_vector = NULL;
    d->showstr_count = 0;
    if (d->showstr_head) {
      free(d->showstr_head);
      d->showstr_head = NULL;
    }
    return;
  }
  /* R is for refresh, so back up one page internally so we can display
   * it again.
   */
  else if (LOWER(*buf) == 'r')
    d->showstr_page = MAX(0, d->showstr_page - 1);

  /* B is for back, so back up two pages internally so we can display the
   * correct page here.
   */
  else if (LOWER(*buf) == 'b')
    d->showstr_page = MAX(0, d->showstr_page - 2);

  /* Feature to 'goto' a page.  Just type the number of the page and you
   * are there!
   */
  else if (isdigit(*buf))
    d->showstr_page = MAX(0, MIN(atoi(buf) - 1, d->showstr_count - 1));

  else if (*buf) {
    send_to_char(d->character, "Valid commands while paging are RETURN, Q, R, B, or a numeric value.\r\n");
    return;
  }
  /* If we're displaying the last page, just send it to the character, and
   * then free up the space we used.
   */
  if (d->showstr_page + 1 >= d->showstr_count) {
    send_to_char(d->character, "%s", d->showstr_vector[d->showstr_page]);
    free(d->showstr_vector);
    d->showstr_vector = NULL;
    d->showstr_count = 0;
    if (d->showstr_head) {
      free(d->showstr_head);
      d->showstr_head = NULL;
    }
  }
  /* Or if we have more to show.... */
  else {
    diff = d->showstr_vector[d->showstr_page + 1] - d->showstr_vector[d->showstr_page];
    if (diff > MAX_STRING_LENGTH - 3) /* 3=\r\n\0 */
      diff = MAX_STRING_LENGTH - 3;
    strncpy(buffer, d->showstr_vector[d->showstr_page], diff);	/* strncpy: OK (size truncated above) */
    /*
     * Fix for prompt overwriting last line in compact mode submitted by
     * Peter Ajamian <peter@pajamian.dhs.org> on 04/21/2001
     */
    if (buffer[diff - 2] == '\r' && buffer[diff - 1]=='\n')
      buffer[diff] = '\0';
    else if (buffer[diff - 2] == '\n' && buffer[diff - 1] == '\r')
      /* This is backwards.  Fix it. */
      strcpy(buffer + diff - 2, "\r\n");	/* strcpy: OK (size checked) */
    else if (buffer[diff - 1] == '\r' || buffer[diff - 1] == '\n')
      /* Just one of \r\n.  Overwrite it. */
      strcpy(buffer + diff - 1, "\r\n");	/* strcpy: OK (size checked) */
    else
      /* Tack \r\n onto the end to fix bug with prompt overwriting last line. */
      strcpy(buffer + diff, "\r\n");	/* strcpy: OK (size checked) */
    send_to_char(d->character, "%s", buffer);
    d->showstr_page++;
  }
}
