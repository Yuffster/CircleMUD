/*

   play2to3.c - CircleMUD v2.2-to-v3.0 player file converter

   June 12th, 1995

   This is a quick&dirty hack to convert a player file from CircleMUD v2.2
   to CircleMUD v3.0.  As far as I know it works and it should be portable,
   but I have only compiled and tested it under Linux 1.1.59.

   Edward Almasy
   almasy@axis.com

 */

#include "conf.h"
#include "sysdep.h"

typedef signed char sbyte;
typedef unsigned char ubyte;
typedef signed short int sh_int;
typedef unsigned short int ush_int;
typedef char bool;
typedef char byte;
typedef sh_int room_num;

#define THREE_MAX_NAME_LENGTH   20
#define THREE_MAX_PWD_LENGTH    10
#define THREE_MAX_TITLE_LENGTH  80
#define THREE_HOST_LENGTH       30
#define THREE_EXDSCR_LENGTH     240
#define THREE_MAX_TONGUE        3
#define THREE_MAX_SKILLS        200
#define THREE_MAX_AFFECT        32

struct THREE_char_ability_data {
  sbyte str;
  sbyte str_add;		/* 000 - 100 if strength 18             */
  sbyte intel;
  sbyte wis;
  sbyte dex;
  sbyte con;
  sbyte cha;
};

struct THREE_char_point_data {
  sh_int mana;
  sh_int max_mana;		/* Max mana for PC/NPC                     */
  sh_int hit;
  sh_int max_hit;		/* Max hit for PC/NPC                      */
  sh_int move;
  sh_int max_move;		/* Max move for PC/NPC                     */
  sh_int armor;			/* Internal -100..100, external -10..10 AC */
  int gold;			/* Money carried                           */
  int bank_gold;		/* Gold the char has in a bank account     */
  int exp;			/* The experience of the player            */
  sbyte hitroll;		/* Any bonus or penalty to the hit roll    */
  sbyte damroll;		/* Any bonus or penalty to the damage roll */
};

struct THREE_char_special_data_saved {
  int alignment;		/* +-1000 for alignments                */
  long idnum;			/* player's idnum; -1 for mobiles       */
  long act;			/* act flag for NPC's; player flag for PC's */
  long affected_by;		/* Bitvector for spells/skills affected by */
  sh_int apply_saving_throw[5];	/* Saving throw (Bonuses)            */
};

struct THREE_player_special_data_saved {
  byte skills[THREE_MAX_SKILLS + 1];	/* array of skills plus skill 0 */
  byte spells_to_learn;		/* How many can you learn yet this level */
  bool talks[THREE_MAX_TONGUE];	/* PC s Tongues 0 for NPC          */
  int wimp_level;		/* Below this # of hit points, flee!    */
  byte freeze_level;		/* Level of god who froze char, if any  */
  sh_int invis_level;		/* level of invisibility                */
  room_num load_room;		/* Which room to place char in          */
  long pref;			/* preference flags for PC's.           */
  ubyte bad_pws;		/* number of bad password attemps       */
  sbyte conditions[3];		/* Drunk, full, thirsty                 */
  ubyte spare0;
  ubyte spare1;
  ubyte spare2;
  ubyte spare3;
  ubyte spare4;
  ubyte spare5;
  int spare6;
  int spare7;
  int spare8;
  int spare9;
  int spare10;
  int spare11;
  int spare12;
  int spare13;
  int spare14;
  int spare15;
  int spare16;
  long spare17;
  long spare18;
  long spare19;
  long spare20;
  long spare21;
};

/* An affect structure.  Used in char_file_u *DO*NOT*CHANGE* */
struct THREE_affected_type {
  sh_int type;			/* The type of spell that caused this      */
  sh_int duration;		/* For how long its effects will last      */
  sbyte modifier;		/* This is added to apropriate ability     */
  byte location;		/* Tells which ability to change(APPLY_XXX) */
  long bitvector;		/* Tells which bits to set (AFF_XXX)       */

  struct THREE_affected_type *next;
};

struct THREE_char_file_u {
  char name[THREE_MAX_NAME_LENGTH + 1];
  char description[THREE_EXDSCR_LENGTH];
  char title[THREE_MAX_TITLE_LENGTH + 1];
  byte sex;
  byte class;
  byte level;
  sh_int hometown;
  time_t birth;			/* Time of birth of character     */
  int played;			/* Number of secs played in total */
  ubyte weight;
  ubyte height;
  char pwd[THREE_MAX_PWD_LENGTH + 1];	/* character's password */

  struct THREE_char_special_data_saved char_specials_saved;
  struct THREE_player_special_data_saved player_specials_saved;
  struct THREE_char_ability_data abilities;
  struct THREE_char_point_data points;
  struct THREE_affected_type affected[THREE_MAX_AFFECT];

  time_t last_logon;		/* Time (in secs) of last logon */
  char host[THREE_HOST_LENGTH + 1];	/* host of last logon */
};

#define TWO_MAX_PWD_LENGTH  10
#define TWO_HOST_LEN        30
#define TWO_MAX_TOUNGE      3
#define TWO_MAX_SKILLS      128
#define TWO_MAX_AFFECT      32

struct TWO_char_ability_data {
  sbyte str;
  sbyte str_add;		/* 000 - 100 if strength 18             */
  sbyte intel;
  sbyte wis;
  sbyte dex;
  sbyte con;
};

struct TWO_char_point_data {
  sh_int mana;
  sh_int max_mana;		/* Max move for PC/NPC                     */
  sh_int hit;
  sh_int max_hit;		/* Max hit for PC/NPC                      */
  sh_int move;
  sh_int max_move;		/* Max move for PC/NPC                     */
  sh_int armor;			/* Internal -100..100, external -10..10 AC */
  int gold;			/* Money carried                           */
  int bank_gold;		/* Gold the char has in a bank account     */
  int exp;			/* The experience of the player            */
  sbyte hitroll;		/* Any bonus or penalty to the hit roll    */
  sbyte damroll;		/* Any bonus or penalty to the damage roll */
};

struct TWO_char_special2_data {
  long idnum;			/* player's idnum                       */
  sh_int load_room;		/* Which room to place char in          */
  byte spells_to_learn;		/* How many can you learn yet this level */
  int alignment;		/* +-1000 for alignments                */
  long act;			/* act flag for NPC's; player flag for PC's */
  long pref;			/* preference flags for PC's.           */
  int wimp_level;		/* Below this # of hit points, flee!    */
  byte freeze_level;		/* Level of god who froze char, if any  */
  ubyte bad_pws;		/* number of bad password attemps       */
  sh_int apply_saving_throw[5];	/* Saving throw (Bonuses)              */
  sbyte conditions[3];		/* Drunk full etc.                      */
  ubyte spare0;
  ubyte spare1;
  ubyte spare2;
  ubyte spare3;
  ubyte spare4;
  ubyte spare5;
  ubyte spare6;
  ubyte spare7;
  ubyte spare8;
  ubyte spare9;
  ubyte spare10;
  ubyte spare11;
  long spare12;
  long spare13;
  long spare14;
  long spare15;
  long spare16;
  long spare17;
  long spare18;
  long spare19;
  long spare20;
  long spare21;
};

/* Used in CHAR_FILE_U *DO*NOT*CHANGE* */
struct TWO_affected_type {
  sbyte type;			/* The type of spell that caused this      */
  sh_int duration;		/* For how long its effects will last      */
  sbyte modifier;		/* This is added to apropriate ability     */
  byte location;		/* Tells which ability to change(APPLY_XXX) */
  long bitvector;		/* Tells which bits to set (AFF_XXX)       */
  struct TWO_affected_type *next;
};

struct TWO_char_file_u {
  byte sex;
  byte class;
  byte level;
  time_t birth;			/* Time of birth of character     */
  int played;			/* Number of secs played in total */
  ubyte weight;
  ubyte height;
  char title[80];
  sh_int hometown;
  char description[240];
  bool talks[TWO_MAX_TOUNGE];
  struct TWO_char_ability_data abilities;
  struct TWO_char_point_data points;
  byte skills[TWO_MAX_SKILLS];
  struct TWO_affected_type affected[TWO_MAX_AFFECT];
  struct TWO_char_special2_data specials2;
  time_t last_logon;		/* Time (in secs) of last logon */
  char host[TWO_HOST_LEN + 1];	/* host of last logon */
  char name[20];
  char pwd[TWO_MAX_PWD_LENGTH + 1];
};


int main(int argc, char *argv[])
{
  struct TWO_char_file_u stTwo;
  struct THREE_char_file_u stThree;
  FILE *ptTwoHndl;
  FILE *ptThreeHndl;
  int iIndex;
  char *apcClassAbbrev[] =
  {"Mu", "Cl", "Th", "Wa"};
  int aiSkillMappings[] =
  {138, 133, 139, 131, 135, 134, 132, 137};

  if (argc < 3) {
    printf("usage: play2to3 [old 2.2 player file] [new 3.0 player file]\n");
    exit(1);
  }
  ptTwoHndl = fopen(argv[1], "rb");
  if (ptTwoHndl == NULL) {
    printf("unable to open source file \"%s\"\n", argv[1]);
    exit(1);
  }
  ptThreeHndl = fopen(argv[2], "wb");
  if (ptThreeHndl == NULL) {
    printf("unable to open destination file \"%s\"\n", argv[2]);
    exit(1);
  }
  while (!feof(ptTwoHndl)) {
    fread(&stTwo, sizeof(struct TWO_char_file_u), 1, ptTwoHndl);

    strcpy(stThree.name, stTwo.name);
    strcpy(stThree.description, stTwo.description);
    strcpy(stThree.title, stTwo.title);
    stThree.sex = stTwo.sex;
    stThree.class = stTwo.class - 1;
    stThree.level = stTwo.level;
    stThree.hometown = stTwo.hometown;
    stThree.birth = stTwo.birth;
    stThree.played = stTwo.played;
    stThree.weight = stTwo.weight;
    stThree.height = stTwo.height;
    strcpy(stThree.pwd, stTwo.pwd);

    stThree.char_specials_saved.alignment = stTwo.specials2.alignment;
    stThree.char_specials_saved.idnum = stTwo.specials2.idnum;
    stThree.char_specials_saved.act = stTwo.specials2.act;
    stThree.char_specials_saved.affected_by = 0;	/* ??? */
    for (iIndex = 0; iIndex < 5; iIndex++)
      stThree.char_specials_saved.apply_saving_throw[iIndex] =
	  stTwo.specials2.apply_saving_throw[iIndex];

    for (iIndex = 0; iIndex < THREE_MAX_SKILLS; iIndex++)
      stThree.player_specials_saved.skills[iIndex] = 0;
    for (iIndex = 0; iIndex < 53; iIndex++)
      if (iIndex > 44) {
	stThree.player_specials_saved.skills[
		    aiSkillMappings[iIndex - 45]] = stTwo.skills[iIndex];
      } else
	stThree.player_specials_saved.skills[iIndex] =
	    stTwo.skills[iIndex];
    stThree.player_specials_saved.spells_to_learn =
	stTwo.specials2.spells_to_learn;
    for (iIndex = 0; iIndex < THREE_MAX_TONGUE; iIndex++)
      stThree.player_specials_saved.talks[iIndex] = stTwo.talks[iIndex];
    stThree.player_specials_saved.wimp_level = stTwo.specials2.wimp_level;
    stThree.player_specials_saved.freeze_level =
	stTwo.specials2.freeze_level;
    stThree.player_specials_saved.invis_level = 0;
    stThree.player_specials_saved.load_room = stTwo.specials2.load_room;
    stThree.player_specials_saved.pref = stTwo.specials2.pref;
    stThree.player_specials_saved.bad_pws = stTwo.specials2.bad_pws;
    for (iIndex = 0; iIndex < 3; iIndex++)
      stThree.player_specials_saved.conditions[iIndex] =
	  stTwo.specials2.conditions[iIndex];
    stThree.player_specials_saved.spare0 = 0;
    stThree.player_specials_saved.spare1 = 0;
    stThree.player_specials_saved.spare2 = 0;
    stThree.player_specials_saved.spare3 = 0;
    stThree.player_specials_saved.spare4 = 0;
    stThree.player_specials_saved.spare5 = 0;
    stThree.player_specials_saved.spare6 = 0;
    stThree.player_specials_saved.spare7 = 0;
    stThree.player_specials_saved.spare8 = 0;
    stThree.player_specials_saved.spare9 = 0;
    stThree.player_specials_saved.spare10 = 0;
    stThree.player_specials_saved.spare11 = 0;
    stThree.player_specials_saved.spare12 = 0;
    stThree.player_specials_saved.spare13 = 0;
    stThree.player_specials_saved.spare14 = 0;
    stThree.player_specials_saved.spare15 = 0;
    stThree.player_specials_saved.spare16 = 0;
    stThree.player_specials_saved.spare17 = 0;
    stThree.player_specials_saved.spare18 = 0;
    stThree.player_specials_saved.spare19 = 0;
    stThree.player_specials_saved.spare20 = 0;
    stThree.player_specials_saved.spare21 = 0;

    stThree.abilities.str = stTwo.abilities.str;
    stThree.abilities.str_add = stTwo.abilities.str_add;
    stThree.abilities.intel = stTwo.abilities.intel;
    stThree.abilities.wis = stTwo.abilities.wis;
    stThree.abilities.dex = stTwo.abilities.dex;
    stThree.abilities.con = stTwo.abilities.con;
    stThree.abilities.cha = 12;

    stThree.points.mana = stTwo.points.mana;
    stThree.points.max_mana = stTwo.points.max_mana;
    stThree.points.hit = stTwo.points.hit;
    stThree.points.max_hit = stTwo.points.max_hit;
    stThree.points.move = stTwo.points.move;
    stThree.points.max_move = stTwo.points.max_move;
    stThree.points.armor = stTwo.points.armor;
    stThree.points.gold = stTwo.points.gold;
    stThree.points.bank_gold = stTwo.points.bank_gold;
    stThree.points.exp = stTwo.points.exp;
    stThree.points.hitroll = stTwo.points.hitroll;
    stThree.points.damroll = stTwo.points.damroll;

    for (iIndex = 0; iIndex < TWO_MAX_AFFECT; iIndex++) {
      stThree.affected[iIndex].type = stTwo.affected[iIndex].type;
      stThree.affected[iIndex].duration = stTwo.affected[iIndex].duration;
      stThree.affected[iIndex].modifier = stTwo.affected[iIndex].modifier;
      stThree.affected[iIndex].location = stTwo.affected[iIndex].location;
      stThree.affected[iIndex].bitvector = stTwo.affected[iIndex].bitvector;
      stThree.affected[iIndex].next = NULL;
    }

    stThree.last_logon = stTwo.last_logon;
    strcpy(stThree.host, stTwo.host);

    printf("[%2d %s] %s %s\n",
	   stThree.level, apcClassAbbrev[(int)stThree.class],
	   stThree.name, stThree.title);

    fwrite(&stThree, sizeof(struct THREE_char_file_u), 1, ptThreeHndl);
  }

  fclose(ptThreeHndl);
  fclose(ptTwoHndl);

  return (0);
}
