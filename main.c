//#define CHEAT

// Copyright 2014 Sean Barrett

#define RELEASE
#define TIME_SCALE 1
#define MAX_GEMS 30

#define APPNAME "PROMESST2"

// default screen size, and required aspect ratio
#define SCREEN_X  1024
#define SCREEN_Y  768

#pragma warning(disable:4244; disable:4305; disable:4018)

#include <assert.h>
#include <ctype.h>
#define STB_DEFINE
#define STB_WINMAIN

#ifdef _WIN32
#include "stb_wingraph.h"
#else
#include "stb_sdl2graph.h"
#include <sys/stat.h>

#define VK_DOWN  SDLK_DOWN
#define VK_LEFT  SDLK_LEFT
#define VK_RIGHT SDLK_RIGHT
#define VK_UP    SDLK_UP
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "stb_image.c"  // before stb_gl so we get stbgl_LoadImage support
#include "stb_gl.h"


#define stb_clamp(x,xmin,xmax)  ((x) < (xmin) ? (xmin) : (x) > (xmax) ? (xmax) : (x))
#define stb_lerp(t,a,b)               ( (a) + (t) * (float) ((b)-(a)) )

static int screen_x, screen_y;
static int tex;

// red green blue orange yellow violet cyan pink

// r g b o y v c p


typedef unsigned char uint8;
typedef   signed char  int8;
typedef unsigned short uint16;

int game_started=0;

enum
{
   M_logo,
   M_menu,
   M_credits,
   M_game,
} main_mode = M_logo;

int menu_selection = 2;


typedef struct
{
   uint8 r,g,b,a;
} color;

color powers[8];
uint8 *colordata;

static int sss_tex, title_tex;

void init_graphics(void)
{
   int w,h,i;
   colordata = stbi_load("data2/sprites.png", &w,&h,0,0);
   tex = stbgl_TexImage2D(0, w,h, colordata, "n");
   for (i=0; i < 8; ++i)
      memcpy(&powers[i].r, colordata+96*128*4 + (112+i)*4, 4);
   title_tex = stbgl_LoadTexture("data2/title.png", "n");
   sss_tex = stbgl_LoadTexture("data2/sss_logo.png", "n");
}

void get_map_color(color *c, int x, int y, int z)
{
   int s = 7*16+8 + (x&7);
   int t = 6*16 + (y&7) + (z ? 8 : 0);
   c->a = 255;
   c->r = colordata[t*128*4 + s*4 + 0];
   c->g = colordata[t*128*4 + s*4 + 1];
   c->b = colordata[t*128*4 + s*4 + 2];
}

enum
{
   DIR_e,
   DIR_n,
   DIR_w,
   DIR_s
};

int xdir[4] = { 1,0,-1,0 };
int ydir[4] = { 0,-1,0,1 };

enum
{
   TILE_wall,
   TILE_door,
   TILE_open_door,
   TILE_floor,
   TILE_receptacle,
   TILE_stone_fragments,
   TILE_arrow_e,
   TILE_arrow_n,
   TILE_arrow_w,
   TILE_arrow_s,
   TILE_stairs,

   TILE_egg,
   TILE_egg_2,
   TILE_egg_3,
   TILE_egg_4,
   TILE_egg_5,
   TILE_egg_6,

   TILE_egg_7,
   TILE_egg_8,
};

typedef struct
{
   int s,t;
} sprite_id;

sprite_id tile_sprite[] =
{
   { 0,0 }, // wall
   { 6,0 }, // door
   { 5,0 }, // open door
   { 0,3 }, // floor
   { 1,4 }, // receptacle
   { 6,2 }, // stone fragments
   { 0,3 }, // arrow
   { 0,3 },
   { 0,3 },
   { 0,3 },
   { 1,3 }, // stairs
   { 5,3 }, { 6,3 }, { 7,3 },
   { 3,1 }, { 4,1 }, { 5,1 },
   { 6,5 }, { 6,6 },
};

enum
{
   T_empty,
   T_stone,
   T_gem,

   T_projector,
   T_wand,
   T_reflector,

   T_rover
};

sprite_id obj_sprite[] =
{
   { 6,1 },
   { 2,1 },
   { 0,4 },

   { 0,2 },
   { 2,4 },
   { 5,6 },

   { 6,2 },
   { 3,1 },
};

int obj_sprite_hasdir[] =
{
   0,0,0,1,
   0,1,2,0,
};

typedef struct
{
   uint8 type:3;
   uint8 dir:2;
   uint8 color:3;
} obj;

#define NUM_X   4
#define NUM_Y   4
#define SIZE_X  6
#define SIZE_Y  6

uint8 tilemap[2][NUM_Y*SIZE_Y][NUM_Y*SIZE_Y];
obj objmap[2][NUM_Y*SIZE_Y][NUM_X*SIZE_X];
uint8 edge[2][NUM_Y*SIZE_Y][NUM_X*SIZE_X];

typedef struct
{
   uint8 player_x, player_y, player_z;
   uint8 pdir;

   uint8 num_treasures;
   uint8 has_wand;
   uint8 ability_flag;
   uint8 num_reflectors;

   uint16 num_zaps;
   uint16 egg_timer;
   uint16 seen[2];
   int16  gems_stored;
   uint8 num_gems;
} player_state;

// E N W S                                           # down staircase
// r R w P  red: unlock doors                        | wall
// g   H G  green: walk through walls                . floor
// Y   L y  yellow: destroy blocks                   * boulder
// j V t T  violet: far travel                       = gem slot
// o A O a  orange: step 2x        0,1,2,3,4,5,6,7,8,9 gem in slot
//                                                 @,% bare gem
char *rawmap[NUM_Y*SIZE_Y][2] =
{
   "|||||| |...|| |||.G| |y....    ",      "|||||| .|%.|| |||||| |||||| ", 
   "||6D.. |...=| |@|... |||||*    ",      "|.j.|| .|.|.* .|.H.| ||O||. ", 
   "D|.||| |***.L |||||| |...|%    ",      "*...|| .T.|.D .y...| |.*.|| ", 
   "||*||| |***|| |..=.| |..O|T    ",      "|.|||| .||||| .||||| |||..% ", 
   "|...|| ||*||9 |V...| |*..|.    ",      "|T.|.. ...... ...... ..D.w| ", 
   "|...|| ||.||. ||.... ...=|.    ",      "||||=| |.=||| .=|||| =||||| ", 
           
   "|||||| |..%|. |||||D ||||||    ",      "|||||% |.|..| .||||| ||||||", 
   "|*|D|. H...|. ||P||. |..%||    ",      "|||||. |.|t.| .....| ||%..|", 
   "|...|. |||||| |..2|. |.D|||    ",      "||*a|. |.||%| .||||| ||||.|", 
   "|2..|= |||||| |..=|. ||.|||    ",      "|||..| |.|||| .A%||| *o.O.*", 
   "|o..|. ..===| .D..|. |*....    ",      "g||||| ...... .=|V|| |=.|||", 
   "|||||| ..|||| .||||. ||||||    ",      "%||.=. .=|||| |||||| ||.|.|", 
           
   "y|||R. ....|| .||||. |T.|%.    ",      "*|||.| |||..| |*|||| ||.|.|", 
   "|9||=. |||... ...... ..0||*    ",      ".|...y ||...| |..|t| ||.|.|", 
   "|.||H. |.|#.. |||*.. ..0|||    ",      ".|*|=. D..#.| |o...% *G.|.*", 
   "*D|||. |.|=.. |3.D.| ||.|||    ",      ".|..|| ||...| |=|||| ||%|.|", 
   ".%|j|. |.|... |..*.| |.||%|    ",      ".||... ||.... |.|||| ||||=|", 
   "|R|||. |A|||| |||||| =..|*|    ",      ".||||* |||.|. ...... ...D.|", 
           
   "||..|. |||||| >>.v<| |9y|.|    ",      "...... |...|| |||||| |*|D*| ", 
   "||.... ..||G| >^>=v| |*.|.|    ",      "|.|||= |r.... *g.||* |....| ", 
   "|D.=|| |....| pvz<.| |P|.|H    ",      "*.||.| |===.D ..=|.. ...P.| ", 
   "*9||*| |.|||| ^.&.<* ||*|||    ",      "|.|%.| |===.| ||D||| ||..|| ", 
   "||...D ..|9.| |>^.<. ..D.|O    ",      "|||||| |===.| |..... .|-..| ", 
   "||V2|| |.||*| ||#||| |*||||    ",      "||||A| |||||| |.#.|| |||||9 ", 
};  // 19                                  // 11

// see revision history for puzzle notes

enum
{
   COLOR_red,
   COLOR_green,
   COLOR_blue,
   COLOR_orange,
   COLOR_yellow,
   COLOR_violet,
   COLOR_teal,
   COLOR_pink,
};

char *lights = "rRPwgGHyYLtTVjoOAa";

// this actually encodes color + direction, not sprite_id
sprite_id lightdata[] =
{
   { COLOR_red   , DIR_e }, { COLOR_red   , DIR_n }, { COLOR_red, DIR_s }, { COLOR_red, DIR_w },
   { COLOR_green , DIR_e }, { COLOR_green , DIR_s }, { COLOR_green, DIR_w },
   { COLOR_yellow, DIR_s }, { COLOR_yellow, DIR_e }, { COLOR_yellow, DIR_w },
   { COLOR_violet, DIR_w }, { COLOR_violet, DIR_s }, { COLOR_violet, DIR_n }, { COLOR_violet, DIR_e },
   { COLOR_orange, DIR_e }, { COLOR_orange, DIR_w }, { COLOR_orange, DIR_n }, { COLOR_orange, DIR_s },
};




player_state state;
int px,py,pz,pdir;
int egg_x, egg_y;

#define IS_ARROW(x)  ((x) >= TILE_arrow_e && (x) < TILE_arrow_e+4)

void init_game(void)
{
   int x,y,z;
   int foo=0;

   for (z=0; z < 2; ++z) {
      for (y=0; y < NUM_Y*SIZE_Y; ++y) {
         char *data = rawmap[y][z];
         for (x=0; x < NUM_X*SIZE_X; ++x) {
            int d = data[x + x/SIZE_X];
            tilemap[z][y][x] = TILE_floor;
            objmap[z][y][x].type = T_empty;
            objmap[z][y][x].dir = 0;
            switch (d) {
               case 'p':
                  px = x; py = y; pz = z;
                  pdir = DIR_s;
                  break;
               case '>': tilemap[z][y][x] = TILE_arrow_e; break;
               case '^': tilemap[z][y][x] = TILE_arrow_n; break;
               case '<': tilemap[z][y][x] = TILE_arrow_w; break;
               case 'v': tilemap[z][y][x] = TILE_arrow_s; break;
               default:  tilemap[z][y][x] = TILE_wall;    break;
               case '=': tilemap[z][y][x] = TILE_receptacle; break;
               case 'D': tilemap[z][y][x] = TILE_door;    break;
               case '#': tilemap[z][y][x] = TILE_stairs; break;
               case '/': objmap[z][y][x].type = T_reflector;
                         objmap[z][y][x].dir = 1;
                         break;

               case 'z': egg_x = x;
                         egg_y = y;
                         break;

               case '*': objmap[z][y][x].type = T_stone; break;
               case '-': objmap[z][y][x].type = T_wand;  break;
//             case '$': objmap[z][y][x].type = T_treasure; break;

               case '1': case '2': case '3': case '4': case '0':
               case '5': case '6': case '7': case '8': case '9':
               case '@': case '%':
                         objmap [z][y][x].type = T_gem;
                         objmap [z][y][x].dir = 0;
                         if (d != '@' && d != '%')
                            tilemap[z][y][x] = TILE_receptacle;
                         break;

               case '&': objmap[z][y][x].type = T_rover;
                         objmap[z][y][x].dir = DIR_e;
                         break;

               case 'r': case 'g': case 'y': case 't': case 'o':
               case 'R': case 'G': case 'Y': case 'T': case 'O': 
               case 'P': case 'H': case 'w': case 'V': case 'j': case 'L':
               case 'a': case 'A':
               {
                  char *s = strchr(lights, d);
                  assert(s != NULL);
                  d = s - lights;
                  objmap[z][y][x].type  = T_projector;
                  objmap[z][y][x].color = lightdata[d].s;
                  objmap[z][y][x].dir   = lightdata[d].t;
                  break;
               }

               case '.':
               case ' ':
                  break;

            }
         }
      }
   }

   for (x=0; x < 3; ++x)
      for (y=1; y < 2; ++y)
         tilemap[0][egg_y+y][egg_x-1+x] = TILE_egg + y*3+x;

   // initialize rover directions by finding adjacent arrow and setting initial direction in the arrow dir
   for (z=0; z < 2; ++z) {
      for (y=0; y < NUM_Y*SIZE_Y; ++y) {
         for (x=0; x < NUM_X*SIZE_X; ++x) {
            if (objmap[z][y][x].type == T_rover) {
               int d;
               // find an adjacent arrow
               for (d=0; d < 4; ++d)
                  if (tilemap[z][y-ydir[d]][x-xdir[d]] == TILE_arrow_e + d)
                     objmap[z][y][x].dir = d;
            }
         }
      }
   }
}

typedef struct st_roomsave_big
{
   struct st_roomsave_big *next;
//   uint8 big_flag;
   player_state state;
   uint8 room_x, room_y, room_z;
   uint8 doors[SIZE_Y];
   obj h_objmap[SIZE_Y][NUM_X*SIZE_X];
   obj v_objmap[NUM_Y*SIZE_Y][SIZE_X];
} roomsave_big; // ~316 bytes incl malloc overhead -- full world would be about 2x

// how undo works:
//
//   world state N
//      state of world in N-1 that might be changed by N
//   world state N-1
//      state of world in N-2 that might be changed by N-1
//   world state N-2
//      state of world in N-3 that might be changed by N-2
//   world state N-3
//
// One way to store the above would be to store the *entire*
// state of world N-1, N-2, etc.
//
//  save:
//         1. create a roomsave_big for the room
//            being entered (and save previous player state)
//         2. push on undo stack
//
//  undo:
//         1. pop undo stack


#if 0
// we could try to use this if we know that the change doesn't
// include any out-of-room effects, but the restore logic was
// a lot more complicated so I punted it
typedef struct st_roomsave
{
   struct st_roomsave *next;
   uint8 big_flag;
   player_state state;
   uint8 room_x, room_y, room_z;
   obj objmap[SIZE_Y][SIZE_X];   
} roomsave; // 52 bytes incl malloc overhead
//   when you enter a room:
//      turn previous save into an undo entry:
//         1. if "out-of-room" effects flag set,
//            add roomsave_big to undo chain
//         2. otherwise, add roomsave to undo chain
//      save current state:
//         3. create a roomsave_big for the room
//            being entered (and save previous player state)
//         4. clear the "out-of-room" effects flag.
//         5. during play, set the "out-of-room"
//            effects flag if the player pushes
//            anything out of room
//
//  on undo, say we're in world state N:
//
//     1. restore stored roomsave_big
//
//  this returns us to state N-1. but we can now make further
//  changes, invalidating the N-2 delta IF that delta isn't big.
//  
//     2. pop undo stack
//        2a. if big on stack, copy into roomsave_big
//        2b. if small on stack, combined current world with undo'd room to create roomsave_big
#endif


int room_x, room_y, room_z;

void compute_room(void)
{
   room_x = px / SIZE_X;
   room_y = py / SIZE_Y;
   room_z = pz;
}

int has_power[NUM_Y][NUM_X];

void compute_powered(void)
{
   int x,y;
   memset(has_power, 0, sizeof(has_power));

   // there's power if (a) there's at least one
   // receptacle, and (b) there's no unpowered receptacle
   for (y=0; y < NUM_Y*SIZE_Y; ++y)
      for (x=0; x < NUM_X*SIZE_X; ++x)
         if (tilemap[room_z][y][x] == TILE_receptacle)
            has_power[y/SIZE_Y][x/SIZE_X] = 1;

   for (y=0; y < NUM_Y*SIZE_Y; ++y)
      for (x=0; x < NUM_X*SIZE_X; ++x)
         if (tilemap[room_z][y][x] == TILE_receptacle && objmap[room_z][y][x].type != T_gem)
            has_power[y/SIZE_Y][x/SIZE_X] = 0;
}

int8 light_for_dir[NUM_Y*SIZE_Y][NUM_X*SIZE_X][4];
uint8 any_light_for_dir[NUM_Y*SIZE_Y][NUM_X*SIZE_X];

void propagate_light(void)
{
   int x,y;
   memset(light_for_dir, -1, sizeof(light_for_dir));
   memset(any_light_for_dir, 0, sizeof(any_light_for_dir));
   for (y=0; y < NUM_Y*SIZE_Y; ++y)
      for (x=0; x < NUM_X*SIZE_X; ++x)
         if (objmap[room_z][y][x].type == T_projector)
            if (has_power[y/SIZE_Y][x/SIZE_X]) {
               obj o = objmap[room_z][y][x];
               int dir = o.dir;
               int ax = x, dx = xdir[o.dir];
               int ay = y, dy = ydir[o.dir];
               uint8 c = o.color;
               for(;;) {
                  ax += dx; if (ax < 0) ax += NUM_X*SIZE_X; else if (ax >= NUM_X*SIZE_X) ax=0;
                  ay += dy; if (ay < 0) ay += NUM_Y*SIZE_Y; else if (ay >= NUM_Y*SIZE_Y) ay=0;
                  if (tilemap[room_z][ay][ax] == TILE_door)
                     break;
                  if (objmap[room_z][ay][ax].type == T_reflector) {
                     if (objmap[room_z][ay][ax].dir == 0)
                        dir ^= 1;
                     else
                        dir ^= 3;
                     dx = xdir[dir];
                     dy = ydir[dir];
                  } else {
                     if (objmap[room_z][ay][ax].type != T_empty)
                        break;
                  }
                  light_for_dir[ay][ax][dir] = o.color;
                  any_light_for_dir[ay][ax] = 1;
               }
            }

}

typedef struct
{
   int x,y;
} point;

int find_lights_on_player(point projectors[4])
{
   int x,y, n=0;
   for (y=0; y < NUM_Y*SIZE_Y; ++y)
      for (x=0; x < NUM_X*SIZE_X; ++x)
         if (objmap[room_z][y][x].type == T_projector)
            if (has_power[y/SIZE_Y][x/SIZE_X]) {
               obj o = objmap[room_z][y][x];
               int dir = o.dir;
               int ax = x, dx = xdir[o.dir];
               int ay = y, dy = ydir[o.dir];
               uint8 c = o.color;
               for(;;) {
                  ax += dx; if (ax < 0) ax += NUM_X*SIZE_X; else if (ax >= NUM_X*SIZE_X) ax=0;
                  ay += dy; if (ay < 0) ay += NUM_Y*SIZE_Y; else if (ay >= NUM_Y*SIZE_Y) ay=0;
                  if (ax == px && ay == py) {
                     projectors[n].x = x;
                     projectors[n].y = y;
                     ++n;
                  }
                  if (tilemap[room_z][ay][ax] == TILE_door)
                     break;
                  if (objmap[room_z][ay][ax].type == T_reflector) {
                     if (objmap[room_z][ay][ax].dir == 0)
                        dir ^= 1;
                     else
                        dir ^= 3;
                     dx = xdir[dir];
                     dy = ydir[dir];
                  } else {
                     if (objmap[room_z][ay][ax].type != T_empty)
                        break;
                  }
                  light_for_dir[ay][ax][dir] = o.color;
                  any_light_for_dir[ay][ax] = 1;
               }
            }

   return n;
}

typedef struct
{
   uint8 tilemap[2][NUM_Y*SIZE_Y][NUM_Y*SIZE_Y];
   obj objmap[2][NUM_Y*SIZE_Y][NUM_X*SIZE_X];
   player_state state;   
} gamestate;


static void save_map(gamestate *g)
{
   memcpy(g->tilemap, tilemap, sizeof(tilemap));
   memcpy(g->objmap, objmap, sizeof(objmap));
   state.player_x = px;
   state.player_y = py;
   state.player_z = pz;
   g->state = state;
}

static void restore_map(gamestate *g)
{
   memcpy(tilemap, g->tilemap, sizeof(tilemap));
   memcpy(objmap, g->objmap, sizeof(objmap));
   state = g->state;
   px = state.player_x;
   py = state.player_y;
   pz = state.player_z;

   compute_room();
   compute_powered();
   propagate_light();
}

roomsave_big *undo_chain;

static void save_state(int rx, int ry, int rz)
{
   int x,y;
   roomsave_big *save_big;
   save_big = malloc(sizeof(*save_big));
   for (y=0; y < SIZE_Y; ++y)
      for (x=0; x < SIZE_X*NUM_X; ++x)
         save_big->h_objmap[y][x] = objmap[rz][ry * SIZE_Y + y][x];
   for (y=0; y < SIZE_Y*NUM_Y; ++y)
      for (x=0; x < SIZE_X; ++x)
         save_big->v_objmap[y][x] = objmap[rz][y][rx * SIZE_X + x];

   for (y=0; y < SIZE_Y; ++y) {
      uint8 doors=0;
      for (x=0; x < SIZE_X; ++x)
            if (tilemap[rz][ry*SIZE_Y+y][rx*SIZE_X+x] == TILE_door)
               doors |= 1 << x;
      save_big->doors[y] = doors;
   }
            
   save_big->next = undo_chain;
   undo_chain = save_big;
   save_big->room_x = rx;
   save_big->room_y = ry;
   save_big->room_z = rz;
   save_big->state = state;
}

static void restore_undo(void)
{
   int x,y,rx,ry,rz;
   roomsave_big *save = undo_chain;
   if (save == NULL) return;
   undo_chain = save->next;

   rx = save->room_x;
   ry = save->room_y;
   rz = save->room_z;
   for (y=0; y < SIZE_Y; ++y)
      for (x=0; x < SIZE_X*NUM_X; ++x)
         objmap[rz][ry * SIZE_Y + y][x] = save->h_objmap[y][x];
   for (y=0; y < SIZE_Y*NUM_Y; ++y)
      for (x=0; x < SIZE_X; ++x)
         objmap[rz][y][rx * SIZE_X + x] = save->v_objmap[y][x];
   // restore closed doors in case any got opened
   for (y=0; y < SIZE_Y; ++y)
      for (x=0; x < SIZE_X*NUM_X; ++x)
         if (save->doors[y] & (1 << x))
            tilemap[rz][ry*SIZE_Y+y][rx*SIZE_X+x] = TILE_door;
   state = save->state;
   free(save);

   px = state.player_x;
   py = state.player_y;
   pz = state.player_z;
   pdir = DIR_s;

   compute_room();
   compute_powered();
   propagate_light();
}

static void flush_undo(void)
{
   while (undo_chain) {
      roomsave_big *save = undo_chain;
      undo_chain = save->next;
      free(save);
   }
}

static void set_player_pos(int x, int y, int z)
{
   int new_room_x = x / SIZE_X;
   int new_room_y = y / SIZE_Y;

   if (new_room_x != room_x || new_room_y != room_y || z != pz) {
      // entering a new room!
      state.player_x = px;
      state.player_y = py;
      state.player_z = pz;
      save_state(new_room_x, new_room_y, pz);
   }
   px = x;
   py = y;
   pz = z;

   compute_room();
   compute_powered();
   propagate_light();

   state.seen[pz] |= 1 << (room_y * NUM_X + room_x);
}

#define POWER_doors    COLOR_red
#define POWER_walls    COLOR_green
#define POWER_destroy  COLOR_yellow
//#define POWER_immobile COLOR_teal
#define POWER_double   COLOR_orange
#define POWER_travel   COLOR_violet

static void get_abilities(uint8 enabled[8])
{
   int d;
   memset(enabled, 0, 8);
   for (d=0; d < 4; ++d)
      if (light_for_dir[py][px][d] >= 0)
         enabled[light_for_dir[py][px][d]] += 1;
}

#define WRAP_X(x)  (((x) + SIZE_X*NUM_X) % (SIZE_X*NUM_X))
#define WRAP_Y(y)  (((y) + SIZE_Y*NUM_Y) % (SIZE_Y*NUM_Y))

static int push_rovers(int x, int y, int dir, int n)
{
   return 0;
}

#define PLAYER_MOVE_TIME    80
#define PLAYER_TRAVEL_TIME  80
int player_timer;

#define ROVER_MOVE_TIME   630

int noclip;

gamestate checkpoint;
int checkpoint_timer = 0;

static void move(int x, int y)
{
   uint8 used_flag = state.ability_flag;
   int got_wand=0;
   uint8 abilities[8];
   int gx,gy;
   int travel;
   int proposed_pdir;

   proposed_pdir = x ? x<0 ? DIR_w : DIR_e : y<0 ? DIR_n : DIR_s;

   travel = (light_for_dir[py][px][proposed_pdir] == POWER_travel || light_for_dir[py][px][proposed_pdir^2] == POWER_travel);

   get_abilities(abilities);

   if (travel) {
      gx = WRAP_X(px+x);
      gy = WRAP_Y(py+y);
      while(light_for_dir[gy][gx][proposed_pdir] == POWER_travel || light_for_dir[gy][gx][proposed_pdir^2] == POWER_travel) {
         gx = WRAP_X(gx+x);
         gy = WRAP_Y(gy+y);
      }
      gx = WRAP_X(gx-x);
      gy = WRAP_Y(gy-y);
      if (gx != px || gy != py)
         used_flag |= (1 << POWER_travel);
      else
         travel = 0;
   }

   if (!travel) {
      if (abilities[POWER_double]) {
         x *= (1 << abilities[POWER_double]);
         y *= (1 << abilities[POWER_double]);
      }
      gx = WRAP_X(px+x);
      gy = WRAP_Y(py+y);
   }

   if (noclip) {
      set_player_pos(gx,gy,pz);
      pdir = proposed_pdir;
      return;
   }

   // make them get stuck if they move into a wall and have no projector
   if (tilemap[pz][py][px] == TILE_wall) {
      if (!abilities[POWER_walls])
         return;
      used_flag |= (1 << POWER_walls);
   }
      
   if (objmap[pz][gy][gx].type == T_projector)
      return;

   if (tilemap[pz][gy][gx] == TILE_wall) {
      if (!abilities[POWER_walls])
         return;
      used_flag |= (1 << POWER_walls);
   }

   if (tilemap[pz][gy][gx] == TILE_door) {
      if (!abilities[POWER_doors])
         return;
      state.ability_flag |= (1 << POWER_doors);
      tilemap[pz][gy][gx] = TILE_open_door;
      pdir = proposed_pdir;
      return; // don't move, just open
   }

#if 0
   if (objmap[pz][gy][gx].type >= T_rover) {
      if (!push_rovers(gx, gy, proposed_pdir, 1))
         return;
   }
#endif

   if (objmap[pz][gy][gx].type == T_reflector)
      return;

   if (objmap[pz][gy][gx].type == T_stone) {
      if (!abilities[POWER_destroy])
         return;
      pdir = proposed_pdir;
      state.ability_flag |= (1 << POWER_destroy);
      objmap[pz][gy][gx].type = T_empty;
      return; // don't move, just destroy
   }

   if (objmap[pz][gy][gx].type == T_wand) {
      state.has_wand = 1;
      objmap[pz][gy][gx].type = 0;
      got_wand = 1;
   }

   if (abilities[POWER_double])
      used_flag |= (1 << POWER_double);

   state.ability_flag = used_flag;
   pdir = proposed_pdir;

   if (tilemap[pz][gy][gx] == TILE_stairs)
      set_player_pos(gx,gy,(pz+1)%2);
   else
      set_player_pos(gx,gy,pz);

   player_timer = PLAYER_MOVE_TIME;
   if (got_wand) {
      save_map(&checkpoint);
      checkpoint_timer = 1600;
   }
}

static void drop(void)
{
   if (objmap[pz][py][px].type == T_gem) {
      objmap[pz][py][px].type = T_empty;
      ++state.num_gems;
   } else
#if 0
   // drop anywhere: this lets you hack things by dropping beams
   if (state.num_gems && objmap[pz][py][px].type == T_empty && tilemap[pz][py][px] != TILE_wall)
#else
   if (state.num_gems && objmap[pz][py][px].type == T_empty && tilemap[pz][py][px] == TILE_receptacle)
#endif
   {
      --state.num_gems;
      objmap[pz][py][px].type = T_gem;
      objmap[pz][py][px].dir = 1;
   }
   compute_powered();
   propagate_light();
}

static void reflector(void)
{
   if (objmap[pz][py][px].type == T_reflector) {
      if (objmap[pz][py][px].dir == 0)
         objmap[pz][py][px].dir = 1;
      else {
         objmap[pz][py][px].type = T_empty;
         ++state.num_reflectors;
      }
   } else if (state.num_reflectors && objmap[pz][py][px].type == T_empty) {
      --state.num_reflectors;
      objmap[pz][py][px].dir = 0;
      objmap[pz][py][px].type = T_reflector;
   }
}

#define MAGIC_STEP_TIME  60
int magic_timer;
int magic_phase;

int shot_x, shot_y, shot_timer;

#if 0
// boulder/recolor combo
static void shoot(void)
{
   int x = WRAP_X(px+xdir[pdir]);
   int y = WRAP_Y(py+ydir[pdir]);
   int ax,ay, dx,dy;

   if (!state.has_wand)
      return;
   if (objmap[pz][y][x].type != T_projector)
      return;

   dx = -xdir[objmap[pz][y][x].dir];
   dy = -ydir[objmap[pz][y][x].dir];

   ax = x+dx;
   ay = y+dy;

   if (ax == px && ay == py)
      return;

   shot_x = x;
   shot_y = y;
   shot_timer = 250;

   while (objmap[pz][ay][ax].type == T_stone) {
      ax = WRAP_X(ax+dx);
      ay = WRAP_Y(ay+dy);
   }

   {
      int recolor[6] =
      {
         COLOR_green, // COLOR_red,
         COLOR_yellow, // COLOR_green,
         COLOR_blue,   // COLOR_blue,
         COLOR_violet, // COLOR_orange,
         COLOR_orange, // COLOR_yellow,
         COLOR_red, // COLOR_violet,
      };

      objmap[pz][ay][ax].type = T_stone;
      objmap[pz][y][x].color = recolor[objmap[pz][y][x].color];
   }

   ++state.num_zaps;
}
#elif 0
// room rotate
static void shoot(void)
{
   if (!state.has_wand)
      return;

   magic_phase = 9;
   magic_timer = MAGIC_STEP_TIME;

   shot_x = px;
   shot_y = py;
   shot_timer = 100;

   ++state.num_zaps;
}
#elif 0
// place/rotate reflector
static void shoot(void)
{
   int x = WRAP_X(px+xdir[pdir]);
   int y = WRAP_Y(py+ydir[pdir]);

   if (!state.has_wand)
      return;

   shot_x = x;
   shot_y = y;
   shot_timer = 50;

   if (objmap[pz][y][x].type == T_reflector)
      objmap[pz][y][x].dir ^= 1;
   else if (objmap[pz][y][x].type == T_empty) {
      if (tilemap[pz][y][x] != TILE_wall && tilemap[pz][y][x] != TILE_door) {
         objmap[pz][y][x].type = T_reflector;
         objmap[pz][y][x].dir = 0;
         ++state.num_zaps;
      }
   }
}
#elif 0
static void shoot(void)
{
   int x,y;
   int i,n=0,j = -1, any=0;
   uint8 abilities[8];

   if (!state.has_wand)
      return;

   get_abilities(abilities);
   for (i=0; i < 8; ++i)
      if (abilities[i])
         ++n, j=i;
   if (n != 1)
      return;

   shot_x = px;
   shot_y = py;
   shot_timer = 50;

   for (x=0; x < SIZE_X; ++x) {
      for (y=0; y < SIZE_Y; ++y) {
         int gx = x + room_x*SIZE_X;
         int gy = y + room_y*SIZE_Y;

         if (objmap[pz][gy][gx].type == T_projector && objmap[pz][gy][gx].color != j) {
            objmap[pz][gy][gx].color = j;
            any = 1;
            shot_x = gx;
            shot_y = gy;
         }
      }
   }
   if (any) {
      ++state.num_zaps;
      shot_timer = 150;
   }
}
#else
static void shoot(void)
{
   int i,n, any=0;
   point projectors[4];

   if (!state.has_wand)
      return;

   shot_x = px;
   shot_y = py;
   shot_timer = 50;

   n = find_lights_on_player(projectors);

   for (i=0; i < n; ++i) {
      if (objmap[pz][projectors[i].y][projectors[i].x].dir != pdir) {
         objmap[pz][projectors[i].y][projectors[i].x].dir = pdir;
         any = 1;
      }
   }
   if (any) {
      ++state.num_zaps;
      shot_timer = 150;
   }
}
#endif


int queued_key;

static void do_key(int ch)
{
   switch (ch) {
      case 's' : move(0, 1); break;
      case 'a' : move(-1,0); break;
      case 'd' : move( 1,0); break;
      case 'w' : move(0,-1); break;
      case 'x' : drop(); break;
      case 'z' : shoot(); break;
      case 'c' : reflector(); break;
#if defined(_DEBUG) || defined(CHEAT)
      case 'g' : state.num_gems = 30; state.has_wand = 1; break;
      case 'n' : noclip = !noclip; break;
#endif
      case  8  : restore_undo(); break;
      case 27  : {
         main_mode = M_menu;
         menu_selection = 0; // continue
         break;
      }
   }
}

static int player_allowed(int x, int y)
{
   uint8 abilities[8];
   get_abilities(abilities);
   if (tilemap[pz][y][x] == TILE_wall) {
      if (abilities[POWER_walls])
         return 1;
      return 0;
   }
   if (objmap[pz][y][x].type != T_empty)
      return 0;
   return 1;
}

static int rove_allowed(int x, int y, int d, int rx, int ry)
{
   if (x < rx || x >= rx+SIZE_X)
      return 0;
   if (y < ry || y >= ry+SIZE_Y)
      return 0;
   if (tilemap[pz][y][x] == TILE_wall)
      return 0;
   if (tilemap[pz][y][x] == TILE_door)
      return 0;
#if 0
   if (x == px && y == py)
      return 0;
#endif
   if (objmap[pz][y][x].type == T_gem)
      return 1;
   // @TODO: push gems? then we need to do chained pushing
   // below includes a rover, even if it's GOING to move
   if (objmap[pz][y][x].type != T_empty)
      return 0;
   return 1;
}

int reverse_timer = -1;

static int preferred_rove_direction(uint8 occupied[SIZE_Y][SIZE_X], int x, int y, int rx, int ry)
{
   int cx = rx + x;
   int cy = ry + y;

   // compute the preferred direction
   int d = objmap[pz][cy][cx].dir;
   if (reverse_timer == 0)
      return d;
   if (IS_ARROW(tilemap[pz][cy][cx]))
      d = tilemap[pz][cy][cx] - TILE_arrow_e;
   if (!rove_allowed(cx+xdir[d],cy+ydir[d], d, rx,ry) || occupied[y][x] > 1)
      d = (d+2)&3;
   if (!rove_allowed(cx+xdir[d],cy+ydir[d], d, rx,ry) || occupied[y][x] > 1)
      return -1;

   return d;
}

int rover_timer = 0;
int feed_timer;

static void move_rovers(int rx, int ry)
{
   int x,y;
   sprite_id rovers[SIZE_X*SIZE_Y];
   int num_rovers=0, i;

   uint8 occupied[SIZE_Y][SIZE_X] = {0};
   // we want to move rovers simultaneously, so we need to get each one to tell us
   // where it WANTS to go

   ry = ry * SIZE_Y;
   rx = rx * SIZE_X;

   for (y=0; y < SIZE_Y; ++y) {
      for (x=0; x < SIZE_X; ++x) {
         if (objmap[pz][ry+y][rx+x].type >= T_rover) {
            int d = preferred_rove_direction(occupied, x,y, rx,ry);
            if (d < 0)
               ++occupied[y][x];
            else
               ++occupied[y+ydir[d]][x+xdir[d]];
            rovers[num_rovers].s = x;
            rovers[num_rovers].t = y;
            ++num_rovers;
         }
      }
   }

   if (reverse_timer == 0) {
      if (num_rovers)
         objmap[pz][ry+rovers[0].t][rx+rovers[0].s].dir = (objmap[pz][ry+rovers[0].t][rx+rovers[0].s].dir+2) & 3;
   }

   // then we decide where to go based on nobody else going there.
   // note that we really need to keep iterating this to a fixed point

   for(;;) {
      int any_change = 0;
      for (i=0; i < num_rovers; ++i) {
         if (rovers[i].s >= 0) {
            int d;
            x = rovers[i].s;
            y = rovers[i].t;
            d = preferred_rove_direction(occupied, x,y, rx,ry);
            if (d < 0) {
               objmap[pz][ry+y][rx+x].type = T_rover;
            } else {
               // it might currently be blocked by another rover that's moving away
               if (objmap[pz][ry+y+ydir[d]][rx+x+xdir[d]].type >= T_rover)
                  continue;
#if 0
               // @TODO: push player
               if (px == rx+x+xdir[d] && py == ry+y+ydir[d]) {
                  player_timer = rover_timer;
                  px = rx+x+xdir[d]*2;
                  py = ry+y+ydir[d]*2;
                  pdir = d;
               }
#endif
               // if there's a gem there, eat it
               if (objmap[pz][ry+y+ydir[d]][rx+x+xdir[d]].type == T_gem) {
                  ++state.gems_stored;  
                  feed_timer = 1200;
                  reverse_timer = 1100;
               }
               objmap[pz][ry+y][rx+x].type = 0;
               objmap[pz][ry+y+ydir[d]][rx+x+xdir[d]].type = T_rover;
               objmap[pz][ry+y+ydir[d]][rx+x+xdir[d]].dir = d;
            }
            rovers[i].s = -1;
            any_change = 1;
         }
      }
      if (!any_change)
         break;
   }
   if (reverse_timer == 0 && num_rovers)
      reverse_timer = -1;
}

obj rotate_obj(obj o)
{
   o.dir = (o.dir + 1) & 3;
   return o;
}

int rotate_tile(int t)
{
   switch (t) {
      case TILE_arrow_e: return TILE_arrow_n;
      case TILE_arrow_n: return TILE_arrow_w;
      case TILE_arrow_w: return TILE_arrow_s;
      case TILE_arrow_s: return TILE_arrow_e;
   }
   return t;
}

static void timestep(uint ms)
{
   if (state.has_wand && state.gems_stored < 0)
      state.gems_stored = 0;

   if (shot_timer > 0) {
      shot_timer -= ms;
   }
   if (feed_timer > 0) {
      feed_timer -= ms;
   }
   if (reverse_timer > 0) {
      reverse_timer -= ms;
      if (reverse_timer < 0)
         reverse_timer = 0;
   }
   if (magic_phase > 0) {
      magic_timer -= ms;
      while (magic_timer <= 0 && magic_phase > 0) {
         int ax,ay,sx,sy,rx,ry;
         --magic_phase;

         rx = room_x * SIZE_X;
         ry = room_y * SIZE_Y;

         ax = magic_phase % 3;
         ay = magic_phase / 3;
         sx = rx + SIZE_X-1;
         sy = ry + SIZE_Y-1;
         {
            obj o = objmap[pz][ry+ay][rx+ax];
            objmap[pz][ry+ay][rx+ax] = rotate_obj(objmap[pz][ry+ax][sx-ay]);
            objmap[pz][ry+ax][sx-ay] = rotate_obj(objmap[pz][sy-ay][sx-ax]);
            objmap[pz][sy-ay][sx-ax] = rotate_obj(objmap[pz][sy-ax][rx+ay]);
            objmap[pz][sy-ax][rx+ay] = rotate_obj(o);
         }
         {
            int t = tilemap[pz][ry+ay][rx+ax];
            tilemap[pz][ry+ay][rx+ax] = rotate_tile(tilemap[pz][ry+ax][sx-ay]);
            tilemap[pz][ry+ax][sx-ay] = rotate_tile(tilemap[pz][sy-ay][sx-ax]);
            tilemap[pz][sy-ay][sx-ax] = rotate_tile(tilemap[pz][sy-ax][rx+ay]);
            tilemap[pz][sy-ax][rx+ay] = rotate_tile(t);
         }

         if ((py == ry+ay && px == rx+ax) ||
             (py == ry+ax && px == sx-ay) ||
             (py == sy-ay && px == sx-ax) ||
             (py == sy-ax && px == rx+ay)) {
            int t;
            px = px - rx;
            py = py - ry;
            t = px;
            px = py;
            py = SIZE_Y-1 - t;
            pdir = (pdir+1) & 3;
            px += rx;
            py += ry;
         }
         magic_timer += MAGIC_STEP_TIME;
      }
      queued_key = 0;
      return; 
   }
   if (rover_timer >= 0 && (state.gems_stored < MAX_GEMS || reverse_timer > 0)) {
      rover_timer -= ms;
      while (rover_timer < 0 && (state.gems_stored < MAX_GEMS || reverse_timer > 0)) {
         int x,y;
         rover_timer += ROVER_MOVE_TIME;
         for (y=0; y < NUM_Y; ++y)
            for (x=0; x < NUM_X; ++x)
               move_rovers(x,y);
      }
   }

   if (player_timer > 0) {
      player_timer -= ms;
   }
   if (checkpoint_timer > 0) {
      checkpoint_timer -= ms;
   }
   if (player_timer <= 0 && shot_timer <= 0 && queued_key) {
      do_key(queued_key);
      queued_key = 0;
   }

   if (state.gems_stored >= MAX_GEMS) {
      state.egg_timer += ms;
   }
}

unsigned int animcycle;

static void restart_game(void)
{
   memset(&state, 0, sizeof(state));
   state.gems_stored = -1;
   init_game();
   compute_room();
   compute_powered();
   state.seen[pz] |= 1 << (room_y * NUM_X + room_x);
   feed_timer = 0;
}

char *choices[] =
{
   "CONTINUE GAME",
   "RESTORE FROM WAND GET",
   "START NEW GAME",
   "CREDITS",
   "SAVE AND QUIT",
};
#define NUM_MENU  (sizeof(choices)/sizeof(choices[0]))

struct {
   int color;
   float scale;
   char *text;
} credits[] =
{
   0,3.0,0,
   COLOR_blue, 2.5, "GAME BY",
   0,0.5,0,
   COLOR_yellow, 3, "SEAN BARRETT",

   0,3.0,0,
   COLOR_blue, 2.0, "ENDGAME ART BY",
   0,0.5,0,
   COLOR_yellow, 2.5, "ORYX",

   0,3.0,0,
   COLOR_blue, 2, "TWIST CONCEPT ASSISTANCE",
   0,0.5,0,
   COLOR_yellow, 2.5, "CASEY MURATORI",

   0,3.0,0,
   COLOR_green, 2.0, "LINUX PORT BY",
   0,0.5,0,
   COLOR_violet, 2.5, "DAVID GOW",

   0,3.0,0,
   COLOR_red, 2, "PLAYTEST BY",
   0,0.5,0,
   COLOR_teal, 2.5, "CASEY MURATORI   ZACH SAMUELS",
   COLOR_teal, 2.5, "JONATHAN BLOW    ERIN ROBINSON",
   COLOR_teal, 2.5, "ALEX MARTIN      DAVE MOORE",
};
#define NUM_CREDITS  (sizeof(credits)/sizeof(credits[0]))

void get_choices(int enabled[8])
{
   int i;
   for (i=0; i < 8; ++i)
      enabled[i] = 1;
   if (!game_started)
      enabled[0] = 0;
   if (!state.has_wand)
      enabled[1] = 0;
}

static void move_selection(int dir)
{
   int enabled[8];
   get_choices(enabled);
   for(;;) {
      menu_selection = (menu_selection + dir + NUM_MENU) % NUM_MENU;
      if (enabled[menu_selection])
         break;
   }
}

gamestate loaded_game;

#ifdef _WIN32
static char *get_savegame_path(void)
{
   // should save this to User blah blah
   return "data2/save.dat";
}
#else
static char *get_savegame_path(void)
{
	char *root_dir = SDL_getenv("XDG_DATA_HOME");
	static int gotpath = FALSE;
	static char save_dir[PATH_MAX] = {0};
	if (gotpath) return save_dir;
	if (!root_dir)
	{
		root_dir = SDL_getenv("HOME");
		if (!root_dir)
		{
			strcpy(save_dir, "data2/save.dat");
			gotpath = TRUE;
			return save_dir;
		}
		else
		{
			strcpy(save_dir, root_dir);
			strcat(save_dir, "/.local/share/Promesst2/save.dat");
		}
	}
	else
	{
		strcpy(save_dir, root_dir);
		strcat(save_dir, "/Promesst2/save.dat");
	}

	// Make all of the directories leading up to this.
	int i;
	for (i = 0; i < strlen(save_dir); ++i)
	{
		if (save_dir[i] == '/')
		{
			save_dir[i] = '\0';
			mkdir(save_dir, S_IRWXU);
			save_dir[i] = '/';
		}
	}
	gotpath = TRUE;
	return save_dir;
}
#endif

static void load_game(void)
{
   roomsave_big *tail=0;
   FILE *f = fopen(get_savegame_path(), "rb");
   if (!f) return;
   if (!fread(&loaded_game, sizeof(loaded_game), 1, f))
      goto end;
   restore_map(&loaded_game);
   fread(&checkpoint, sizeof(checkpoint), 1, f);
   flush_undo();
   for(;;) {
      roomsave_big r;
      roomsave_big *p;
      if (!fread(&r, sizeof(r), 1, f))
         break;
      p = malloc(sizeof(*p));
      *p = r;
      p->next = 0;
      if (tail)
         tail->next = p;
      else
         undo_chain = p;
      tail = p;
   }
 end:
   fclose(f);
   game_started = 1;
}

static void save_game(void)
{
   roomsave_big *r;
   FILE *f;
   if (!game_started)
      return;
#ifndef RELEASE
return;
#endif
   f = fopen(get_savegame_path(), "wb");
   if (!f) return;
   save_map(&loaded_game);
   fwrite(&loaded_game, sizeof(loaded_game), 1, f);
   fwrite(&checkpoint, sizeof(checkpoint), 1, f);
   r = undo_chain;
   while (r) {
      fwrite(r, sizeof(*r), 1, f);
      r = r->next;
   }
   fclose(f);
}

void do_metagame_key(int key)
{
   if (main_mode != M_menu) {
      main_mode = M_menu;
   } else {
      switch (key) {
         case 'w': move_selection(-1); break;
         case 's': move_selection( 1); break;

         case '\n':
         case '\r':
         case ' ': {
            switch (menu_selection) {
               case 0:
                  main_mode = M_game;
                  game_started = 1;
                  break;
               case 1:
                  restore_map(&checkpoint);
                  game_started = 1;
                  flush_undo();
                  main_mode = M_game;
                  break;
               case 2:
                  restart_game();
                  flush_undo();
                  game_started = 1;
                  main_mode = M_game;
                  break;
               case 3:
                  main_mode = M_credits;
                  break;
               case 4:
                  save_game();
                  exit(0);
            }
         }
      }
   }
}

#define MAX_LOGO 1500 // ms
int logo_time = MAX_LOGO;

void process_metagame(float dt)
{
   if (queued_key) {
      do_metagame_key(queued_key);
      queued_key = 0;
   }
   if (main_mode == M_logo && logo_time > 0) {
      logo_time -= 1000*dt;
      if (logo_time <= 0) {
         main_mode = M_menu;
      }
   }
}



int draw_initialized=0;

void draw_init(void)
{
   draw_initialized = 1;

//   glEnable(GL_CULL_FACE);
   glDisable(GL_TEXTURE_2D);
   glDisable(GL_LIGHTING);
   glDisable(GL_DEPTH_TEST);
   glDepthMask(GL_FALSE);

   glViewport(0,0,screen_x,screen_y);
   glClearColor(0,0,0,0);
   glClear(GL_COLOR_BUFFER_BIT);

   // translate so we draw centered
   if (screen_x*SCREEN_Y > screen_y*SCREEN_X) {
      int w;
      // pixel size is sy/SCREEN_Y, so extent of x is SCREEN_X * sy / SCREEN_y
      w = SCREEN_X * screen_y / SCREEN_Y;
      glViewport((screen_x - w)/2.0, 0, w, screen_y);
      // transform function is:
      // window_x = (sx-w)/2.0 + SCREEN_X * logical_x / w;
      // window_y = SCREEN_Y * logical_y / sy;

      //xs_p2v = (float) SCREEN_X/w;
      //ys_p2v = (float) SCREEN_Y/sy;
      //xoff_p2v = (sx-w)/2.0f;
      //yoff_p2v = 0;
   } else {
      int h = SCREEN_Y * screen_x / SCREEN_X;
      glViewport(0, (screen_y - h)/2, screen_x,h);

      //xs_p2v = (float) SCREEN_X / sx;
      //ys_p2v = (float) SCREEN_Y / h;
      //xoff_p2v = 0;
      //yoff_p2v = (sy-h)/2.0f;
   }

   // force a viewport that matches our size

}

static int blinn_8x8(int p1, int p2)
{
   int m = p1*p2 + 128;
   return (m + (m>>8)) >> 8;
}

static void draw_rect(int x, int y, int w, int h, int is0, int it0, int is1, int it1)
{
   float s0,s1,t0,t1;

   s0 = (is0+0.05) / 128.0;
   t0 = (it0+0.05) / 128.0;
   s1 = (is1-0.05) / 128.0;
   t1 = (it1-0.05) / 128.0;

   glTexCoord2f(s0,t0); glVertex2i(x   ,y   );
   glTexCoord2f(s1,t0); glVertex2i(x+w ,y   );
   glTexCoord2f(s1,t1); glVertex2i(x+w ,y+h );
   glTexCoord2f(s0,t1); glVertex2i(x   ,y+h );
}

static void draw_rectf(int x, int y, float w, float h, int is0, int it0, int is1, int it1)
{
   float s0,s1,t0,t1;

   s0 = (is0+0.05) / 128.0;
   t0 = (it0+0.05) / 128.0;
   s1 = (is1-0.05) / 128.0;
   t1 = (it1-0.05) / 128.0;

   glTexCoord2f(s0,t0); glVertex2f(x   ,y   );
   glTexCoord2f(s1,t0); glVertex2f(x+w ,y   );
   glTexCoord2f(s1,t1); glVertex2f(x+w ,y+h );
   glTexCoord2f(s0,t1); glVertex2f(x   ,y+h );
}


static void draw_sprite_raw(int x, int y, int s, int t)
{
   x = x*16 - 12;
   y = y*16 - 12;

   draw_rect(x, y, 16, 16, s*16, t*16, (s+1)*16,(t+1)*16);
}

static void draw_sprite(int x, int y, int s, int t, color *c, float a)
{
   if (c)
      glColor4ub(c->r, c->g, c->b, (int) (c->a * a));
   else
      glColor4f(1,1,1,a);

   x = x*16 - 12;
   y = y*16 - 12;

   draw_rect(x, y, 16, 16, s*16, t*16, (s+1)*16,(t+1)*16);
}

static void draw_subsprite(int x, int y, int dx, int dy, int s, int t, color *c, float a)
{
   if (c)
      glColor4ub(c->r, c->g, c->b, (int) (c->a * a));
   else
      glColor4f(1,1,1,a);

   x = x*16 - 12 + dx;
   y = y*16 - 12 + dy;

   draw_rect(x, y, 16, 16, s*16, t*16, (s+1)*16,(t+1)*16);
}

static char *font = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789>^<v/ ";
int fsize[42] =
{  // A B C D E F G H I J K L M N O P Q R S T U V W X Y Z 0 1 2 3 4 5 6 7 8 9 > ^ < v / 
      4,4,4,4,4,4,4,4,3,4,4,4,5,4,4,4,4,4,4,3,4,5,5,5,5,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,4
};

static int draw_text(int x, int y, float size, char *text, int extra_spacing)
{
   while (*text) {
      char *p = strchr(font, *text);
      if (p) {
         int ch = p - font;
         int s = 1 + (ch % 21)*6;
         int t = 113 + (ch/21)*8;
         draw_rect(x, y, size*5, size*7, s, t, s+5,t+7);
         x += size*(fsize[ch]+1+extra_spacing);
      }
      ++text;
   }
   return x;
}

static int textlen(float size, char *text, int extra_spacing)
{
   int x = 0;
   while (*text) {
      char *p = strchr(font, *text);
      if (p) {
         int ch = p - font;
         int s = 1 + (ch % 21)*6;
         int t = 113 + (ch/21)*8;
         x += size*(fsize[ch]+1+extra_spacing);
      }
      ++text;
   }
   return x;
}



void draw_metagame(float flicker)
{
   int i;
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0,800,600,0,-1,1);


// draw background

   glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   glEnable(GL_BLEND);

   if (main_mode == M_logo)
      glBindTexture(GL_TEXTURE_2D, sss_tex);
   else 
      glBindTexture(GL_TEXTURE_2D, tex);  // sprite sheet

   glEnable(GL_TEXTURE_2D);
   glBegin(GL_QUADS);

   for (i=0; i < 24; ++i) {
      int s,t;
      int x = (rand() % 32)-32;
      int y = (rand() % 32)-32;
      int r = (rand() % 32)+32;
      int g = (rand() % 32)+32;
      int b = (rand() % 32)+32;
      int z = 6;//(rand() % 3) + 3;
      glColor4ub(r,g,b, 32);
      s = rand() % 128;
      t = rand() % 128;
      if (main_mode == M_logo) {
         float t = (logo_time-100) / (float) MAX_LOGO;
         if (t < 0) t = 0;
         t = 1-sqrt(t);
         x += 16;
         y += 16;
         x = x * (t+1)*1;
         y = y * (t+1)*1;
         glColor4ub(r,g,b, 48 * (1-t));
         draw_rect(x,y+100,832,432, 0,0,128,128);
      } else {
         glColor4ub(r,g,b, 80);
         draw_rect(x,y,832,632, s+16*z,t+12*z,s,t); // random fragments of sprite sheet
         if (i == 16)
            break;
      }
   }
   rand(); // above computes 7*24 = 168, we go to 169 here to go more out of phase with 2^15
   glEnd();

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (main_mode == M_logo) {
      float t = (logo_time-250) / (float) MAX_LOGO;
      if (t < 0) t = 0;
      //t = sqrt(t);
      glColor4f(1,1,1,1-t);
      glBegin(GL_QUADS);
      draw_rect(0,0+100,832,432, 0,0,128,128);
      glEnd();

      return;
   }

// draw title


// draw menu or credits

   glBindTexture(GL_TEXTURE_2D, title_tex);
   glColor3f(1,1,1);
   glBegin(GL_QUADS);
   draw_rect(100, main_mode == M_menu ? 0 : -105, 600,300, 0,0,128,128);
   glEnd();

   glBindTexture(GL_TEXTURE_2D, tex);
   glBegin(GL_QUADS);

   if (main_mode == M_menu) {
      int sx = 400, sy = 320;
      int i;
      int enabled[8];
      get_choices(enabled);

      for (i=0; i < NUM_MENU; ++i) {
         if (enabled[i]) {
            int len = strlen(choices[i]);
            int selected = (menu_selection == i);
            float scale = (selected ? 5 : 4);
            int width = len * 6 * scale;

            if (!selected) {
               glColor4f(0.55,0.35,0.55,1.0);
            } else {
               glColor4f(1,0.5,1,0.5+0.5*flicker);
            }
            draw_text(sx - width/2, sy, scale, choices[i], 1);
            sy += 50;
         }
      }
   } else {
      int sx = 400, sy = 180;
      int i;
      for (i=0; i < NUM_CREDITS; ++i) {
         if (credits[i].text) {
            //int len = strlen(credits[i].text);
            int selected = (menu_selection == i);
            float scale = credits[i].scale * 1.2;
            float width = textlen(scale, credits[i].text, 0);
            color c = powers[credits[i].color];
            if ((animcycle/500) % NUM_CREDITS == (i*7)%NUM_CREDITS) { //(NUM_CREDITS*7) == (i*71) % (NUM_CREDITS*7)) {
               glColor4ub(c.r,c.g,c.b, (int) (255*flicker));
            } else {
               glColor4ubv(&c.r);
            }
            draw_text(sx - width/2, sy, scale, credits[i].text, 0);
         }
         sy += 8.5 * credits[i].scale;
      }
   }
   glEnd();
}


void draw_world(void)
{
   color c;
   int on_top_t = 0, ox, oy;
   int x,y,sx;
   int epx,epy;
   int power = has_power[room_y][room_x];
   int a = animcycle / 10;

   float flicker = fabs(sin(animcycle/100.0))*0.5 + 0.5;

   if (a % 130 == 0 || a % 217 == 0 || a % 252 == 0)
      flicker += 0.5;
   if (a % 103 == 0 || a % 117 == 0 || a % 501 == 0)
      flicker *= 0.7;
   if (a % 88 == 0 || a % 281 == 0)
      flicker *= 0.5;      
   if (flicker > 1) flicker = 1;
   if (flicker < 0.2) flicker = 0.2;

   if (main_mode != M_game) {
      draw_metagame(flicker);
      return;
   }

   tile_sprite[0].s = (room_x ^ room_y) & 1 ? 7 : 0;

   propagate_light();

   glBindTexture(GL_TEXTURE_2D, tex);
   glEnable(GL_TEXTURE_2D);

   get_map_color(&c, room_x, room_y, room_z);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0,140,104,0,-1,1);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_BLEND);

   glBegin(GL_QUADS);
   for (y=-1; y <= SIZE_Y; ++y) {
      for (x=-1; x <= SIZE_X; ++x) {
         color *z, lit;
         int mx = ((room_x+NUM_X)*SIZE_X + x) % (SIZE_X*NUM_X);
         int my = ((room_y+NUM_Y)*SIZE_Y + y) % (SIZE_Y*NUM_Y);
         // draw map
         int t = tilemap[room_z][my][mx];

         if (any_light_for_dir[my][mx]) {
            int d;
            float r=255,g=255,b=255,a=1;
            for (d=0; d < 4; ++d)
               if (light_for_dir[my][mx][d] >= 0) {
                  int c = light_for_dir[my][mx][d];
                  r += powers[c].r;
                  g += powers[c].g;
                  b += powers[c].b;
                  a += 1;
               }
            lit.r = r/a;
            lit.g = g/a;
            lit.b = b/a;
            lit.a = 255;
            z = &lit;
         } else
            z = &c;

         draw_sprite(x+1,y+1, tile_sprite[t].s,tile_sprite[t].t, z, 1);
         if (t >= TILE_floor) {
            int r;
            r = (mx + 1); if (r == SIZE_X*NUM_X) r = 0;
            if (tilemap[room_z][my][r] == TILE_wall) draw_sprite(x+1,y+1, 1,0, z,1);
            r = (mx - 1); if (r < 0) r = SIZE_X*NUM_X - 1;
            if (tilemap[room_z][my][r] == TILE_wall) draw_sprite(x+1,y+1, 3,0, z,1);
            r = (my + 1); if (r == SIZE_Y*NUM_Y) r = 0;
            if (tilemap[room_z][r][mx] == TILE_wall) draw_sprite(x+1,y+1, 4,0, z,1);
            r = (my - 1); if (r < 0) r = SIZE_Y*NUM_Y - 1;
            if (tilemap[room_z][r][mx] == TILE_wall) draw_sprite(x+1,y+1, 2,0, z,1);
         }
      }
   }

   for (y=-1; y <= SIZE_Y; ++y) {
      for (x=-1; x <= SIZE_X; ++x) {
         color *z, lit;
         obj o;
         int mx = ((room_x+NUM_X)*SIZE_X + x) % (SIZE_X*NUM_X);
         int my = ((room_y+NUM_Y)*SIZE_Y + y) % (SIZE_Y*NUM_Y);
         int ds,dt=0,sx,sy, t;

         if (any_light_for_dir[my][mx]) {
            int d;
            float r=255,g=255,b=255,a=1;
            for (d=0; d < 4; ++d)
               if (light_for_dir[my][mx][d] >= 0) {
                  int c = light_for_dir[my][mx][d];
                  r += powers[c].r;
                  g += powers[c].g;
                  b += powers[c].b;
                  a += 1;
               }
            lit.r = r/a;
            lit.g = g/a;
            lit.b = b/a;
            lit.a = 255;
            z = &lit;
         } else
            z = NULL;

         // draw obj
         o = objmap[room_z][my][mx];
         t = o.type;
         switch (obj_sprite_hasdir[t]) {
            case 1: ds = o.dir; break;
            case 2: ds = (animcycle>>7) % 3; break;
            default: ds = 0;
         }
         if (t == T_rover) {
            //if (x >= 0 && y >= 0 && x < SIZE_X && y < SIZE_Y) {
            if (state.egg_timer > 0) {
               on_top_t = t;
               ox = x;
               oy = y;
               continue;
            } else {
               int t = rover_timer - ROVER_MOVE_TIME*3/4;
               ds = (mx ^ my) & 1;
               if (t > 0) {
                  sx = -xdir[o.dir] * 16 * t / (ROVER_MOVE_TIME/4);
                  sy = -ydir[o.dir] * 16 * t / (ROVER_MOVE_TIME/4);
                  ds = (animcycle>>8) % 2;
               }
               dt = o.dir;
               //} else {
               //   sx = sy = 0;
               //}
            }
         } else
            sx = sy = 0;
         draw_subsprite(x+1,y+1, sx,sy, obj_sprite[t].s+ds, obj_sprite[t].t+dt, z, 1);

         if (x >= 0 && x < SIZE_X && y >= 0 && y < SIZE_Y) {
            if (t == T_projector) {
               int p = has_power[my/SIZE_Y][mx/SIZE_X];
               draw_sprite(x+1,y+1, 4+p, 2, &powers[o.color], 0.5);
            }
         }
      }
   }

   // draw player
   if (player_timer > PLAYER_MOVE_TIME - PLAYER_TRAVEL_TIME) {
      epx = - (16 * xdir[pdir] * (player_timer - (PLAYER_MOVE_TIME - PLAYER_TRAVEL_TIME)) / PLAYER_TRAVEL_TIME);
      epy = - (16 * ydir[pdir] * (player_timer - (PLAYER_MOVE_TIME - PLAYER_TRAVEL_TIME))/ PLAYER_TRAVEL_TIME);
   } else {
      epx = epy  =0;
   }

   draw_subsprite(px - room_x*SIZE_X+1, py - room_y*SIZE_Y+1, epx, epy, pdir,6, 0,1);
   if (0) {
      int colors[4], num_colors = 0;
      int d = 0;
      color *c;
      for (d=0; d < 4; ++d)
         if (light_for_dir[py][px][d])
            colors[num_colors++] = light_for_dir[py][px][d];
      if (num_colors) {
         float a = (512 - abs((animcycle&1023) - 512)) / 512.0;
         c = &powers[colors[(animcycle >> 10) % num_colors]];
         draw_sprite(epx - room_x*SIZE_X+1, epy - room_y*SIZE_Y+1, 1,1, c,a*0.5);
      }
   }

   if (shot_timer > 0) {
      x = shot_x - room_x*SIZE_X + 1;
      y = shot_y - room_y*SIZE_Y + 1;
      x = x*16 - 12;
      y = y*16 - 12;
      glColor3f(1,1,1);
      draw_rect(x+4,y+4,8,8, 32,48,48,64);
   }

   if (on_top_t) {
      // compute lizard speech color (found below) and use for lizard
      if (state.egg_timer <= 3000) {
         float weight = state.egg_timer / 3000.0;
         float hue = fmod(animcycle / 77.0f, 3);
         float sat = stb_lerp(sin(animcycle / 120.0f)/2+0.5, 0.5,0.9) * weight;
         float lum = (sin(animcycle/33.0)/2+0.5)/2+0.5;
         float r,g,b;
         lum = stb_lerp(weight, 1,lum);
         r = (1-stb_clamp(fabs(hue-1),0,1));
         g = (1-stb_clamp(fabs(hue-2),0,1));
         b = (1-stb_clamp(fabs(hue-3),0,1)+1-stb_clamp(hue,0,1));
         r = stb_lerp(sat, 1,r)*(lum/2+0.5);
         g = stb_lerp(sat, 1,g)*lum;
         b = stb_lerp(sat, 1,b)*(lum/2+0.5);
         r = (r+1)/2;
         g = (r+1)/2;
         b = (r+1)/2;
         glColor3f(r,g,b);
         draw_sprite_raw(ox+1,oy+1, obj_sprite[on_top_t].s, obj_sprite[on_top_t].t+3);
      } else {
         // now flash between lizard and newshape
         float scale = ((state.egg_timer)-3000)/3000.0f;
         if (scale > 1) scale = 1;
         scale = scale/2 + 0.5;
         glColor3f(1,1,1);
         if (state.egg_timer % 200 < (state.egg_timer-3000)/20 || state.egg_timer > 6000) {
            draw_rectf(ox*16+4,oy*16+4,32*scale,32*scale, 64,48, 96,80);
         } else {
            int s = obj_sprite[on_top_t].s;
            int t = obj_sprite[on_top_t].t+3;
            draw_rectf(ox*16+4,oy*16+4,32*scale,32*scale, s*16,t*16,s*16+16,t*16+16);
         }
      }
   }
   glEnd();


   // do all additive rendering

   glBlendFunc(GL_SRC_ALPHA, GL_ONE);
   glEnable(GL_BLEND);

   glBegin(GL_QUADS);
   for (y= -1; y <= SIZE_Y; ++y) {
      for (x= -1; x <= SIZE_X; ++x) {
         int mx = ((room_x+NUM_X)*SIZE_X + x) % (SIZE_X*NUM_X);
         int my = ((room_y+NUM_Y)*SIZE_Y + y) % (SIZE_Y*NUM_Y);
         obj o = objmap[room_z][my][mx];
         int t = o.type;
         if (t == T_projector) {
            if (has_power[my / SIZE_Y][mx / SIZE_X]) {
               draw_sprite(x+1,y+1, 4+1    , 2, &powers[o.color], flicker * 0.5);
               draw_sprite(x+1,y+1, 0+o.dir, 5, &powers[o.color], flicker * 0.25);
            }
         }
         if (any_light_for_dir[my][mx]) {
            int d;
            for (d=0; d < 4; ++d) {
               draw_sprite(x+1,y+1, 4+(d&1), 5, &powers[light_for_dir[my][mx][d]], flicker * 0.25);
            }
         }
      }
   }

   if (1) {
      int colors[4], num_colors = 0;
      int d = 0;
      color *c;
      for (d=0; d < 4; ++d)
         if (light_for_dir[py][px][d] >= 0)
            colors[num_colors++] = light_for_dir[py][px][d];
      if (num_colors) {
         float a = (512 - abs((animcycle&1023) - 512)) / 512.0;
         c = &powers[colors[(animcycle >> 10) % num_colors]];
         draw_subsprite(px - room_x*SIZE_X+1, py - room_y*SIZE_Y+1, epx,epy, 1,1, c,a*0.5);
      }
   }
   glEnd();

   sx = (SIZE_X+1) * 16 - 12 + 4;

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_BLEND);

   glBegin(GL_QUADS);

      for (y= -1; y <= SIZE_Y; ++y) {
         for (x= -1; x <= SIZE_X; ++x) {
            int mx = ((room_x+NUM_X)*SIZE_X + x) % (SIZE_X*NUM_X);
            int my = ((room_y+NUM_Y)*SIZE_Y + y) % (SIZE_Y*NUM_Y);
            obj o = objmap[room_z][my][mx];
            int t = o.type,ds=0;

            // draw obj
            if (t == T_reflector) {
               int ds = o.dir;
               draw_sprite(x+1,y+1, obj_sprite[t].s+ds, obj_sprite[t].t, NULL, 1);
            }
         }
      }


      glColor3f(0,0,0);
      draw_rect(sx, 0, 120,160, 112,16,127,31);
   glEnd();

   // draw lizard speech

   if (state.gems_stored >= 0) {
      if (room_z == 0 && room_x == 2 && room_y == 3) {
         float weight = (float) state.gems_stored / MAX_GEMS;
         float hue = fmod(animcycle / 77.0f, 3);
         float sat = stb_lerp(sin(animcycle / 120.0f)/2+0.5, 0.5,0.9) * weight;
         float lum = (sin(animcycle/33.0)/2+0.5)/2+0.5;
         float r,g,b;
         lum = stb_lerp(weight, 1,lum);
         r = (1-stb_clamp(fabs(hue-1),0,1));
         g = (1-stb_clamp(fabs(hue-2),0,1));
         b = (1-stb_clamp(fabs(hue-3),0,1)+1-stb_clamp(hue,0,1));
         r = stb_lerp(sat, 1,r)*(lum/2+0.5);
         g = stb_lerp(sat, 1,g)*lum;
         b = stb_lerp(sat, 1,b)*(lum/2+0.5);

         glColor3f(r,g,b);
         for (y=0; y < SIZE_Y; ++y)
            for (x=0; x < SIZE_X; ++x)
               if (objmap[room_z][room_y*SIZE_Y + y][room_x*SIZE_X+x].type == T_rover)
                  goto break_two;
        break_two:
         assert(x < SIZE_X && y < SIZE_Y);
         if (state.gems_stored == 0) {
            if (animcycle % 15000 < 2000) {
               glBegin(GL_QUADS);
               draw_rect(x*16+8, y*16+2, 32,16, 80,96, 112,112);
               glEnd();
            }
         } else if (state.egg_timer >= 8000) {
            glColor3f(1,1,1);
            glBegin(GL_QUADS);
            draw_text(x*16-6, y*16+36, 1, "WELL NOW", 1);
            glEnd();
         } else {
            if (animcycle % 45000 < 2000 || feed_timer > 0) {
               char buffer[64], *s = "%d MORE";
               if (state.gems_stored % 7 == 3)
                  s = "%d TO GO";
               if (state.gems_stored >= 1 && state.gems_stored <= 12)
                  s = "NEED MORE";
               sprintf(buffer, s,  MAX_GEMS - state.gems_stored);
               glBegin(GL_QUADS);
               draw_text(x*16+16, y*16+2, 1, buffer, 1);
               glEnd();
            }
         }
      }
   }


   sx = sx*800/140;

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0,800,600,0,-1,1);

   glBegin(GL_QUADS);
   {
      char buffer[64];
      int x, y, sy;
      glColor3f(1,1,1);
      //draw_rect(0,0, sx,1, 127,31,127,31);
      //draw_rect(0,415, sx,1, 127,31,127,31);
      //draw_rect(0,0, 1,415, 127,31,127,31);
      //draw_rect(sx,0, 1,415, 127,31,127,31);
      draw_rect(sx,0,   210,2, 127,31,127,31);
      draw_rect(sx,598, 210,2, 127,31,127,31);
      draw_rect(sx,0,   2,600, 127,31,127,31);
      draw_rect(798,0,  2,600, 127,31,127,31);

      y = 15;
      glColor3f(0.5,0.5,0.5);
      draw_text(sx+16, y, 3, "PROMESST 2", 1);

      y += 30;

      glColor3f(0.75,0.5,0.75);

      draw_text(sx+20, y, 2, "<>^v TO MOVE", 0);
      y += 20;
      draw_text(sx+20, y, 2, "ESC FOR MENU", 0);
      y += 20;
      draw_text(sx+20, y, 2, "BACKSPACE TO", 0);
      y += 15;
      draw_text(sx+50, y, 2, "UNDO 1 ROOM", 0);
      y += 25;

      if (  state.has_wand &&
           (light_for_dir[py][px][0] >= 0 ||
            light_for_dir[py][px][1] >= 0 ||
            light_for_dir[py][px][2] >= 0 ||
            light_for_dir[py][px][3] >= 0)) {
         float n = fabs((animcycle % 4000)/2000.0 - 1);
         glColor3f(0.75,n,0.75);
      } else
         glColor3f(0.25,0.25,0.25);
      draw_text(sx+20, y, 2, "Z TO FIRE", 0);
      y += 20;
      if ((state.num_gems && tilemap[pz][py][px] == TILE_receptacle) || objmap[pz][py][px].type == T_gem)
         glColor3f(0.75,0.5,0.75);
      else
         glColor3f(0.25,0.25,0.25);
      x = draw_text(sx+20, y, 2, "X TO GET/PUT", 0);
      if ((state.num_gems && tilemap[pz][py][px] == TILE_receptacle) || objmap[pz][py][px].type == T_gem)
         glColor3f(1,1,1);
      else
         glColor3f(0.25,0.25,0.25);
      draw_rect(x+6,y-2,16,16, 0,64,0+16,64+16);

      y += 30;

#if 0
    if ((state.num_reflectors && tilemap[pz][py][px] != TILE_wall && objmap[pz][py][px].type == T_empty) || objmap[pz][py][px].type == T_gem)
         glColor3f(0.75,0.5,0.75);
      else
         glColor3f(0.25,0.25,0.25);
      x = draw_text(sx+20, y, 2, "C TO GET/PUT, 0");
      glColor3f(1,1,1);
      draw_rect(x+2,y-3,16,16, 80,96,80+16,96+16);
      y += 20;
#endif

      if (state.num_gems) {
         sprintf(buffer, "%d X ", state.num_gems);
         glColor3f(0.75,0.5,0.75);
         x = draw_text(sx + 46, y, 3, buffer, 0);
         glColor3f(1,1,1);
         draw_rect(x-2,y-4,28,28, 0,64,0+16,64+16);
      }

      y += 50;

      {
         uint8 abilities[8] = { 1,1,1,1, 1,1,1,1 };
         int i;
         int which[5] = { COLOR_red, COLOR_green, COLOR_yellow, COLOR_orange, COLOR_violet };
         char *cname[5] = { "RED" , "GREEN"  , "YELLOW", "ORANGE", "PURPLE" };
         char *aname[5] = { "OPEN", "THROUGH", "SMASH" , "DOUBLE", "ENDPOINT" };
         float ccol[5][3] = { 
            1.0,0.25,0.25,
            0.25,1.0,0.25,
            1.0,1.0,0.25 ,
            1.0,0.5,0.15 ,
            0.75,0.25,1.0,
         };
         get_abilities(abilities);
         glColor3f(0.5,0.5,0.5);
         draw_text(sx+13,y, 3, "/ABILITIES/", 1);
         y += 10;

         for (i=0; i < 5; ++i) {
            int a = which[i];
            y += 25;
            if (abilities[a] || (state.ability_flag & (1 << a))) {
               char *s;
               int w;
               glColor4f(ccol[i][0], ccol[i][1], ccol[i][2], abilities[a] ? 1 : 0.35);
               s = (state.ability_flag & (1 << a)) ? aname[i] : cname[i];
               w = textlen(3, s, 0);
               draw_text((sx+800)/2 - w/2, y, 3, s, 0);
            }
         }
      }

      y += 33;
      glColor3f(0.5,0.5,0.5);
      draw_text(sx + 61, y, 3, "/MAP/", 1);

      sy = y+25;
      for (y=0; y < NUM_Y*SIZE_Y; ++y) {
         int ry = y / SIZE_Y;
         for (x=0; x < NUM_X*SIZE_X; ++x) {
            int rx = x / SIZE_X;
            if (state.seen[room_z] & (1 << (ry*NUM_X+rx))) {
               float a = 0.5;
               if (rx == room_x && ry == room_y)
                  a = 1.0;
               if (room_z == pz && x == px && y == py)
                  glColor4f(0.7,0.5,0.7,a);
               else if (objmap[room_z][y][x].type == T_projector) {
                  color *c = &powers[objmap[room_z][y][x].color];
                  uint8 ia;
                  if (has_power[y/SIZE_Y][x/SIZE_X])
                     ia = (a < 1) ? 128 : 255;
                  else
                     ia = (a < 1) ?  96 : 192;

                  // don't let orange turn brown
                  if (objmap[room_z][y][x].color == 3 && ia < 191)
                     ia += 32;
                     
                  glColor4ub(c->r, c->g, c->b, ia);
               } else if (tilemap[room_z][y][x] == TILE_wall || tilemap[room_z][y][x] == TILE_door) {
                  if (room_z)
                     glColor4f(0.4,0.5,0.6,a);
                  else
                     glColor4f(0.6,0.5,0.4,a);
               } else if (tilemap[room_z][y][x] == TILE_stairs) {
                  glColor4f(0.65,0.7,0.95,a);
               } else if (objmap[room_z][y][x].type == T_gem) {
                  float hue;
                  float sat = 0.6;
                  float lum = 1.0;
                  float r,g,b;
                  float blue;
                  if (objmap[room_z][y][x].dir)
                     hue = fmod(animcycle / 350.0f, 6),
                     lum *= (flicker+2)/3;
                  else
                     hue = fmod(animcycle / 450.0f, 6);
                  r = (1-stb_clamp(fabs(hue-2)-1.00,0,1));
                  g = (1-stb_clamp(fabs(hue-4)-1.00,0,1));
                  b = (1-stb_clamp(fabs(hue-6)-1.00,0,1)+1-stb_clamp(hue-1.00,0,1));
                  blue = stb_clamp(b-r-g,0,1);
                  sat = stb_lerp(blue, 1.0, 0.7);
                  r = stb_lerp(sat, 1,r)*lum;
                  g = stb_lerp(sat, 1,g)*lum;
                  b = stb_lerp(sat, 1,b)*lum;
                  glColor4f(r,g,b,a);
               } else if (objmap[room_z][y][x].type == T_stone)
                  glColor4f(0.45,0.35,0.25,a);
               else
                  glColor4f(0.35,0.35,0.35,a);
            } else
               glColor4f(0.1,0.1,0.1,1);
            draw_rect(sx+20+x*7,sy+y*7,7,7, 127,31,127,31);
         }
      }
   }

   if (checkpoint_timer > 0) {
      glColor4f(0.5,1.0,0.5,flicker);
      draw_text(60, checkpoint_timer/2 - 120, 8, "CHECKPOINT", 1);
      draw_text(160, checkpoint_timer/2 - 40, 8, "SAVED", 1);
   }
   glEnd();


   if (state.egg_timer > 8000) {
      int a = (state.egg_timer - 8000) >> 4;
      int r = powers[COLOR_violet].r;
      int g = powers[COLOR_violet].g;
      int b = powers[COLOR_violet].b;
      if (a >= 128) {
         r += a-128; if (r >= 255) r = 255;
         g += a-128; if (g >= 255) g = 255;
         b += a-128; if (b >= 255) b = 255;
         if (a >= 255) a = 255;
      }
      glBlendFunc(GL_SRC_ALPHA, GL_ADD);
      glEnable(GL_BLEND);
      glColor4ub(r,g,b,a);
      glBegin(GL_QUADS);
      draw_rect(0,0, 800,600, 112,16,127,31);
      glEnd();
      if (state.egg_timer > 14000) {
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         glEnable(GL_BLEND);
         glBegin(GL_QUADS);
         glColor3f(0,0,0);
         draw_text(150,250, 12, "YOU WIN", 1);
         if (state.egg_timer > 17000) {
            float a = (state.egg_timer - 17000)/2000.0;
            char buffer[64];
            sprintf(buffer, "USED %d ZAPS", state.num_zaps);
            if (a > 1) a = 1;
            glColor4f(0.5,0,0.5, a);
            draw_text(250,400,4, buffer, 1);
         }
         glEnd();
      }
      if (state.egg_timer > 22000)
         exit(0);  // shouldn't call save!
   }
}

void draw()
{
   draw_init();
   draw_world();
   stbwingraph_SwapBuffers(NULL);
}

static int initialized=0;
static float last_dt;

int active;

int loopmode(float dt, int real, int in_client)
{
   uint eff_time;
   float actual_dt = dt;

   if (!initialized) return 0;

   animcycle += (uint) ((dt) * 1000 * TIME_SCALE);

   if (main_mode != M_game) {
      process_metagame(dt);
   } else if (dt) {
      if (dt > 0.25) dt = 0.25;
      if (dt < 0.01) dt = 0.01;

      eff_time = (uint) ((dt) * 1000 * TIME_SCALE);
      animcycle += eff_time;

      timestep(eff_time);
   }

   if (active || !real)
      draw();

#ifdef _DEBUG
   return 0;
#else
   return active ? 0 : STBWINGRAPH_update_pause;
#endif
}

int winproc(void *data, stbwingraph_event *e)
{
   switch (e->type) {
      case STBWGE_create:
         active = 1;
         break;

      case STBWGE_char:
         switch(e->key) {
#if 0
            case 27:
               stbwingraph_ShowCursor(NULL,1);
                  return STBWINGRAPH_winproc_exit;
               break;
#endif
            default:
               queued_key = e->key;
               break;
         }
         break;

      case STBWGE_destroy:
         save_game();
         break;

      case STBWGE_keydown:
         switch (e->key) {
            case VK_DOWN : queued_key = 's'; break;
            case VK_LEFT : queued_key = 'a'; break;
            case VK_RIGHT: queued_key = 'd'; break;
            case VK_UP   : queued_key = 'w'; break;
         }
         break;
      case STBWGE_keyup:
         break;

      case STBWGE_deactivate:
         active = 0;
         break;
      case STBWGE_activate:
         active = 1;
         break;

      case STBWGE_size:
         screen_x = e->width;
         screen_y = e->height;
         loopmode(0,1,0);
         break;

      case STBWGE_draw:
         if (initialized)
            loopmode(0,1,0);
         break;

      default:
         return STBWINGRAPH_unprocessed;
   }
   return 0;
}

void stbwingraph_main(void)
{
   stbwingraph_Priority(2);
   stbwingraph_CreateWindow(1, winproc, NULL, APPNAME, SCREEN_X,SCREEN_Y, 0, 1, 0, 0);
   stbwingraph_ShowCursor(NULL, 0);

   init_graphics();

#ifdef RELEASE
   restart_game(); // necessary to populate the edge table on restore
   load_game();
   if (game_started)
      menu_selection = 0;
#else
   restart_game();
   main_mode = M_game;
#endif
   initialized = 1;

   stbwingraph_MainLoop(loopmode, 0.016f);   // 30 fps = 0.033
}
