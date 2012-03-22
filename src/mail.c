/* ************************************************************************
*   File: mail.c                                        Part of CircleMUD *
*  Usage: Internal funcs and player spec-procs of mud-mail system         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/******* MUD MAIL SYSTEM MAIN FILE ***************************************

Written by Jeremy Elson (jelson@circlemud.org)

*************************************************************************/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "interpreter.h"
#include "handler.h"
#include "mail.h"


/* external variables */
extern int no_mail;

/* external functions */
SPECIAL(postmaster);

/* local globals */
mail_index_type *mail_index = NULL;	/* list of recs in the mail file  */
position_list_type *free_list = NULL;	/* list of free positions in file */
long file_end_pos = 0;			/* length of file */

/* local functions */
void postmaster_send_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void postmaster_check_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void postmaster_receive_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void push_free_list(long pos);
long pop_free_list(void);
void clear_free_list(void);
mail_index_type *find_char_in_index(long searchee);
void write_to_file(void *buf, int size, long filepos);
void read_from_file(void *buf, int size, long filepos);
void index_mail(long id_to_index, long pos);
int mail_recip_ok(const char *name);

/* -------------------------------------------------------------------------- */

int mail_recip_ok(const char *name)
{
  struct char_file_u tmp_store;
  struct char_data *victim;
  int ret = FALSE;

  CREATE(victim, struct char_data, 1);
  clear_char(victim);
  if (load_char(name, &tmp_store) >= 0) {
    store_to_char(&tmp_store, victim);
    char_to_room(victim, 0);
    if (!PLR_FLAGGED(victim, PLR_DELETED))
      ret = TRUE;
    extract_char_final(victim);
  } else 
    free(victim);
  return ret;
}

/*
 * void push_free_list(long #1)
 * #1 - What byte offset into the file the block resides.
 *
 * Net effect is to store a list of free blocks in the mail file in a linked
 * list.  This is called when people receive their messages and at startup
 * when the list is created.
 */
void push_free_list(long pos)
{
  position_list_type *new_pos;

  CREATE(new_pos, position_list_type, 1);
  new_pos->position = pos;
  new_pos->next = free_list;
  free_list = new_pos;
}


/*
 * long pop_free_list(none)
 * Returns the offset of a free block in the mail file.
 *
 * Typically used whenever a person mails a message.  The blocks are not
 * guaranteed to be sequential or in any order at all.
 */
long pop_free_list(void)
{
  position_list_type *old_pos;
  long return_value;

  /*
   * If we don't have any free blocks, we append to the file.
   */
  if ((old_pos = free_list) == NULL)
    return (file_end_pos);

  /* Save the offset of the free block. */
  return_value = free_list->position;
  /* Remove this block from the free list. */
  free_list = old_pos->next;
  /* Get rid of the memory the node took. */
  free(old_pos);
  /* Give back the free offset. */
  return (return_value);
}


void clear_free_list(void)
{
  while (free_list)
    pop_free_list();
}


/*
 * main_index_type *find_char_in_index(long #1)
 * #1 - The idnum of the person to look for.
 * Returns a pointer to the mail block found.
 *
 * Finds the first mail block for a specific person based on id number.
 */
mail_index_type *find_char_in_index(long searchee)
{
  mail_index_type *tmp;

  if (searchee < 0) {
    log("SYSERR: Mail system -- non fatal error #1 (searchee == %ld).", searchee);
    return (NULL);
  }
  for (tmp = mail_index; (tmp && tmp->recipient != searchee); tmp = tmp->next);

  return (tmp);
}


/*
 * void write_to_file(void * #1, int #2, long #3)
 * #1 - A pointer to the data to write, usually the 'block' record.
 * #2 - How much to write (because we'll write NUL terminated strings.)
 * #3 - What offset (block position) in the file to write to.
 *
 * Writes a mail block back into the database at the given location.
 */
void write_to_file(void *buf, int size, long filepos)
{
  FILE *mail_file;

  if (filepos % BLOCK_SIZE) {
    log("SYSERR: Mail system -- fatal error #2!!! (invalid file position %ld)", filepos);
    no_mail = TRUE;
    return;
  }
  if (!(mail_file = fopen(MAIL_FILE, "r+b"))) {
    log("SYSERR: Unable to open mail file '%s'.", MAIL_FILE);
    no_mail = TRUE;
    return;
  }
  fseek(mail_file, filepos, SEEK_SET);
  fwrite(buf, size, 1, mail_file);

  /* find end of file */
  fseek(mail_file, 0L, SEEK_END);
  file_end_pos = ftell(mail_file);
  fclose(mail_file);
  return;
}


/*
 * void read_from_file(void * #1, int #2, long #3)
 * #1 - A pointer to where we should store the data read.
 * #2 - How large the block we're reading is.
 * #3 - What position in the file to read.
 *
 * This reads a block from the mail database file.
 */
void read_from_file(void *buf, int size, long filepos)
{
  FILE *mail_file;

  if (filepos % BLOCK_SIZE) {
    log("SYSERR: Mail system -- fatal error #3!!! (invalid filepos read %ld)", filepos);
    no_mail = TRUE;
    return;
  }
  if (!(mail_file = fopen(MAIL_FILE, "r+b"))) {
    log("SYSERR: Unable to open mail file '%s'.", MAIL_FILE);
    no_mail = TRUE;
    return;
  }

  fseek(mail_file, filepos, SEEK_SET);
  fread(buf, size, 1, mail_file);
  fclose(mail_file);
  return;
}


void index_mail(long id_to_index, long pos)
{
  mail_index_type *new_index;
  position_list_type *new_position;

  if (id_to_index < 0) {
    log("SYSERR: Mail system -- non-fatal error #4. (id_to_index == %ld)", id_to_index);
    return;
  }
  if (!(new_index = find_char_in_index(id_to_index))) {
    /* name not already in index.. add it */
    CREATE(new_index, mail_index_type, 1);
    new_index->recipient = id_to_index;
    new_index->list_start = NULL;

    /* add to front of list */
    new_index->next = mail_index;
    mail_index = new_index;
  }
  /* now, add this position to front of position list */
  CREATE(new_position, position_list_type, 1);
  new_position->position = pos;
  new_position->next = new_index->list_start;
  new_index->list_start = new_position;
}


/*
 * int scan_file(none)
 * Returns false if mail file is corrupted or true if everything correct.
 *
 * This is called once during boot-up.  It scans through the mail file
 * and indexes all entries currently in the mail file.
 */
int scan_file(void)
{
  FILE *mail_file;
  header_block_type next_block;
  int total_messages = 0, block_num = 0;

  if (!(mail_file = fopen(MAIL_FILE, "rb"))) {
    log("   Mail file non-existant... creating new file.");
    touch(MAIL_FILE);
    return (1);
  }
  while (fread(&next_block, sizeof(header_block_type), 1, mail_file)) {
    if (next_block.block_type == HEADER_BLOCK) {
      index_mail(next_block.header_data.to, block_num * BLOCK_SIZE);
      total_messages++;
    } else if (next_block.block_type == DELETED_BLOCK)
      push_free_list(block_num * BLOCK_SIZE);
    block_num++;
  }

  file_end_pos = ftell(mail_file);
  fclose(mail_file);
  log("   %ld bytes read.", file_end_pos);
  if (file_end_pos % BLOCK_SIZE) {
    log("SYSERR: Error booting mail system -- Mail file corrupt!");
    log("SYSERR: Mail disabled!");
    return (0);
  }
  log("   Mail file read -- %d messages.", total_messages);
  return (1);
}				/* end of scan_file */


/*
 * int has_mail(long #1)
 * #1 - id number of the person to check for mail.
 * Returns true or false.
 *
 * A simple little function which tells you if the guy has mail or not.
 */
int has_mail(long recipient)
{
  return (find_char_in_index(recipient) != NULL);
}


/*
 * void store_mail(long #1, long #2, char * #3)
 * #1 - id number of the person to mail to.
 * #2 - id number of the person the mail is from.
 * #3 - The actual message to send.
 *
 * call store_mail to store mail.  (hard, huh? :-) )  Pass 3 arguments:
 * who the mail is to (long), who it's from (long), and a pointer to the
 * actual message text (char *).
 */
void store_mail(long to, long from, char *message_pointer)
{
  header_block_type header;
  data_block_type data;
  long last_address, target_address;
  char *msg_txt = message_pointer;
  int bytes_written, total_length = strlen(message_pointer);

  if ((sizeof(header_block_type) != sizeof(data_block_type)) ||
      (sizeof(header_block_type) != BLOCK_SIZE)) {
    core_dump();
    return;
  }

  if (from < 0 || to < 0 || !*message_pointer) {
    log("SYSERR: Mail system -- non-fatal error #5. (from == %ld, to == %ld)", from, to);
    return;
  }
  memset((char *) &header, 0, sizeof(header));	/* clear the record */
  header.block_type = HEADER_BLOCK;
  header.header_data.next_block = LAST_BLOCK;
  header.header_data.from = from;
  header.header_data.to = to;
  header.header_data.mail_time = time(0);
  strncpy(header.txt, msg_txt, HEADER_BLOCK_DATASIZE);	/* strncpy: OK (h.txt:HEADER_BLOCK_DATASIZE+1) */
  header.txt[HEADER_BLOCK_DATASIZE] = '\0';

  target_address = pop_free_list();	/* find next free block */
  index_mail(to, target_address);	/* add it to mail index in memory */
  write_to_file(&header, BLOCK_SIZE, target_address);

  if (strlen(msg_txt) <= HEADER_BLOCK_DATASIZE)
    return;			/* that was the whole message */

  bytes_written = HEADER_BLOCK_DATASIZE;
  msg_txt += HEADER_BLOCK_DATASIZE;	/* move pointer to next bit of text */

  /*
   * find the next block address, then rewrite the header to reflect where
   * the next block is.
   */
  last_address = target_address;
  target_address = pop_free_list();
  header.header_data.next_block = target_address;
  write_to_file(&header, BLOCK_SIZE, last_address);

  /* now write the current data block */
  memset((char *) &data, 0, sizeof(data));	/* clear the record */
  data.block_type = LAST_BLOCK;
  strncpy(data.txt, msg_txt, DATA_BLOCK_DATASIZE);	/* strncpy: OK (d.txt:DATA_BLOCK_DATASIZE+1) */
  data.txt[DATA_BLOCK_DATASIZE] = '\0';
  write_to_file(&data, BLOCK_SIZE, target_address);
  bytes_written += strlen(data.txt);
  msg_txt += strlen(data.txt);

  /*
   * if, after 1 header block and 1 data block there is STILL part of the
   * message left to write to the file, keep writing the new data blocks and
   * rewriting the old data blocks to reflect where the next block is.  Yes,
   * this is kind of a hack, but if the block size is big enough it won't
   * matter anyway.  Hopefully, MUD players won't pour their life stories out
   * into the Mud Mail System anyway.
   * 
   * Note that the block_type data field in data blocks is either a number >=0,
   * meaning a link to the next block, or LAST_BLOCK flag (-2) meaning the
   * last block in the current message.  This works much like DOS' FAT.
   */
  while (bytes_written < total_length) {
    last_address = target_address;
    target_address = pop_free_list();

    /* rewrite the previous block to link it to the next */
    data.block_type = target_address;
    write_to_file(&data, BLOCK_SIZE, last_address);

    /* now write the next block, assuming it's the last.  */
    data.block_type = LAST_BLOCK;
    strncpy(data.txt, msg_txt, DATA_BLOCK_DATASIZE);	/* strncpy: OK (d.txt:DATA_BLOCK_DATASIZE+1) */
    data.txt[DATA_BLOCK_DATASIZE] = '\0';
    write_to_file(&data, BLOCK_SIZE, target_address);

    bytes_written += strlen(data.txt);
    msg_txt += strlen(data.txt);
  }
}				/* store mail */


/*
 * char *read_delete(long #1)
 * #1 - The id number of the person we're checking mail for.
 * Returns the message text of the mail received.
 *
 * Retrieves one messsage for a player. The mail is then discarded from
 * the file and the mail index.
 */
char *read_delete(long recipient)
{
  header_block_type header;
  data_block_type data;
  mail_index_type *mail_pointer, *prev_mail;
  position_list_type *position_pointer;
  long mail_address, following_block;
  char *tmstr, buf[MAX_MAIL_SIZE + 256];	/* + header */
  char *from, *to;

  if (recipient < 0) {
    log("SYSERR: Mail system -- non-fatal error #6. (recipient: %ld)", recipient);
    return (NULL);
  }
  if (!(mail_pointer = find_char_in_index(recipient))) {
    log("SYSERR: Mail system -- post office spec_proc error?  Error #7. (invalid character in index)");
    return (NULL);
  }
  if (!(position_pointer = mail_pointer->list_start)) {
    log("SYSERR: Mail system -- non-fatal error #8. (invalid position pointer %p)", position_pointer);
    return (NULL);
  }
  if (!(position_pointer->next)) {	/* just 1 entry in list. */
    mail_address = position_pointer->position;
    free(position_pointer);

    /* now free up the actual name entry */
    if (mail_index == mail_pointer) {	/* name is 1st in list */
      mail_index = mail_pointer->next;
      free(mail_pointer);
    } else {
      /* find entry before the one we're going to del */
      for (prev_mail = mail_index;
	   prev_mail->next != mail_pointer;
	   prev_mail = prev_mail->next);
      prev_mail->next = mail_pointer->next;
      free(mail_pointer);
    }
  } else {
    /* move to next-to-last record */
    while (position_pointer->next->next)
      position_pointer = position_pointer->next;
    mail_address = position_pointer->next->position;
    free(position_pointer->next);
    position_pointer->next = NULL;
  }

  /* ok, now lets do some readin'! */
  read_from_file(&header, BLOCK_SIZE, mail_address);

  if (header.block_type != HEADER_BLOCK) {
    log("SYSERR: Oh dear. (Header block %ld != %d)", header.block_type, HEADER_BLOCK);
    no_mail = TRUE;
    log("SYSERR: Mail system disabled!  -- Error #9. (Invalid header block.)");
    return (NULL);
  }
  tmstr = asctime(localtime(&header.header_data.mail_time));
  *(tmstr + strlen(tmstr) - 1) = '\0';

  from = get_name_by_id(header.header_data.from);
  to = get_name_by_id(recipient);

  snprintf(buf, sizeof(buf),
	" * * * * Midgaard Mail System * * * *\r\n"
	"Date: %s\r\n"
	"  To: %s\r\n"
	"From: %s\r\n"
	"\r\n"
	"%s",

	tmstr,
	to ? to : "Unknown",
	from ? from : "Unknown",
	header.txt
	);
  following_block = header.header_data.next_block;

  /* mark the block as deleted */
  header.block_type = DELETED_BLOCK;
  write_to_file(&header, BLOCK_SIZE, mail_address);
  push_free_list(mail_address);

  while (following_block != LAST_BLOCK) {
    read_from_file(&data, BLOCK_SIZE, following_block);

    strcat(buf, data.txt);	/* strcat: OK (data.txt:DATA_BLOCK_DATASIZE < buf:MAX_MAIL_SIZE) */
    mail_address = following_block;
    following_block = data.block_type;
    data.block_type = DELETED_BLOCK;
    write_to_file(&data, BLOCK_SIZE, mail_address);
    push_free_list(mail_address);
  }

  return strdup(buf);
}


/****************************************************************
* Below is the spec_proc for a postmaster using the above       *
* routines.  Written by Jeremy Elson (jelson@circlemud.org) *
****************************************************************/

SPECIAL(postmaster)
{
  if (!ch->desc || IS_NPC(ch))
    return (0);			/* so mobs don't get caught here */

  if (!(CMD_IS("mail") || CMD_IS("check") || CMD_IS("receive")))
    return (0);

  if (no_mail) {
    send_to_char(ch, "Sorry, the mail system is having technical difficulties.\r\n");
    return (0);
  }

  if (CMD_IS("mail")) {
    postmaster_send_mail(ch, (struct char_data *)me, cmd, argument);
    return (1);
  } else if (CMD_IS("check")) {
    postmaster_check_mail(ch, (struct char_data *)me, cmd, argument);
    return (1);
  } else if (CMD_IS("receive")) {
    postmaster_receive_mail(ch, (struct char_data *)me, cmd, argument);
    return (1);
  } else
    return (0);
}


void postmaster_send_mail(struct char_data *ch, struct char_data *mailman,
			  int cmd, char *arg)
{
  long recipient;
  char buf[MAX_INPUT_LENGTH], **mailwrite;

  if (GET_LEVEL(ch) < MIN_MAIL_LEVEL) {
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you have to be level %d to send mail!'", MIN_MAIL_LEVEL);
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  one_argument(arg, buf);

  if (!*buf) {			/* you'll get no argument from me! */
    act("$n tells you, 'You need to specify an addressee!'",
	FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  if (GET_GOLD(ch) < STAMP_PRICE) {
    snprintf(buf, sizeof(buf), "$n tells you, 'A stamp costs %d coin%s.'\r\n"
	    "$n tells you, '...which I see you can't afford.'", STAMP_PRICE,
            STAMP_PRICE == 1 ? "" : "s");
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  if ((recipient = get_id_by_name(buf)) < 0 || !mail_recip_ok(buf)) {
    act("$n tells you, 'No one by that name is registered here!'",
	FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  act("$n starts to write some mail.", TRUE, ch, 0, 0, TO_ROOM);
  snprintf(buf, sizeof(buf), "$n tells you, 'I'll take %d coins for the stamp.'\r\n"
       "$n tells you, 'Write your message, use @ on a new line when done.'",
	  STAMP_PRICE);

  act(buf, FALSE, mailman, 0, ch, TO_VICT);
  GET_GOLD(ch) -= STAMP_PRICE;
  SET_BIT(PLR_FLAGS(ch), PLR_MAILING);	/* string_write() sets writing. */

  /* Start writing! */
  CREATE(mailwrite, char *, 1);
  string_write(ch->desc, mailwrite, MAX_MAIL_SIZE, recipient, NULL);
}


void postmaster_check_mail(struct char_data *ch, struct char_data *mailman,
			  int cmd, char *arg)
{
  if (has_mail(GET_IDNUM(ch)))
    act("$n tells you, 'You have mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
  else
    act("$n tells you, 'Sorry, you don't have any mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
}


void postmaster_receive_mail(struct char_data *ch, struct char_data *mailman,
			  int cmd, char *arg)
{
  char buf[256];
  struct obj_data *obj;

  if (!has_mail(GET_IDNUM(ch))) {
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you don't have any mail waiting.'");
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  while (has_mail(GET_IDNUM(ch))) {
    obj = create_obj();
    obj->item_number = NOTHING;
    obj->name = strdup("mail paper letter");
    obj->short_description = strdup("a piece of mail");
    obj->description = strdup("Someone has left a piece of mail here.");

    GET_OBJ_TYPE(obj) = ITEM_NOTE;
    GET_OBJ_WEAR(obj) = ITEM_WEAR_TAKE | ITEM_WEAR_HOLD;
    GET_OBJ_WEIGHT(obj) = 1;
    GET_OBJ_COST(obj) = 30;
    GET_OBJ_RENT(obj) = 10;
    obj->action_description = read_delete(GET_IDNUM(ch));

    if (obj->action_description == NULL)
      obj->action_description =
	strdup("Mail system error - please report.  Error #11.\r\n");

    obj_to_char(obj, ch);

    act("$n gives you a piece of mail.", FALSE, mailman, 0, ch, TO_VICT);
    act("$N gives $n a piece of mail.", FALSE, ch, 0, mailman, TO_ROOM);
  }
}
