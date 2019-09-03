// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include <iostream>
#include <fstream>
#include <functional>
#include <vector>
#include <unordered_map>
#include <locale>

#ifdef WIN32
	#include <conio.h>
#else
	#include <termios.h>
	#include <unistd.h>
	#include <stdio.h>
#endif

#include <yaml-cpp/yaml.h>

#include "../common/core.hpp"
#include "../common/malloc.hpp"
#include "../common/mmo.hpp"
#include "../common/showmsg.hpp"
#include "../common/strlib.hpp"
#include "../common/utilities.hpp"

// Only for constants - do not use functions of it or linking will fail
#include "../map/itemdb.hpp"
#include "../map/map.hpp"
#include "../map/mob.hpp" // MAX_MVP_DROP and MAX_MOB_DROP
#include "../map/pc.hpp"

using namespace rathena;

#ifndef WIN32
int getch( void ){
    struct termios oldattr, newattr;
    int ch;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return ch;
}
#endif

// Required constant and structure definitions
#define MAX_GUILD_SKILL_REQUIRE 5

std::unordered_map<item_types, std::string> um_itemtypenames{
	{ IT_HEALING, "IT_HEALING" },
	{ IT_USABLE, "IT_USABLE" },
	{ IT_ETC, "IT_ETC" },
	{ IT_ARMOR, "IT_ARMOR" },
	{ IT_WEAPON, "IT_WEAPON" },
	{ IT_CARD, "IT_CARD" },
	{ IT_PETEGG, "IT_PETEGG" },
	{ IT_PETARMOR, "IT_PETARMOR" },
	{ IT_AMMO, "IT_AMMO" },
	{ IT_DELAYCONSUME, "IT_DELAYCONSUME" },
	{ IT_SHADOWGEAR, "IT_SHADOWGEAR" },
	{ IT_CASH, "IT_CASH" },
};

std::unordered_map<weapon_type, std::string> um_weapontypenames{
	{ W_FIST, "W_FIST" },
	{ W_DAGGER, "W_DAGGER" },
	{ W_1HSWORD, "W_1HSWORD" },
	{ W_2HSWORD, "W_2HSWORD" },
	{ W_1HSPEAR, "W_1HSPEAR" },
	{ W_2HSPEAR, "W_2HSPEAR" },
	{ W_1HAXE, "W_1HAXE" },
	{ W_2HAXE, "W_2HAXE" },
	{ W_MACE, "W_MACE" },
	{ W_2HMACE, "W_2HMACE" },
	{ W_STAFF, "W_STAFF" },
	{ W_BOW, "W_BOW" },
	{ W_KNUCKLE, "W_KNUCKLE" },
	{ W_MUSICAL, "W_MUSICAL" },
	{ W_WHIP, "W_WHIP" },
	{ W_BOOK, "W_BOOK" },
	{ W_KATAR, "W_KATAR" },
	{ W_REVOLVER, "W_REVOLVER" },
	{ W_RIFLE, "W_RIFLE" },
	{ W_GATLING, "W_GATLING" },
	{ W_SHOTGUN, "W_SHOTGUN" },
	{ W_GRENADE, "W_GRENADE" },
	{ W_HUUMA, "W_HUUMA" },
	{ W_2HSTAFF, "W_2HSTAFF" },
};

std::unordered_map<ammo_type, std::string> um_ammotypenames{
	{ AMMO_ARROW, "AMMO_ARROW" },
	{ AMMO_DAGGER, "AMMO_DAGGER" },
	{ AMMO_BULLET, "AMMO_BULLET" },
	{ AMMO_SHELL, "AMMO_SHELL" },
	{ AMMO_GRENADE, "AMMO_GRENADE" },
	{ AMMO_SHURIKEN, "AMMO_SHURIKEN" },
	{ AMMO_KUNAI, "AMMO_KUNAI" },
	{ AMMO_CANNONBALL, "AMMO_CANNONBALL" },
	{ AMMO_THROWWEAPON, "AMMO_THROWWEAPON" },
};

struct s_item_flag {
	bool buyingstore, dead_branch, group, guid, broadcast, bindOnEquip, delay_consume;
	e_item_drop_effect dropEffect;
};

struct s_item_delay {
	uint32 delay;
	std::string sc;
};

struct s_item_stack {
	uint16 amount;
	bool inventory, cart, storage, guild_storage;
};

struct s_item_nouse {
	uint16 override;
	bool sitting;
};

struct s_item_trade {
	uint16 override;
	bool drop, trade, trade_partner, sell, cart, storage, guild_storage, mail, auction;
};

// Forward declaration of conversion functions
static bool guild_read_guildskill_tree_db( char* split[], int columns, int current );
static size_t pet_read_db( const char* file, std::ofstream &out );
static bool itemdb_read_buyingstore(char* fields[], int columns, int current);
static bool itemdb_read_flag(char* fields[], int columns, int current);
static bool itemdb_read_itemdelay(char* str[], int columns, int current);
static bool itemdb_read_stack(char* fields[], int columns, int current);
static bool itemdb_read_nouse(char* fields[], int columns, int current);
static bool itemdb_read_itemtrade(char* fields[], int columns, int current);
static size_t itemdb_read_db(const char *file, std::ofstream &out);

// Pre-loaded databases for item database merge
std::unordered_map<int, bool> item_buyingstore;
std::unordered_map<int, s_item_flag> item_flag;
std::unordered_map<int, s_item_delay> item_delay;
std::unordered_map<int, s_item_stack> item_stack;
std::unordered_map<int, s_item_nouse> item_nouse;
std::unordered_map<int, s_item_trade> item_trade;

// Constants for conversion
std::unordered_map<uint16, std::string> aegis_itemnames;
std::unordered_map<uint16, std::string> aegis_mobnames;
std::unordered_map<uint16, std::string> aegis_skillnames;

// Forward declaration of constant loading functions
static bool parse_item_constants( const char* path );
static bool parse_mob_constants( char* split[], int columns, int current );
static bool parse_skill_constants( char* split[], int columns, int current );

bool fileExists( const std::string& path );
bool writeToFile( const YAML::Node& node, const std::string& path );
void prepareHeader( YAML::Node& node, const std::string& type, uint32 version );
bool askConfirmation( const char* fmt, ... );

YAML::Node body;
size_t counter;

template<typename Func>
bool process( const std::string& type, uint32 version, const std::vector<std::string>& paths, const std::string& name, Func lambda ){
	for( const std::string& path : paths ){
		const std::string name_ext = name + ".txt";
		const std::string from = path + "/" + name_ext;
		const std::string to = path + "/" + name + ".yml";

		if( fileExists( from ) ){
			if( !askConfirmation( "Found the file \"%s\", which requires migration to yml.\nDo you want to convert it now? (Y/N)\n", from.c_str() ) ){
				continue;
			}

			YAML::Node root;

			prepareHeader( root, type, version );
			body.reset();
			counter = 0;

			if( fileExists( to ) ){
				if( !askConfirmation( "The file \"%s\" already exists.\nDo you want to replace it? (Y/N)\n", to.c_str() ) ){
					continue;
				}
			}

			std::ofstream out;

			out.open(to);

			if (!out.is_open()) {
				ShowError("Can not open file \"%s\" for writing.\n", to.c_str());
				return false;
			}

			if (!lambda(path, name_ext, out)) {
				return false;
			}

			/*
			root["Body"] = body;

			if( !writeToFile( root, to ) ){
				ShowError( "Failed to write the converted yml data to \"%s\".\nAborting now...\n", to.c_str() );
				return false;
			}
			*/

			
			// TODO: do you want to delete/rename?
		}
	}

	return true;
}

int do_init( int argc, char** argv ){
	const std::string path_db = std::string( db_path );
	const std::string path_db_mode = path_db + "/" + DBPATH;
	const std::string path_db_import = path_db + "/" + DBIMPORT + "/";

	// Loads required conversion constants
	parse_item_constants( ( path_db_mode + "item_db.txt" ).c_str() );
	parse_item_constants( ( path_db_import + "/item_db.txt" ).c_str() );
	sv_readdb( path_db_mode.c_str(), "mob_db.txt", ',', 31 + 2 * MAX_MVP_DROP + 2 * MAX_MOB_DROP, 31 + 2 * MAX_MVP_DROP + 2 * MAX_MOB_DROP, -1, &parse_mob_constants, false );
	sv_readdb( path_db_import.c_str(), "mob_db.txt", ',', 31 + 2 * MAX_MVP_DROP + 2 * MAX_MOB_DROP, 31 + 2 * MAX_MVP_DROP + 2 * MAX_MOB_DROP, -1, &parse_mob_constants, false );
	sv_readdb( path_db_mode.c_str(), "skill_db.txt", ',', 18, 18, -1, parse_skill_constants, false );
	sv_readdb( path_db_import.c_str(), "skill_db.txt", ',', 18, 18, -1, parse_skill_constants, false );

	// Pre-loads required item database data
	if (fileExists(path_db_mode + "item_db.txt")) {
		sv_readdb(path_db_mode.c_str(), "item_buyingstore.txt", ',', 1, 1, -1, &itemdb_read_buyingstore, false);
		sv_readdb(path_db_mode.c_str(), "item_flag.txt", ',', 2, 2, -1, &itemdb_read_flag, false);
		sv_readdb(path_db_mode.c_str(), "item_delay.txt", ',', 2, 3, -1, &itemdb_read_itemdelay, false);
		sv_readdb(path_db_mode.c_str(), "item_stack.txt", ',', 3, 3, -1, &itemdb_read_stack, false);
		sv_readdb(path_db.c_str(), "item_nouse.txt", ',', 3, 3, -1, &itemdb_read_nouse, false);
		sv_readdb(path_db_mode.c_str(), "item_trade.txt", ',', 3, 3, -1, &itemdb_read_itemtrade, false);
	}
	if (fileExists(path_db_import + "item_db.txt")) {
		sv_readdb(path_db_import.c_str(), "item_buyingstore.txt", ',', 1, 1, -1, &itemdb_read_buyingstore, false);
		sv_readdb(path_db_import.c_str(), "item_flag.txt", ',', 2, 2, -1, &itemdb_read_flag, false);
		sv_readdb(path_db_import.c_str(), "item_delay.txt", ',', 2, 3, -1, &itemdb_read_itemdelay, false);
		sv_readdb(path_db_import.c_str(), "item_stack.txt", ',', 3, 3, -1, &itemdb_read_stack, false);
		sv_readdb(path_db_import.c_str(), "item_trade.txt", ',', 3, 3, -1, &itemdb_read_itemtrade, false);
	}

	std::vector<std::string> guild_skill_tree_paths = {
		path_db,
		path_db_import
	};

	if( !process( "GUILD_SKILL_TREE_DB", 1, guild_skill_tree_paths, "guild_skill_tree", []( const std::string& path, const std::string& name_ext, std::ofstream &out ) -> bool {
		return sv_readdb( path.c_str(), name_ext.c_str(), ',', 2 + MAX_GUILD_SKILL_REQUIRE * 2, 2 + MAX_GUILD_SKILL_REQUIRE * 2, -1, &guild_read_guildskill_tree_db, false );
	} ) ){
		return 0;
	}

	std::vector<std::string> pet_paths = {
		path_db_mode,
		path_db_import
	};

	if( !process( "PET_DB", 1, pet_paths, "pet_db", []( const std::string& path, const std::string& name_ext, std::ofstream &out ) -> bool {
		return pet_read_db( ( path + name_ext ).c_str(), out );
	} ) ){
		return 0;
	}

	std::vector<std::string> itemdb_paths = {
		path_db_mode,
		path_db_import
	};

	if (!process("ITEM_DB", 1, itemdb_paths, "item_db", [](const std::string& path, const std::string& name_ext, std::ofstream &out) -> bool {
		return itemdb_read_db((path + name_ext).c_str(), out);
	})) {
		return 0;
	}

	// TODO: add implementations ;-)

	return 0;
}

void do_final(void){
}

bool fileExists( const std::string& path ){
	std::ifstream in;

	in.open( path );

	if( in.is_open() ){
		in.close();

		return true;
	}else{
		return false;
	}
}

bool writeToFile( const YAML::Node& node, const std::string& path ){
	std::ofstream out;

	out.open( path );

	if( !out.is_open() ){
		ShowError( "Can not open file \"%s\" for writing.\n", path.c_str() );
		return false;
	}

	out << node;

	// Make sure there is an empty line at the end of the file for git
#ifdef WIN32
	out << "\r\n";
#else
	out << "\n";
#endif

	out.close();

	return true;
}

void prepareHeader( YAML::Node& node, const std::string& type, uint32 version ){
	YAML::Node header;

	header["Type"] = type;
	header["Version"] = version;

	node["Header"] = header;
}

bool askConfirmation( const char* fmt, ... ){
	va_list ap;

	va_start( ap, fmt );

	_vShowMessage( MSG_NONE, fmt, ap );

	va_end( ap );

	char c = getch();

	if( c == 'Y' || c == 'y' ){
		return true;
	}else{
		return false;
	}
}

// Constant loading functions
static bool parse_item_constants( const char* path ){
	uint32 lines = 0, count = 0;
	char line[1024];

	FILE* fp;

	fp = fopen(path, "r");
	if (fp == NULL) {
		ShowWarning("itemdb_readdb: File not found \"%s\", skipping.\n", path);
		return false;
	}

	// process rows one by one
	while (fgets(line, sizeof(line), fp))
	{
		char *str[32], *p;
		int i;
		lines++;
		if (line[0] == '/' && line[1] == '/')
			continue;
		memset(str, 0, sizeof(str));

		p = strstr(line, "//");

		if (p != nullptr) {
			*p = '\0';
		}

		p = line;
		while (ISSPACE(*p))
			++p;
		if (*p == '\0')
			continue;// empty line
		for (i = 0; i < 19; ++i)
		{
			str[i] = p;
			p = strchr(p, ',');
			if (p == NULL)
				break;// comma not found
			*p = '\0';
			++p;
		}

		if (p == NULL)
		{
			ShowError("itemdb_readdb: Insufficient columns in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
			continue;
		}

		// Script
		if (*p != '{')
		{
			ShowError("itemdb_readdb: Invalid format (Script column) in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
			continue;
		}
		str[19] = p + 1;
		p = strstr(p + 1, "},");
		if (p == NULL)
		{
			ShowError("itemdb_readdb: Invalid format (Script column) in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
			continue;
		}
		*p = '\0';
		p += 2;

		// OnEquip_Script
		if (*p != '{')
		{
			ShowError("itemdb_readdb: Invalid format (OnEquip_Script column) in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
			continue;
		}
		str[20] = p + 1;
		p = strstr(p + 1, "},");
		if (p == NULL)
		{
			ShowError("itemdb_readdb: Invalid format (OnEquip_Script column) in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
			continue;
		}
		*p = '\0';
		p += 2;

		// OnUnequip_Script (last column)
		if (*p != '{')
		{
			ShowError("itemdb_readdb: Invalid format (OnUnequip_Script column) in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
			continue;
		}
		str[21] = p;
		p = &str[21][strlen(str[21]) - 2];

		if (*p != '}') {
			/* lets count to ensure it's not something silly e.g. a extra space at line ending */
			int lcurly = 0, rcurly = 0;

			for (size_t v = 0; v < strlen(str[21]); v++) {
				if (str[21][v] == '{')
					lcurly++;
				else if (str[21][v] == '}') {
					rcurly++;
					p = &str[21][v];
				}
			}

			if (lcurly != rcurly) {
				ShowError("itemdb_readdb: Mismatching curly braces in line %d of \"%s\" (item with id %d), skipping.\n", lines, path, atoi(str[0]));
				continue;
			}
		}
		str[21] = str[21] + 1;  //skip the first left curly
		*p = '\0';              //null the last right curly

		uint16 item_id = atoi( str[0] );
		char* name = trim( str[1] );

		aegis_itemnames[item_id] = std::string(name);

		count++;
	}

	fclose(fp);

	ShowStatus("Done reading '" CL_WHITE "%u" CL_RESET "' entries in '" CL_WHITE "%s" CL_RESET "'.\n", count, path);

	return true;
}

static bool parse_mob_constants( char* split[], int columns, int current ){
	uint16 mob_id = atoi( split[0] );
	char* name = trim( split[1] );

	aegis_mobnames[mob_id] = std::string( name );

	return true;
}

static bool parse_skill_constants( char* split[], int columns, int current ){
	uint16 skill_id = atoi( split[0] );
	char* name = trim( split[16] );

	aegis_skillnames[skill_id] = std::string( name );

	return true;
}

// Implementation of the conversion functions

// Copied and adjusted from guild.cpp
// <skill id>,<max lv>,<req id1>,<req lv1>,<req id2>,<req lv2>,<req id3>,<req lv3>,<req id4>,<req lv4>,<req id5>,<req lv5>
static bool guild_read_guildskill_tree_db( char* split[], int columns, int current ){
	YAML::Node node;

	uint16 skill_id = (uint16)atoi(split[0]);

	std::string* name = util::umap_find( aegis_skillnames, skill_id );

	if( name == nullptr ){
		ShowError( "Skill name for skill id %hu is not known.\n", skill_id );
		return false;
	}

	node["Id"] = *name;
	node["MaxLevel"] = (uint16)atoi(split[1]);

	for( int i = 0, j = 0; i < MAX_GUILD_SKILL_REQUIRE; i++ ){
		uint16 required_skill_id = atoi( split[i * 2 + 2] );
		uint16 required_skill_level = atoi( split[i * 2 + 3] );

		if( required_skill_id == 0 || required_skill_level == 0 ){
			continue;
		}

		std::string* required_name = util::umap_find( aegis_skillnames, required_skill_id );

		if( required_name == nullptr ){
			ShowError( "Skill name for required skill id %hu is not known.\n", required_skill_id );
			return false;
		}

		YAML::Node req;

		req["Id"] = *required_name;
		req["Level"] = required_skill_level;

		node["Required"][j++] = req;
	}

	body[counter++] = node;

	return true;
}

// Copied and adjusted from pet.cpp
static size_t pet_read_db( const char* file, std::ofstream &out ){
	FILE* fp = fopen( file, "r" );

	if( fp == nullptr ){
		ShowError( "can't read %s\n", file );
		return 0;
	}

	int lines = 0;
	size_t entries = 0;
	char line[1024];

	while( fgets( line, sizeof(line), fp ) ) {
		char *str[22], *p;
		unsigned k;
		lines++;

		if(line[0] == '/' && line[1] == '/')
			continue;

		memset(str, 0, sizeof(str));
		p = line;

		while( ISSPACE(*p) )
			++p;

		if( *p == '\0' )
			continue; // empty line

		for( k = 0; k < 20; ++k ) {
			str[k] = p;
			p = strchr(p,',');

			if( p == NULL )
				break; // comma not found

			*p = '\0';
			++p;
		}

		if( p == NULL ) {
			ShowError("read_petdb: Insufficient columns in line %d, skipping.\n", lines);
			continue;
		}

		// Pet Script
		if( *p != '{' ) {
			ShowError("read_petdb: Invalid format (Pet Script column) in line %d, skipping.\n", lines);
			continue;
		}

		str[20] = p;
		p = strstr(p+1,"},");

		if( p == NULL ) {
			ShowError("read_petdb: Invalid format (Pet Script column) in line %d, skipping.\n", lines);
			continue;
		}

		p[1] = '\0';
		p += 2;

		// Equip Script
		if( *p != '{' ) {
			ShowError("read_petdb: Invalid format (Equip Script column) in line %d, skipping.\n", lines);
			continue;
		}

		str[21] = p;

		uint16 mob_id = atoi( str[0] );
		std::string* mob_name = util::umap_find( aegis_mobnames, mob_id );

		if( mob_name == nullptr ){
			ShowWarning( "pet_db reading: Invalid mob-class %hu, pet not read.\n", mob_id );
			continue;
		}

		YAML::Node node;

		node["Mob"] = *mob_name;

		uint16 tame_item_id = (uint16)atoi( str[3] );

		if( tame_item_id > 0 ){
			std::string* tame_item_name = util::umap_find( aegis_itemnames, tame_item_id );

			if( tame_item_name == nullptr ){
				ShowError( "Item name for item id %hu is not known.\n", tame_item_id );
				return false;
			}

			node["TameItem"] = *tame_item_name;
		}

		uint16 egg_item_id = (uint16)atoi( str[4] );

		std::string* egg_item_name = util::umap_find( aegis_itemnames, egg_item_id );

		if( egg_item_name == nullptr ){
			ShowError( "Item name for item id %hu is not known.\n", egg_item_id );
			return false;
		}

		node["EggItem"] = *egg_item_name;

		uint16 equip_item_id = (uint16)atoi( str[5] );

		if( equip_item_id > 0 ){
			std::string* equip_item_name = util::umap_find( aegis_itemnames, equip_item_id );

			if( equip_item_name == nullptr ){
				ShowError( "Item name for item id %hu is not known.\n", equip_item_id );
				return false;
			}

			node["EquipItem"] = *equip_item_name;
		}

		uint16 food_item_id = (uint16)atoi( str[6] );

		if( food_item_id > 0 ){
			std::string* food_item_name = util::umap_find( aegis_itemnames, food_item_id );

			if( food_item_name == nullptr ){
				ShowError( "Item name for item id %hu is not known.\n", food_item_id );
				return false;
			}

			node["FoodItem"] = *food_item_name;
		}

		node["Fullness"] = atoi( str[7] );
		// Default: 60
		if( atoi( str[8] ) != 60 ){
			node["HungryDelay"] = atoi( str[8] );
		}
		// Default: 250
		if( atoi( str[11] ) != 250 ){
			node["IntimacyStart"] = atoi( str[11] );
		}
		node["IntimacyFed"] = atoi( str[9] );
		// Default: -100
		if( atoi( str[10] ) != 100 ){
			node["IntimacyOverfed"] = -atoi( str[10] );
		}
		// pet_hungry_friendly_decrease battle_conf
		//node["IntimacyHungry"] = -5;
		// Default: -20
		if( atoi( str[12] ) != 20 ){
			node["IntimacyOwnerDie"] = -atoi( str[12] );
		}
		node["CaptureRate"] = atoi( str[13] );
		// Default: true
		if( atoi( str[15] ) == 0 ){
			node["SpecialPerformance"] = false;
		}
		node["AttackRate"] = atoi( str[17] );
		node["RetaliateRate"] = atoi( str[18] );
		node["ChangeTargetRate"] = atoi( str[19] );

		if( *str[21] ){
			node["Script"] = str[21];
		}

		if( *str[20] ){
			node["SupportScript"] = str[20];
		}

		body[counter++] = node;

		entries++;
	}

	fclose(fp);
	ShowStatus("Done reading '" CL_WHITE "%d" CL_RESET "' pets in '" CL_WHITE "%s" CL_RESET "'.\n", entries, file );

	return entries;
}

// Copied and adjusted from item_db.cpp
static void itemdb_re_split_atoi(char *str, int *val1, int *val2) {
	int i, val[2];

	for (i = 0; i<2; i++) {
		if (!str) break;
		val[i] = atoi(str);
		str = strchr(str, ':');
		if (str)
			*str++ = 0;
	}
	if (i == 0) {
		*val1 = *val2 = 0;
		return;//no data found
	}
	if (i == 1) {//Single Value
		*val1 = val[0];
		*val2 = 0;
		return;
	}
	//We assume we have 2 values.
	*val1 = val[0];
	*val2 = val[1];
	return;
}

static bool itemdb_read_buyingstore(char* fields[], int columns, int current) {
	item_buyingstore.insert({ atoi(fields[0]), true });
	return true;
}

static bool itemdb_read_flag(char* fields[], int columns, int current) {
	s_item_flag item = { 0 };
	uint16 flag = abs(atoi(fields[1]));

	if (flag & 1)
		item.dead_branch = true;
	if (flag & 2)
		item.group = true;
	if (flag & 4)
		item.guid = true;
	if (flag & 8)
		item.bindOnEquip = true;
	if (flag & 16)
		item.broadcast = true;
	if (flag & 32)
		item.delay_consume = true;
	if (flag & 64)
		item.dropEffect = DROPEFFECT_CLIENT;
	else if (flag & 128)
		item.dropEffect = DROPEFFECT_WHITE_PILLAR;
	else if (flag & 256)
		item.dropEffect = DROPEFFECT_BLUE_PILLAR;
	else if (flag & 512)
		item.dropEffect = DROPEFFECT_YELLOW_PILLAR;
	else if (flag & 1024)
		item.dropEffect = DROPEFFECT_PURPLE_PILLAR;
	else if (flag & 2048)
		item.dropEffect = DROPEFFECT_ORANGE_PILLAR;

	item_flag.insert({ atoi(fields[0]), item });
	return true;
}

static bool itemdb_read_itemdelay(char* str[], int columns, int current) {
	s_item_delay item = { 0 };

	item.delay = atoi(str[1]);

	if (columns == 3)
		item.sc = trim(str[2]);

	item_delay.insert({ atoi(str[0]), item });
	return true;
}

static bool itemdb_read_stack(char* fields[], int columns, int current) {
	s_item_stack item = { 0 };

	item.amount = atoi(fields[1]);

	int type = strtoul(fields[2], NULL, 10);

	if (type & 1)
		item.inventory = true;
	if (type & 2)
		item.cart = true;
	if (type & 4)
		item.storage = true;
	if (type & 8)
		item.guild_storage = true;

	item_stack.insert({ atoi(fields[0]), item });
	return true;
}

static bool itemdb_read_nouse(char* fields[], int columns, int current) {
	s_item_nouse item = { 0 };

	item.sitting = "true";
	item.override = atoi(fields[2]);

	item_nouse.insert({ atoi(fields[0]), item });
	return true;
}

static bool itemdb_read_itemtrade(char* str[], int columns, int current) {
	s_item_trade item = { 0 };
	int flag = atoi(str[1]);

	if (flag & 1)
		item.drop = true;
	if (flag & 2)
		item.trade = true;
	if (flag & 4)
		item.trade_partner = true;
	if (flag & 8)
		item.sell = true;
	if (flag & 16)
		item.cart = true;
	if (flag & 32)
		item.storage = true;
	if (flag & 64)
		item.guild_storage = true;
	if (flag & 128)
		item.mail = true;
	if (flag & 256)
		item.auction = true;

	item.override = atoi(str[2]);

	item_trade.insert({ atoi(str[0]), item });
	return true;
}

static size_t itemdb_read_db(const char* file, std::ofstream &out) {
	YAML::Emitter emitter(out);
	FILE* fp = fopen(file, "r");

	if (fp == nullptr) {
		ShowError("can't read %s\n", file);
		return 0;
	}

	int lines = 0;
	size_t entries = 0;
	char line[1024];

	emitter << YAML::BeginSeq;

	while (fgets(line, sizeof(line), fp)) {
		char *str[32], *p;
		int i;

		lines++;

		if (line[0] == '/' && line[1] == '/')
			continue;

		memset(str, 0, sizeof(str));

		p = strstr(line, "//");

		if (p != nullptr) {
			*p = '\0';
		}

		p = line;
		while (ISSPACE(*p))
			++p;
		if (*p == '\0')
			continue;// empty line
		for (i = 0; i < 19; ++i) {
			str[i] = p;
			p = strchr(p, ',');
			if (p == NULL)
				break;// comma not found
			*p = '\0';
			++p;
		}

		if (p == NULL) {
			ShowError("itemdb_read_db: Insufficient columns in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
			continue;
		}

		// Script
		if (*p != '{') {
			ShowError("itemdb_read_db: Invalid format (Script column) in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
			continue;
		}
		str[19] = p + 1;
		p = strstr(p + 1, "},");
		if (p == NULL) {
			ShowError("itemdb_read_db: Invalid format (Script column) in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
			continue;
		}
		*p = '\0';
		p += 2;

		// OnEquip_Script
		if (*p != '{') {
			ShowError("itemdb_read_db: Invalid format (OnEquip_Script column) in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
			continue;
		}
		str[20] = p + 1;
		p = strstr(p + 1, "},");
		if (p == NULL) {
			ShowError("itemdb_read_db: Invalid format (OnEquip_Script column) in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
			continue;
		}
		*p = '\0';
		p += 2;

		// OnUnequip_Script (last column)
		if (*p != '{') {
			ShowError("itemdb_read_db: Invalid format (OnUnequip_Script column) in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
			continue;
		}
		str[21] = p;
		p = &str[21][strlen(str[21]) - 2];

		if (*p != '}') {
			/* lets count to ensure it's not something silly e.g. a extra space at line ending */
			int lcurly = 0, rcurly = 0;

			for (size_t v = 0; v < strlen(str[21]); v++) {
				if (str[21][v] == '{')
					lcurly++;
				else if (str[21][v] == '}') {
					rcurly++;
					p = &str[21][v];
				}
			}

			if (lcurly != rcurly) {
				ShowError("itemdb_read_db: Mismatching curly braces in line %d (item with id %d), skipping.\n", lines, atoi(str[0]));
				continue;
			}
		}
		str[21] = str[21] + 1;  //skip the first left curly
		*p = '\0';              //null the last right curly

		int nameid = atoi(str[0]);

		emitter << YAML::BeginMap;
		emitter << YAML::Key << "Id" << YAML::Value << nameid;
		emitter << YAML::Key << "AegisName" << YAML::Value << str[1];
		emitter << YAML::Key << "Name" << YAML::Value << str[2];

		int type = atoi(str[3]), subtype = atoi(str[18]);

		emitter << YAML::Key << "Type" << YAML::Value << um_itemtypenames.find(static_cast<item_types>(type))->second;
		if (type == IT_WEAPON && subtype)
			emitter << YAML::Key << "SubType" << YAML::Value << um_weapontypenames.find(static_cast<weapon_type>(subtype))->second;
		else if (type == IT_AMMO && subtype)
			emitter << YAML::Key << "SubType" << YAML::Value << um_ammotypenames.find(static_cast<ammo_type>(subtype))->second;

		if (atoi(str[4]) > 0)
			emitter << YAML::Key << "Buy" << YAML::Value << atoi(str[4]);
		if (atoi(str[5]) > 0) {
			if (atoi(str[4]) / 2 != atoi(str[5]))
				emitter << YAML::Key << "Sell" << YAML::Value << atoi(str[5]);
		}
		if (atoi(str[6]) > 0 )
			emitter << YAML::Key << "Weight" << YAML::Value << atoi(str[6]);

#ifdef RENEWAL
		int atk = 0, matk = 0;

		itemdb_re_split_atoi(str[7], &atk, &matk);
		if (atk > 0)
			emitter << YAML::Key << "Attack" << YAML::Value << atk;
		if (matk > 0)
			emitter << YAML::Key << "MagicAttack" << YAML::Value << matk;
#else
		if (atoi(str[7]) > 0)
			emitter << YAML::Key << "Attack" << YAML::Value << atoi(str[7]);
#endif
		if (atoi(str[8]) > 0)
			emitter << YAML::Key << "Defense" << YAML::Value << atoi(str[8]);
		if (atoi(str[9]) > 0)
			emitter << YAML::Key << "Range" << YAML::Value << atoi(str[9]);
		if (atoi(str[10]) > 0)
			emitter << YAML::Key << "Slots" << YAML::Value << atoi(str[10]);

		uint64 temp_mask = (uint64)strtoull(str[11], NULL, 0);

		if (temp_mask == 0) {
			emitter << YAML::Key << "Job";
			emitter << YAML::Value << YAML::BeginMap << YAML::Key << "All" << YAML::Value << "false" << YAML::EndMap;
		} else if (temp_mask == 0xFFFFFFFF) { // Commented out because it's the default value
			//emitter << YAML::Key << "Job";
			//emitter << YAML::Value << YAML::BeginMap << YAML::Key << "All" << YAML::Value << "true" << YAML::EndMap;
		} else if (temp_mask == 0xFFFFFFFE) {
			emitter << YAML::Key << "Job";
			emitter << YAML::Value << YAML::BeginMap;
			emitter << YAML::Key << "All" << YAML::Value << "true";
			emitter << YAML::Key << "Novice" << YAML::Value << "false";
			emitter << YAML::EndMap;
		} else {
			emitter << YAML::Key << "Job";
			emitter << YAML::Value << YAML::BeginMap;
			for (const auto &it : um_jobnames) {
				uint64 job_mask = 1ULL << it.second;

				if ((temp_mask & job_mask) == job_mask)
					emitter << YAML::Key << it.first << YAML::Value << "true";
			}
			emitter << YAML::EndMap;
		}
		if (atoi(str[12]) > 0) {
			int temp_class = atoi(str[12]);

			if (temp_class == ITEMJ_NONE) {
				emitter << YAML::Key << "Class";
				emitter << YAML::Value << YAML::BeginMap << YAML::Key << "All" << YAML::Value << "false" << YAML::EndMap;
			} else if (temp_class == ITEMJ_ALL) { // Commented out because it's the default value
				//emitter << YAML::Key << "Class";
				//emitter << YAML::Value << YAML::BeginMap << YAML::Key << "All" << YAML::Value << "true" << YAML::EndMap;
			} else {
				emitter << YAML::Key << "Class";
				emitter << YAML::Value << YAML::BeginMap;
				for (const auto & it : um_itemjobnames) {
					if (it.second & temp_class)
						emitter << YAML::Key << it.first << YAML::Value << "true";
				}
				emitter << YAML::EndMap;
			}
		}
		if (atoi(str[13]) > 0) {
			switch (atoi(str[13])) {
				case SEX_FEMALE:
					emitter << YAML::Key << "Gender" << YAML::Value << "SEX_FEMALE";
					break;
				case SEX_MALE:
					emitter << YAML::Key << "Gender" << YAML::Value << "SEX_MALE";
					break;
				//case SEX_BOTH: // Commented out because it's the default value
				//	emitter << YAML::Key << "Gender" << YAML::Value << "SEX_BOTH";
				//	break;
			}
		}
		if (atoi(str[14]) > 0) {
			int temp_loc = atoi(str[14]);

			emitter << YAML::Key << "Location";
			emitter << YAML::Value << YAML::BeginMap;
			for (const auto &it : um_equipnames) {
				if (it.second & temp_loc)
					emitter << YAML::Key << it.first << YAML::Value << "true";
			}
			emitter << YAML::EndMap;
		}
		if (atoi(str[15]) > 0)
			emitter << YAML::Key << "WeaponLevel" << YAML::Value << atoi(str[15]);

		int elv = 0, elvmax = 0;

		itemdb_re_split_atoi(str[16], &elv, &elvmax);
		if (elv > 0)
			emitter << YAML::Key << "EquipLevelMin" << YAML::Value << elv;
		if (elvmax > 0)
			emitter << YAML::Key << "EquipLevelMax" << YAML::Value << elvmax;
		if (atoi(str[17]) > 0)
			emitter << YAML::Key << "Refineable" << YAML::Value << "true";
		if (atoi(str[18]) > 0 && type != IT_WEAPON && type != IT_AMMO)
			emitter << YAML::Key << "View" << YAML::Value << atoi(str[18]);
		if (item_buyingstore.find(nameid) != item_buyingstore.end())
			emitter << YAML::Key << "BuyingStore" << YAML::Value << "true";

		auto it_flag = item_flag.find(nameid);

		if (it_flag != item_flag.end()) {
			if (it_flag->second.dead_branch)
				emitter << YAML::Key << "DeadBranch" << YAML::Value << it_flag->second.dead_branch;
			if (it_flag->second.group)
				emitter << YAML::Key << "Container" << YAML::Value << it_flag->second.group;
			if (it_flag->second.guid)
				emitter << YAML::Key << "GUID" << YAML::Value << it_flag->second.guid;
			if (it_flag->second.bindOnEquip)
				emitter << YAML::Key << "BindOnEquip" << YAML::Value << it_flag->second.bindOnEquip;
			if (it_flag->second.broadcast)
				emitter << YAML::Key << "DropAnnounce" << YAML::Value << it_flag->second.broadcast;
			if (it_flag->second.delay_consume)
				emitter << YAML::Key << "NoConsume" << YAML::Value << it_flag->second.delay_consume;
			if (it_flag->second.dropEffect)
				emitter << YAML::Key << "DropEffect" << YAML::Value << it_flag->second.dropEffect;
		}

		auto it_delay = item_delay.find(nameid);

		if (it_delay != item_delay.end()) {
			emitter << YAML::Key << "Delay";
			emitter << YAML::Value << YAML::BeginMap;
			emitter << YAML::Key << "Duration" << YAML::Value << it_delay->second.delay;
			if (it_delay->second.sc.size() > 0)
				emitter << YAML::Key << "Status" << YAML::Value << it_delay->second.sc;
			emitter << YAML::EndMap;
		}

		auto it_stack = item_stack.find(nameid);

		if (it_stack != item_stack.end()) {
			emitter << YAML::Key << "Stack";
			emitter << YAML::Value << YAML::BeginMap;
			emitter << YAML::Key << "Amount" << YAML::Value << it_stack->second.amount;
			if (it_stack->second.inventory)
				emitter << YAML::Key << "Inventory" << YAML::Value << it_stack->second.inventory;
			if (it_stack->second.cart)
				emitter << YAML::Key << "Cart" << YAML::Value << it_stack->second.cart;
			if (it_stack->second.storage)
				emitter << YAML::Key << "Storage" << YAML::Value << it_stack->second.storage;
			if (it_stack->second.guild_storage)
				emitter << YAML::Key << "GuildStorage" << YAML::Value << it_stack->second.guild_storage;
			emitter << YAML::EndMap;
		}

		auto it_nouse = item_nouse.find(nameid);

		if (it_nouse != item_nouse.end()) {
			emitter << YAML::Key << "NoUse";
			emitter << YAML::Value << YAML::BeginMap;
			emitter << YAML::Key << "Override" << YAML::Value << it_nouse->second.override;
			emitter << YAML::Key << "Sitting" << YAML::Value << "true";
			emitter << YAML::EndMap;
		}

		auto it_trade = item_trade.find(nameid);

		if (it_trade != item_trade.end()) {
			emitter << YAML::Key << "Trade";
			emitter << YAML::Value << YAML::BeginMap;
			emitter << YAML::Key << "Override" << YAML::Value << it_trade->second.override;
			if (it_trade->second.drop)
				emitter << YAML::Key << "NoDrop" << YAML::Value << it_trade->second.drop;
			if (it_trade->second.trade)
				emitter << YAML::Key << "NoTrade" << YAML::Value << it_trade->second.trade;
			if (it_trade->second.trade_partner)
				emitter << YAML::Key << "TradePartner" << YAML::Value << it_trade->second.trade_partner;
			if (it_trade->second.sell)
				emitter << YAML::Key << "NoSell" << YAML::Value << it_trade->second.sell;
			if (it_trade->second.cart)
				emitter << YAML::Key << "NoCart" << YAML::Value << it_trade->second.cart;
			if (it_trade->second.storage)
				emitter << YAML::Key << "NoStorage" << YAML::Value << it_trade->second.storage;
			if (it_trade->second.guild_storage)
				emitter << YAML::Key << "NoGuildStorage" << YAML::Value << it_trade->second.guild_storage;
			if (it_trade->second.mail)
				emitter << YAML::Key << "NoMail" << YAML::Value << it_trade->second.mail;
			if (it_trade->second.auction)
				emitter << YAML::Key << "NoAuction" << YAML::Value << it_trade->second.auction;
			emitter << YAML::EndMap;
		}

		if (*str[19])
			emitter << YAML::Key << "Script" << YAML::Value << YAML::Literal << str[19];
		if (*str[20])
			emitter << YAML::Key << "EquipScript" << YAML::Value << YAML::Literal << str[20];
		if (*str[21])
			emitter << YAML::Key << "UnEquipScript" << YAML::Value << YAML::Literal << str[21];

		//body[counter++] = node;
		emitter << YAML::EndMap;
		counter++;
		//out.flush();
		entries++;
	}

	emitter << YAML::EndSeq;

	fclose(fp);
	ShowStatus("Done reading '" CL_WHITE "%d" CL_RESET "' items in '" CL_WHITE "%s" CL_RESET "'.\n", entries, file);

	return entries;
}
