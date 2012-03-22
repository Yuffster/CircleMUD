/* ************************************************************************
*   File: objsave.c                                     Part of CircleMUD *
*  Usage: loading/saving player objects for rent and crash-save           *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"


#include "structs.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"
#include "spells.h"

/* these factors should be unique integers */
#define RENT_FACTOR 	1
#define CRYO_FACTOR 	4

#define LOC_INVENTORY	0
#define MAX_BAG_ROWS	5

/* external variables */
extern struct player_index_element *player_table;
extern int top_of_p_table;
extern int rent_file_timeout, crash_file_timeout;
extern int free_rent;
extern int min_rent_cost;
extern int max_obj_save;	/* change in config.c */

/* Extern functions */
ACMD(do_action);
ACMD(do_tell);
SPECIAL(receptionist);
SPECIAL(cryogenicist);
int invalid_class(struct char_data *ch, struct obj_data *obj);

/* local functions */
void Crash_extract_norent_eq(struct char_data *ch);
void auto_equip(struct char_data *ch, struct obj_data *obj, int location);
int Crash_offer_rent(struct char_data *ch, struct char_data *recep, int display, int factor);
int Crash_report_unrentables(struct char_data *ch, struct char_data *recep, struct obj_data *obj);
void Crash_report_rent(struct char_data *ch, struct char_data *recep, struct obj_data *obj, long *cost, long *nitems, int display, int factor);
struct obj_data *Obj_from_store(struct obj_file_elem object, int *location);
int Obj_to_store(struct obj_data *obj, FILE *fl, int location);
void update_obj_file(void);
int Crash_write_rentcode(struct char_data *ch, FILE *fl, struct rent_info *rent);
int gen_receptionist(struct char_data *ch, struct char_data *recep, int cmd, char *arg, int mode);
int Crash_save(struct obj_data *obj, FILE *fp, int location);
void Crash_rent_deadline(struct char_data *ch, struct char_data *recep, long cost);
void Crash_restore_weight(struct obj_data *obj);
void Crash_extract_objs(struct obj_data *obj);
int Crash_is_unrentable(struct obj_data *obj);
void Crash_extract_norents(struct obj_data *obj);
void Crash_extract_expensive(struct obj_data *obj);
void Crash_calculate_rent(struct obj_data *obj, int *cost);
void Crash_rentsave(struct char_data *ch, int cost);
void Crash_cryosave(struct char_data *ch, int cost);


struct obj_data *Obj_from_store(struct obj_file_elem object, int *location)
{
  struct obj_data *obj;
  obj_rnum itemnum;
  int j;

  *location = 0;
  if ((itemnum = real_object(object.item_number)) == NOTHING)
    return (NULL);

  obj = read_object(itemnum, REAL);
#if USE_AUTOEQ
  *location = object.location;
#endif
  GET_OBJ_VAL(obj, 0) = object.value[0];
  GET_OBJ_VAL(obj, 1) = object.value[1];
  GET_OBJ_VAL(obj, 2) = object.value[2];
  GET_OBJ_VAL(obj, 3) = object.value[3];
  GET_OBJ_EXTRA(obj) = object.extra_flags;
  GET_OBJ_WEIGHT(obj) = object.weight;
  GET_OBJ_TIMER(obj) = object.timer;
  GET_OBJ_AFFECT(obj) = object.bitvector;

  for (j = 0; j < MAX_OBJ_AFFECT; j++)
    obj->affected[j] = object.affected[j];

  return (obj);
}



int Obj_to_store(struct obj_data *obj, FILE *fl, int location)
{
  int j;
  struct obj_file_elem object;

  object.item_number = GET_OBJ_VNUM(obj);
#if USE_AUTOEQ
  object.location = location;
#endif
  object.value[0] = GET_OBJ_VAL(obj, 0);
  object.value[1] = GET_OBJ_VAL(obj, 1);
  object.value[2] = GET_OBJ_VAL(obj, 2);
  object.value[3] = GET_OBJ_VAL(obj, 3);
  object.extra_flags = GET_OBJ_EXTRA(obj);
  object.weight = GET_OBJ_WEIGHT(obj);
  object.timer = GET_OBJ_TIMER(obj);
  object.bitvector = GET_OBJ_AFFECT(obj);
  for (j = 0; j < MAX_OBJ_AFFECT; j++)
    object.affected[j] = obj->affected[j];

  if (fwrite(&object, sizeof(struct obj_file_elem), 1, fl) < 1) {
    perror("SYSERR: error writing object in Obj_to_store");
    return (0);
  }
  return (1);
}

/*
 * AutoEQ by Burkhard Knopf <burkhard.knopf@informatik.tu-clausthal.de>
 */
void auto_equip(struct char_data *ch, struct obj_data *obj, int location)
{
  int j;

  /* Lots of checks... */
  if (location > 0) {	/* Was wearing it. */
    switch (j = (location - 1)) {
    case WEAR_LIGHT:
      break;
    case WEAR_FINGER_R:
    case WEAR_FINGER_L:
      if (!CAN_WEAR(obj, ITEM_WEAR_FINGER)) /* not fitting :( */
        location = LOC_INVENTORY;
      break;
    case WEAR_NECK_1:
    case WEAR_NECK_2:
      if (!CAN_WEAR(obj, ITEM_WEAR_NECK))
        location = LOC_INVENTORY;
      break;
    case WEAR_BODY:
      if (!CAN_WEAR(obj, ITEM_WEAR_BODY))
        location = LOC_INVENTORY;
      break;
    case WEAR_HEAD:
      if (!CAN_WEAR(obj, ITEM_WEAR_HEAD))
        location = LOC_INVENTORY;
      break;
    case WEAR_LEGS:
      if (!CAN_WEAR(obj, ITEM_WEAR_LEGS))
        location = LOC_INVENTORY;
      break;
    case WEAR_FEET:
      if (!CAN_WEAR(obj, ITEM_WEAR_FEET))
        location = LOC_INVENTORY;
      break;
    case WEAR_HANDS:
      if (!CAN_WEAR(obj, ITEM_WEAR_HANDS))
        location = LOC_INVENTORY;
      break;
    case WEAR_ARMS:
      if (!CAN_WEAR(obj, ITEM_WEAR_ARMS))
        location = LOC_INVENTORY;
      break;
    case WEAR_SHIELD:
      if (!CAN_WEAR(obj, ITEM_WEAR_SHIELD))
        location = LOC_INVENTORY;
      break;
    case WEAR_ABOUT:
      if (!CAN_WEAR(obj, ITEM_WEAR_ABOUT))
        location = LOC_INVENTORY;
      break;
    case WEAR_WAIST:
      if (!CAN_WEAR(obj, ITEM_WEAR_WAIST))
        location = LOC_INVENTORY;
      break;
    case WEAR_WRIST_R:
    case WEAR_WRIST_L:
      if (!CAN_WEAR(obj, ITEM_WEAR_WRIST))
        location = LOC_INVENTORY;
      break;
    case WEAR_WIELD:
      if (!CAN_WEAR(obj, ITEM_WEAR_WIELD))
        location = LOC_INVENTORY;
      break;
    case WEAR_HOLD:
      if (CAN_WEAR(obj, ITEM_WEAR_HOLD))
	break;
      if (IS_WARRIOR(ch) && CAN_WEAR(obj, ITEM_WEAR_WIELD) && GET_OBJ_TYPE(obj) == ITEM_WEAPON)
	break;
      location = LOC_INVENTORY;
      break;
    default:
      location = LOC_INVENTORY;
    }

    if (location > 0) {	    /* Wearable. */
      if (!GET_EQ(ch,j)) {
	/*
	 * Check the characters's alignment to prevent them from being
	 * zapped through the auto-equipping.
         */
         if (invalid_align(ch, obj) || invalid_class(ch, obj))
          location = LOC_INVENTORY;
        else
          equip_char(ch, obj, j);
      } else {	/* Oops, saved a player with double equipment? */
        mudlog(BRF, LVL_IMMORT, TRUE, "SYSERR: autoeq: '%s' already equipped in position %d.", GET_NAME(ch), location);
        location = LOC_INVENTORY;
      }
    }
  }
  if (location <= 0)	/* Inventory */
    obj_to_char(obj, ch);
}


int Crash_delete_file(char *name)
{
  char filename[50];
  FILE *fl;

  if (!get_filename(filename, sizeof(filename), CRASH_FILE, name))
    return (0);
  if (!(fl = fopen(filename, "rb"))) {
    if (errno != ENOENT)	/* if it fails but NOT because of no file */
      log("SYSERR: deleting crash file %s (1): %s", filename, strerror(errno));
    return (0);
  }
  fclose(fl);

  /* if it fails, NOT because of no file */
  if (remove(filename) < 0 && errno != ENOENT)
    log("SYSERR: deleting crash file %s (2): %s", filename, strerror(errno));

  return (1);
}


int Crash_delete_crashfile(struct char_data *ch)
{
  char filename[MAX_INPUT_LENGTH];
  struct rent_info rent;
  int numread;
  FILE *fl;

  if (!get_filename(filename, sizeof(filename), CRASH_FILE, GET_NAME(ch)))
    return (0);
  if (!(fl = fopen(filename, "rb"))) {
    if (errno != ENOENT)	/* if it fails, NOT because of no file */
      log("SYSERR: checking for crash file %s (3): %s", filename, strerror(errno));
    return (0);
  }
  numread = fread(&rent, sizeof(struct rent_info), 1, fl);
  fclose(fl);

  if (numread == 0)
    return (0);

  if (rent.rentcode == RENT_CRASH)
    Crash_delete_file(GET_NAME(ch));

  return (1);
}


int Crash_clean_file(char *name)
{
  char filename[MAX_STRING_LENGTH];
  struct rent_info rent;
  int numread;
  FILE *fl;

  if (!get_filename(filename, sizeof(filename), CRASH_FILE, name))
    return (0);
  /*
   * open for write so that permission problems will be flagged now, at boot
   * time.
   */
  if (!(fl = fopen(filename, "r+b"))) {
    if (errno != ENOENT)	/* if it fails, NOT because of no file */
      log("SYSERR: OPENING OBJECT FILE %s (4): %s", filename, strerror(errno));
    return (0);
  }
  numread = fread(&rent, sizeof(struct rent_info), 1, fl);
  fclose(fl);

  if (numread == 0)
    return (0);

  if ((rent.rentcode == RENT_CRASH) ||
      (rent.rentcode == RENT_FORCED) || (rent.rentcode == RENT_TIMEDOUT)) {
    if (rent.time < time(0) - (crash_file_timeout * SECS_PER_REAL_DAY)) {
      const char *filetype;

      Crash_delete_file(name);
      switch (rent.rentcode) {
      case RENT_CRASH:
        filetype = "crash";
        break;
      case RENT_FORCED:
        filetype = "forced rent";
        break;
      case RENT_TIMEDOUT:
        filetype = "idlesave";
        break;
      default:
        filetype = "UNKNOWN!";
        break;
      }
      log("    Deleting %s's %s file.", name, filetype);
      return (1);
    }
    /* Must retrieve rented items w/in 30 days */
  } else if (rent.rentcode == RENT_RENTED)
    if (rent.time < time(0) - (rent_file_timeout * SECS_PER_REAL_DAY)) {
      Crash_delete_file(name);
      log("    Deleting %s's rent file.", name);
      return (1);
    }
  return (0);
}


void update_obj_file(void)
{
  int i;

  for (i = 0; i <= top_of_p_table; i++)
    if (*player_table[i].name)
      Crash_clean_file(player_table[i].name);
}


void Crash_listrent(struct char_data *ch, char *name)
{
  FILE *fl;
  char filename[MAX_INPUT_LENGTH];
  struct obj_file_elem object;
  struct obj_data *obj;
  struct rent_info rent;
  int numread;

  if (!get_filename(filename, sizeof(filename), CRASH_FILE, name))
    return;
  if (!(fl = fopen(filename, "rb"))) {
    send_to_char(ch, "%s has no rent file.\r\n", name);
    return;
  }
  numread = fread(&rent, sizeof(struct rent_info), 1, fl);

  /* Oops, can't get the data, punt. */
  if (numread == 0) {
    send_to_char(ch, "Error reading rent information.\r\n");
    fclose(fl);
    return;
  }

  send_to_char(ch, "%s\r\n", filename);
  switch (rent.rentcode) {
  case RENT_RENTED:
    send_to_char(ch, "Rent\r\n");
    break;
  case RENT_CRASH:
    send_to_char(ch, "Crash\r\n");
    break;
  case RENT_CRYO:
    send_to_char(ch, "Cryo\r\n");
    break;
  case RENT_TIMEDOUT:
  case RENT_FORCED:
    send_to_char(ch, "TimedOut\r\n");
    break;
  default:
    send_to_char(ch, "Undef\r\n");
    break;
  }
  while (!feof(fl)) {
    fread(&object, sizeof(struct obj_file_elem), 1, fl);
    if (ferror(fl)) {
      fclose(fl);
      return;
    }
    if (!feof(fl))
      if (real_object(object.item_number) != NOTHING) {
	obj = read_object(object.item_number, VIRTUAL);
#if USE_AUTOEQ
	send_to_char(ch, " [%5d] (%5dau) <%2d> %-20s\r\n",
		object.item_number, GET_OBJ_RENT(obj),
		object.location, obj->short_description);
#else
	send_to_char(ch, " [%5d] (%5dau) %-20s\r\n",
		object.item_number, GET_OBJ_RENT(obj),
		obj->short_description);
#endif
	extract_obj(obj);
      }
  }
  fclose(fl);
}


int Crash_write_rentcode(struct char_data *ch, FILE *fl, struct rent_info *rent)
{
  if (fwrite(rent, sizeof(struct rent_info), 1, fl) < 1) {
    perror("SYSERR: writing rent code");
    return (0);
  }
  return (1);
}


/*
 * Return values:
 *  0 - successful load, keep char in rent room.
 *  1 - load failure or load of crash items -- put char in temple.
 *  2 - rented equipment lost (no $)
 */
int Crash_load(struct char_data *ch)
{
  FILE *fl;
  char filename[MAX_STRING_LENGTH];
  struct obj_file_elem object;
  struct rent_info rent;
  int cost, orig_rent_code, num_objs = 0, j;
  float num_of_days;
  /* AutoEQ addition. */
  struct obj_data *obj, *obj2, *cont_row[MAX_BAG_ROWS];
  int location;

  /* Empty all of the container lists (you never know ...) */
  for (j = 0; j < MAX_BAG_ROWS; j++)
    cont_row[j] = NULL;

  if (!get_filename(filename, sizeof(filename), CRASH_FILE, GET_NAME(ch)))
    return (1);
  if (!(fl = fopen(filename, "r+b"))) {
    if (errno != ENOENT) {	/* if it fails, NOT because of no file */
      log("SYSERR: READING OBJECT FILE %s (5): %s", filename, strerror(errno));
      send_to_char(ch,
		"\r\n********************* NOTICE *********************\r\n"
		"There was a problem loading your objects from disk.\r\n"
		"Contact a God for assistance.\r\n");
    }
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s entering game with no equipment.", GET_NAME(ch));
    return (1);
  }
  if (!feof(fl))
    fread(&rent, sizeof(struct rent_info), 1, fl);
  else {
    log("SYSERR: Crash_load: %s's rent file was empty!", GET_NAME(ch));
    return (1);
  }

  if (rent.rentcode == RENT_RENTED || rent.rentcode == RENT_TIMEDOUT) {
    num_of_days = (float) (time(0) - rent.time) / SECS_PER_REAL_DAY;
    cost = (int) (rent.net_cost_per_diem * num_of_days);
    if (cost > GET_GOLD(ch) + GET_BANK_GOLD(ch)) {
      fclose(fl);
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s entering game, rented equipment lost (no $).", GET_NAME(ch));
      Crash_crashsave(ch);
      return (2);
    } else {
      GET_BANK_GOLD(ch) -= MAX(cost - GET_GOLD(ch), 0);
      GET_GOLD(ch) = MAX(GET_GOLD(ch) - cost, 0);
      save_char(ch);
    }
  }
  switch (orig_rent_code = rent.rentcode) {
  case RENT_RENTED:
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s un-renting and entering game.", GET_NAME(ch));
    break;
  case RENT_CRASH:
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s retrieving crash-saved items and entering game.", GET_NAME(ch));
    break;
  case RENT_CRYO:
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s un-cryo'ing and entering game.", GET_NAME(ch));
    break;
  case RENT_FORCED:
  case RENT_TIMEDOUT:
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s retrieving force-saved items and entering game.", GET_NAME(ch));
    break;
  default:
    mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,
		"SYSERR: %s entering game with undefined rent code %d.", GET_NAME(ch), rent.rentcode);
    break;
  }

  while (!feof(fl)) {
    fread(&object, sizeof(struct obj_file_elem), 1, fl);
    if (ferror(fl)) {
      perror("SYSERR: Reading crash file: Crash_load");
      fclose(fl);
      return (1);
    }
    if (feof(fl))
      break;
    ++num_objs;
    if ((obj = Obj_from_store(object, &location)) == NULL)
      continue;

    auto_equip(ch, obj, location);
    /*
     * What to do with a new loaded item:
     *
     * If there's a list with location less than 1 below this, then its
     * container has disappeared from the file so we put the list back into
     * the character's inventory. (Equipped items are 0 here.)
     *
     * If there's a list of contents with location of 1 below this, then we
     * check if it is a container:
     *   - Yes: Get it from the character, fill it, and give it back so we
     *          have the correct weight.
     *   -  No: The container is missing so we put everything back into the
     *          character's inventory.
     *
     * For items with negative location, we check if there is already a list
     * of contents with the same location.  If so, we put it there and if not,
     * we start a new list.
     *
     * Since location for contents is < 0, the list indices are switched to
     * non-negative.
     *
     * This looks ugly, but it works.
     */
    if (location > 0) {		/* Equipped */
      for (j = MAX_BAG_ROWS - 1; j > 0; j--) {
        if (cont_row[j]) {	/* No container, back to inventory. */
          for (; cont_row[j]; cont_row[j] = obj2) {
            obj2 = cont_row[j]->next_content;
            obj_to_char(cont_row[j], ch);
          }
          cont_row[j] = NULL;
        }
      }
      if (cont_row[0]) {	/* Content list existing. */
        if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
	/* Remove object, fill it, equip again. */
          obj = unequip_char(ch, location - 1);
          obj->contains = NULL;	/* Should be NULL anyway, but just in case. */
          for (; cont_row[0]; cont_row[0] = obj2) {
            obj2 = cont_row[0]->next_content;
            obj_to_obj(cont_row[0], obj);
          }
          equip_char(ch, obj, location - 1);
        } else {			/* Object isn't container, empty the list. */
          for (; cont_row[0]; cont_row[0] = obj2) {
            obj2 = cont_row[0]->next_content;
            obj_to_char(cont_row[0], ch);
          }
          cont_row[0] = NULL;
        }
      }
    } else {	/* location <= 0 */
      for (j = MAX_BAG_ROWS - 1; j > -location; j--) {
        if (cont_row[j]) {	/* No container, back to inventory. */
          for (; cont_row[j]; cont_row[j] = obj2) {
            obj2 = cont_row[j]->next_content;
            obj_to_char(cont_row[j], ch);
          }
          cont_row[j] = NULL;
        }
      }
      if (j == -location && cont_row[j]) {	/* Content list exists. */
        if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) {
		/* Take the item, fill it, and give it back. */
          obj_from_char(obj);
          obj->contains = NULL;
          for (; cont_row[j]; cont_row[j] = obj2) {
            obj2 = cont_row[j]->next_content;
            obj_to_obj(cont_row[j], obj);
          }
          obj_to_char(obj, ch);	/* Add to inventory first. */
        } else {	/* Object isn't container, empty content list. */
          for (; cont_row[j]; cont_row[j] = obj2) {
            obj2 = cont_row[j]->next_content;
            obj_to_char(cont_row[j], ch);
          }
          cont_row[j] = NULL;
        }
      }
      if (location < 0 && location >= -MAX_BAG_ROWS) {
        /*
         * Let the object be part of the content list but put it at the
         * list's end.  Thus having the items in the same order as before
         * the character rented.
         */
        obj_from_char(obj);
        if ((obj2 = cont_row[-location - 1]) != NULL) {
          while (obj2->next_content)
            obj2 = obj2->next_content;
          obj2->next_content = obj;
        } else
          cont_row[-location - 1] = obj;
      }
    }
  }

  /* Little hoarding check. -gg 3/1/98 */
  mudlog(NRM, MAX(GET_INVIS_LEV(ch), LVL_GOD), TRUE, "%s (level %d) has %d object%s (max %d).",
	GET_NAME(ch), GET_LEVEL(ch), num_objs, num_objs != 1 ? "s" : "", max_obj_save);

  /* turn this into a crash file by re-writing the control block */
  rent.rentcode = RENT_CRASH;
  rent.time = time(0);
  rewind(fl);
  Crash_write_rentcode(ch, fl, &rent);

  fclose(fl);

  if ((orig_rent_code == RENT_RENTED) || (orig_rent_code == RENT_CRYO))
    return (0);
  else
    return (1);
}



int Crash_save(struct obj_data *obj, FILE *fp, int location)
{
  struct obj_data *tmp;
  int result;

  if (obj) {
    Crash_save(obj->next_content, fp, location);
    Crash_save(obj->contains, fp, MIN(0, location) - 1);
    result = Obj_to_store(obj, fp, location);

    for (tmp = obj->in_obj; tmp; tmp = tmp->in_obj)
      GET_OBJ_WEIGHT(tmp) -= GET_OBJ_WEIGHT(obj);

    if (!result)
      return (0);
  }
  return (TRUE);
}


void Crash_restore_weight(struct obj_data *obj)
{
  if (obj) {
    Crash_restore_weight(obj->contains);
    Crash_restore_weight(obj->next_content);
    if (obj->in_obj)
      GET_OBJ_WEIGHT(obj->in_obj) += GET_OBJ_WEIGHT(obj);
  }
}

/*
 * Get !RENT items from equipment to inventory and
 * extract !RENT out of worn containers.
 */
void Crash_extract_norent_eq(struct char_data *ch)
{
  int j;

  for (j = 0; j < NUM_WEARS; j++) {
    if (GET_EQ(ch, j) == NULL)
      continue;

    if (Crash_is_unrentable(GET_EQ(ch, j)))
      obj_to_char(unequip_char(ch, j), ch);
    else
      Crash_extract_norents(GET_EQ(ch, j));
  }
}

void Crash_extract_objs(struct obj_data *obj)
{
  if (obj) {
    Crash_extract_objs(obj->contains);
    Crash_extract_objs(obj->next_content);
    extract_obj(obj);
  }
}


int Crash_is_unrentable(struct obj_data *obj)
{
  if (!obj)
    return (0);

  if (OBJ_FLAGGED(obj, ITEM_NORENT) || GET_OBJ_RENT(obj) < 0 ||
      GET_OBJ_RNUM(obj) == NOTHING || GET_OBJ_TYPE(obj) == ITEM_KEY)
    return (1);

  return (0);
}


void Crash_extract_norents(struct obj_data *obj)
{
  if (obj) {
    Crash_extract_norents(obj->contains);
    Crash_extract_norents(obj->next_content);
    if (Crash_is_unrentable(obj))
      extract_obj(obj);
  }
}


void Crash_extract_expensive(struct obj_data *obj)
{
  struct obj_data *tobj, *max;

  max = obj;
  for (tobj = obj; tobj; tobj = tobj->next_content)
    if (GET_OBJ_RENT(tobj) > GET_OBJ_RENT(max))
      max = tobj;
  extract_obj(max);
}



void Crash_calculate_rent(struct obj_data *obj, int *cost)
{
  if (obj) {
    *cost += MAX(0, GET_OBJ_RENT(obj));
    Crash_calculate_rent(obj->contains, cost);
    Crash_calculate_rent(obj->next_content, cost);
  }
}


void Crash_crashsave(struct char_data *ch)
{
  char buf[MAX_INPUT_LENGTH];
  struct rent_info rent;
  int j;
  FILE *fp;

  if (IS_NPC(ch))
    return;

  if (!get_filename(buf, sizeof(buf), CRASH_FILE, GET_NAME(ch)))
    return;
  if (!(fp = fopen(buf, "wb")))
    return;

  rent.rentcode = RENT_CRASH;
  rent.time = time(0);
  if (!Crash_write_rentcode(ch, fp, &rent)) {
    fclose(fp);
    return;
  }

  for (j = 0; j < NUM_WEARS; j++)
    if (GET_EQ(ch, j)) {
      if (!Crash_save(GET_EQ(ch, j), fp, j + 1)) {
	fclose(fp);
	return;
      }
      Crash_restore_weight(GET_EQ(ch, j));
    }

  if (!Crash_save(ch->carrying, fp, 0)) {
    fclose(fp);
    return;
  }
  Crash_restore_weight(ch->carrying);

  fclose(fp);
  REMOVE_BIT(PLR_FLAGS(ch), PLR_CRASH);
}


void Crash_idlesave(struct char_data *ch)
{
  char buf[MAX_INPUT_LENGTH];
  struct rent_info rent;
  int j;
  int cost, cost_eq;
  FILE *fp;

  if (IS_NPC(ch))
    return;

  if (!get_filename(buf, sizeof(buf), CRASH_FILE, GET_NAME(ch)))
    return;
  if (!(fp = fopen(buf, "wb")))
    return;

  Crash_extract_norent_eq(ch);
  Crash_extract_norents(ch->carrying);

  cost = 0;
  Crash_calculate_rent(ch->carrying, &cost);

  cost_eq = 0;
  for (j = 0; j < NUM_WEARS; j++)
    Crash_calculate_rent(GET_EQ(ch, j), &cost_eq);

  cost += cost_eq;
  cost *= 2;			/* forcerent cost is 2x normal rent */

  if (cost > GET_GOLD(ch) + GET_BANK_GOLD(ch)) {
    for (j = 0; j < NUM_WEARS; j++)	/* Unequip players with low gold. */
      if (GET_EQ(ch, j))
        obj_to_char(unequip_char(ch, j), ch);

    while ((cost > GET_GOLD(ch) + GET_BANK_GOLD(ch)) && ch->carrying) {
      Crash_extract_expensive(ch->carrying);
      cost = 0;
      Crash_calculate_rent(ch->carrying, &cost);
      cost *= 2;
    }
  }

  if (ch->carrying == NULL) {
    for (j = 0; j < NUM_WEARS && GET_EQ(ch, j) == NULL; j++) /* Nothing */ ;
    if (j == NUM_WEARS) {	/* No equipment or inventory. */
      fclose(fp);
      Crash_delete_file(GET_NAME(ch));
      return;
    }
  }
  rent.net_cost_per_diem = cost;

  rent.rentcode = RENT_TIMEDOUT;
  rent.time = time(0);
  rent.gold = GET_GOLD(ch);
  rent.account = GET_BANK_GOLD(ch);
  if (!Crash_write_rentcode(ch, fp, &rent)) {
    fclose(fp);
    return;
  }
  for (j = 0; j < NUM_WEARS; j++) {
    if (GET_EQ(ch, j)) {
      if (!Crash_save(GET_EQ(ch, j), fp, j + 1)) {
        fclose(fp);
        return;
      }
      Crash_restore_weight(GET_EQ(ch, j));
      Crash_extract_objs(GET_EQ(ch, j));
    }
  }
  if (!Crash_save(ch->carrying, fp, 0)) {
    fclose(fp);
    return;
  }
  fclose(fp);

  Crash_extract_objs(ch->carrying);
}


void Crash_rentsave(struct char_data *ch, int cost)
{
  char buf[MAX_INPUT_LENGTH];
  struct rent_info rent;
  int j;
  FILE *fp;

  if (IS_NPC(ch))
    return;

  if (!get_filename(buf, sizeof(buf), CRASH_FILE, GET_NAME(ch)))
    return;
  if (!(fp = fopen(buf, "wb")))
    return;

  Crash_extract_norent_eq(ch);
  Crash_extract_norents(ch->carrying);

  rent.net_cost_per_diem = cost;
  rent.rentcode = RENT_RENTED;
  rent.time = time(0);
  rent.gold = GET_GOLD(ch);
  rent.account = GET_BANK_GOLD(ch);
  if (!Crash_write_rentcode(ch, fp, &rent)) {
    fclose(fp);
    return;
  }
  for (j = 0; j < NUM_WEARS; j++)
    if (GET_EQ(ch, j)) {
      if (!Crash_save(GET_EQ(ch,j), fp, j + 1)) {
        fclose(fp);
        return;
      }
      Crash_restore_weight(GET_EQ(ch, j));
      Crash_extract_objs(GET_EQ(ch, j));
    }
  if (!Crash_save(ch->carrying, fp, 0)) {
    fclose(fp);
    return;
  }
  fclose(fp);

  Crash_extract_objs(ch->carrying);
}


void Crash_cryosave(struct char_data *ch, int cost)
{
  char buf[MAX_INPUT_LENGTH];
  struct rent_info rent;
  int j;
  FILE *fp;

  if (IS_NPC(ch))
    return;

  if (!get_filename(buf, sizeof(buf), CRASH_FILE, GET_NAME(ch)))
    return;
  if (!(fp = fopen(buf, "wb")))
    return;

  Crash_extract_norent_eq(ch);
  Crash_extract_norents(ch->carrying);

  GET_GOLD(ch) = MAX(0, GET_GOLD(ch) - cost);

  rent.rentcode = RENT_CRYO;
  rent.time = time(0);
  rent.gold = GET_GOLD(ch);
  rent.account = GET_BANK_GOLD(ch);
  rent.net_cost_per_diem = 0;
  if (!Crash_write_rentcode(ch, fp, &rent)) {
    fclose(fp);
    return;
  }
  for (j = 0; j < NUM_WEARS; j++)
    if (GET_EQ(ch, j)) {
      if (!Crash_save(GET_EQ(ch, j), fp, j + 1)) {
        fclose(fp);
        return;
      }
      Crash_restore_weight(GET_EQ(ch, j));
      Crash_extract_objs(GET_EQ(ch, j));
    }
  if (!Crash_save(ch->carrying, fp, 0)) {
    fclose(fp);
    return;
  }
  fclose(fp);

  Crash_extract_objs(ch->carrying);
  SET_BIT(PLR_FLAGS(ch), PLR_CRYO);
}


/* ************************************************************************
* Routines used for the receptionist					  *
************************************************************************* */

void Crash_rent_deadline(struct char_data *ch, struct char_data *recep,
			      long cost)
{
  char buf[256];
  long rent_deadline;

  if (!cost)
    return;

  rent_deadline = ((GET_GOLD(ch) + GET_BANK_GOLD(ch)) / cost);
  snprintf(buf, sizeof(buf), "$n tells you, 'You can rent for %ld day%s with the gold you have\r\n"
	  "on hand and in the bank.'\r\n", rent_deadline, rent_deadline != 1 ? "s" : "");
  act(buf, FALSE, recep, 0, ch, TO_VICT);
}

int Crash_report_unrentables(struct char_data *ch, struct char_data *recep,
			         struct obj_data *obj)
{
  int has_norents = 0;

  if (obj) {
    if (Crash_is_unrentable(obj)) {
      char buf[128];

      has_norents = 1;
      snprintf(buf, sizeof(buf), "$n tells you, 'You cannot store %s.'", OBJS(obj, ch));
      act(buf, FALSE, recep, 0, ch, TO_VICT);
    }
    has_norents += Crash_report_unrentables(ch, recep, obj->contains);
    has_norents += Crash_report_unrentables(ch, recep, obj->next_content);
  }
  return (has_norents);
}



void Crash_report_rent(struct char_data *ch, struct char_data *recep,
		            struct obj_data *obj, long *cost, long *nitems, int display, int factor)
{
  if (obj) {
    if (!Crash_is_unrentable(obj)) {
      (*nitems)++;
      *cost += MAX(0, (GET_OBJ_RENT(obj) * factor));
      if (display) {
        char buf[256];

	snprintf(buf, sizeof(buf), "$n tells you, '%5d coins for %s..'", GET_OBJ_RENT(obj) * factor, OBJS(obj, ch));
	act(buf, FALSE, recep, 0, ch, TO_VICT);
      }
    }
    Crash_report_rent(ch, recep, obj->contains, cost, nitems, display, factor);
    Crash_report_rent(ch, recep, obj->next_content, cost, nitems, display, factor);
  }
}



int Crash_offer_rent(struct char_data *ch, struct char_data *recep,
		         int display, int factor)
{
  int i;
  long totalcost = 0, numitems = 0, norent;

  norent = Crash_report_unrentables(ch, recep, ch->carrying);
  for (i = 0; i < NUM_WEARS; i++)
    norent += Crash_report_unrentables(ch, recep, GET_EQ(ch, i));

  if (norent)
    return (0);

  totalcost = min_rent_cost * factor;

  Crash_report_rent(ch, recep, ch->carrying, &totalcost, &numitems, display, factor);

  for (i = 0; i < NUM_WEARS; i++)
    Crash_report_rent(ch, recep, GET_EQ(ch, i), &totalcost, &numitems, display, factor);

  if (!numitems) {
    act("$n tells you, 'But you are not carrying anything!  Just quit!'",
	FALSE, recep, 0, ch, TO_VICT);
    return (0);
  }
  if (numitems > max_obj_save) {
    char buf[256];

    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, but I cannot store more than %d items.'", max_obj_save);
    act(buf, FALSE, recep, 0, ch, TO_VICT);
    return (0);
  }
  if (display) {
    char buf[256];

    snprintf(buf, sizeof(buf), "$n tells you, 'Plus, my %d coin fee..'", min_rent_cost * factor);
    act(buf, FALSE, recep, 0, ch, TO_VICT);

    snprintf(buf, sizeof(buf), "$n tells you, 'For a total of %ld coins%s.'", totalcost, factor == RENT_FACTOR ? " per day" : "");
    act(buf, FALSE, recep, 0, ch, TO_VICT);

    if (totalcost > GET_GOLD(ch) + GET_BANK_GOLD(ch)) {
      act("$n tells you, '...which I see you can't afford.'", FALSE, recep, 0, ch, TO_VICT);
      return (0);
    } else if (factor == RENT_FACTOR)
      Crash_rent_deadline(ch, recep, totalcost);
  }
  return (totalcost);
}



int gen_receptionist(struct char_data *ch, struct char_data *recep,
		         int cmd, char *arg, int mode)
{
  int cost;
  const char *action_table[] = { "smile", "dance", "sigh", "blush", "burp",
	  "cough", "fart", "twiddle", "yawn" };

  if (!cmd && !rand_number(0, 5)) {
    do_action(recep, NULL, find_command(action_table[rand_number(0, 8)]), 0);
    return (FALSE);
  }

  if (!ch->desc || IS_NPC(ch))
    return (FALSE);

  if (!CMD_IS("offer") && !CMD_IS("rent"))
    return (FALSE);

  if (!AWAKE(recep)) {
    send_to_char(ch, "%s is unable to talk to you...\r\n", HSSH(recep));
    return (TRUE);
  }

  if (!CAN_SEE(recep, ch)) {
    act("$n says, 'I don't deal with people I can't see!'", FALSE, recep, 0, 0, TO_ROOM);
    return (TRUE);
  }

  if (free_rent) {
    act("$n tells you, 'Rent is free here.  Just quit, and your objects will be saved!'",
	FALSE, recep, 0, ch, TO_VICT);
    return (1);
  }

  if (CMD_IS("rent")) {
    char buf[128];

    if (!(cost = Crash_offer_rent(ch, recep, FALSE, mode)))
      return (TRUE);
    if (mode == RENT_FACTOR)
      snprintf(buf, sizeof(buf), "$n tells you, 'Rent will cost you %d gold coins per day.'", cost);
    else if (mode == CRYO_FACTOR)
      snprintf(buf, sizeof(buf), "$n tells you, 'It will cost you %d gold coins to be frozen.'", cost);
    act(buf, FALSE, recep, 0, ch, TO_VICT);

    if (cost > GET_GOLD(ch) + GET_BANK_GOLD(ch)) {
      act("$n tells you, '...which I see you can't afford.'",
	  FALSE, recep, 0, ch, TO_VICT);
      return (TRUE);
    }
    if (cost && (mode == RENT_FACTOR))
      Crash_rent_deadline(ch, recep, cost);

    if (mode == RENT_FACTOR) {
      act("$n stores your belongings and helps you into your private chamber.", FALSE, recep, 0, ch, TO_VICT);
      Crash_rentsave(ch, cost);
      mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s has rented (%d/day, %d tot.)",
		GET_NAME(ch), cost, GET_GOLD(ch) + GET_BANK_GOLD(ch));
    } else {			/* cryo */
      act("$n stores your belongings and helps you into your private chamber.\r\n"
	  "A white mist appears in the room, chilling you to the bone...\r\n"
	  "You begin to lose consciousness...",
	  FALSE, recep, 0, ch, TO_VICT);
      Crash_cryosave(ch, cost);
      mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s has cryo-rented.", GET_NAME(ch));
      SET_BIT(PLR_FLAGS(ch), PLR_CRYO);
    }

    act("$n helps $N into $S private chamber.", FALSE, recep, 0, ch, TO_NOTVICT);

    GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
    extract_char(ch);	/* It saves. */
  } else {
    Crash_offer_rent(ch, recep, TRUE, mode);
    act("$N gives $n an offer.", FALSE, ch, 0, recep, TO_ROOM);
  }
  return (TRUE);
}


SPECIAL(receptionist)
{
  return (gen_receptionist(ch, (struct char_data *)me, cmd, argument, RENT_FACTOR));
}


SPECIAL(cryogenicist)
{
  return (gen_receptionist(ch, (struct char_data *)me, cmd, argument, CRYO_FACTOR));
}


void Crash_save_all(void)
{
  struct descriptor_data *d;
  for (d = descriptor_list; d; d = d->next) {
    if ((STATE(d) == CON_PLAYING) && !IS_NPC(d->character)) {
      if (PLR_FLAGGED(d->character, PLR_CRASH)) {
	Crash_crashsave(d->character);
	save_char(d->character);
	REMOVE_BIT(PLR_FLAGS(d->character), PLR_CRASH);
      }
    }
  }
}
