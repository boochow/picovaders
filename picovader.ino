#include "Arduboy.h"

Arduboy arduboy;

#define DRAW_PIXEL(x,y,c) arduboy.drawPixel((x), (y), (c));
#define GET_PIXEL(x,y) arduboy.getPixel((x), (y))
#define DRAW_BITMAP(x,y,b,w,h,c) arduboy.drawBitmap((x), (y), (b), (w), (h), (c))
#define DRAW_RECT(x,y,w,h,c) arduboy.drawRect((x), (y), (w), (h), (c))
#define FILL_RECT(x,y,w,h,c) arduboy.fillRect((x), (y), (w), (h), (c))
#define DRAW_H_LINE(x,y,l,c) arduboy.drawFastHLine((x),(y),(l),(c))
#define DRAW_V_LINE(x,y,l,c) arduboy.drawFastVLine((x),(y),(l),(c))

#define MAXINT16 65535

boolean sound_on = true;

void ToneWithMute(unsigned int frequency, unsigned long duration) {
  if (sound_on)
    arduboy.tunes.tone(frequency, duration);
}

void draw7seg(int16_t x, int16_t y, uint8_t n) {
  const uint8_t digit[10] = {
    0x5f, 0x06, 0x3b, 0x2f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    //{1, 0, 1, 1, 1, 1, 1}, // bits and lights map
    //{0, 0, 0, 0, 1, 1, 0}, //    6  6  5
    //{0, 1, 1, 1, 0, 1, 1}, //    0     5
    //{0, 1, 0, 1, 1, 1, 1}, //    0  1  5
    //{1, 1, 0, 0, 1, 1, 0}, //    2     4
    //{1, 1, 0, 1, 1, 0, 1}, //    2     4
    //{1, 1, 1, 1, 1, 0, 1}, //    3  3  4
    //{0, 0, 0, 0, 1, 1, 1}, //
    //{1, 1, 1, 1, 1, 1, 1}, //
    //{1, 1, 0, 1, 1, 1, 1}  //
  };
  n = n % 10;
  uint8_t img = digit[n];
  DRAW_H_LINE(x, y, 2, img & 1);
  DRAW_V_LINE(x+2, y, 3, (img = img >> 1) & 1);
  DRAW_V_LINE(x+2, y+3, 3, (img = img >> 1) & 1);
  DRAW_H_LINE(x, y+5, 2, (img = img >> 1) & 1);
  DRAW_V_LINE(x, y+3, 2, (img = img >> 1) & 1);
  DRAW_PIXEL(x + 1, y + 2, (img = img >> 1) & 1);
  DRAW_V_LINE(x, y+1, 2, (img = img >> 1) & 1);
}

#define OBJ_READY -1
#define OBJ_ACTIVE 0
#define OBJ_RECYCLE 1


/////////////////////////////////////////////////////////
//  Invaders
/////////////////////////////////////////////////////////

#define IV_ROW 5
#define IV_COLUMN 10
#define IV_NUM (IV_ROW * IV_COLUMN)
#define IV_TOP 7
#define IV_W 5
#define IV_H 3
#define IV_HSPACING 2
#define IV_VSPACING 3
#define IV_VMOVE 3
#define IV_SHOW_HIT 24

#define IV_W_OF_ALIENS (IV_COLUMN * (IV_W + IV_HSPACING) - IV_HSPACING)
#define IV_H_OF_ALIENS (IV_ROW * (IV_H + IV_VSPACING) - IV_VSPACING)

#define SCRN_LEFT  19
#define SCRN_RIGHT 107
#define SCRN_BOTTOM 64

const uint8_t iv1_img[2][IV_W] PROGMEM = {
  { 0x00, 0x03, 0x06, 0x03, 0x00 },
  { 0x00, 0x06, 0x03, 0x06, 0x00 }
};

const uint8_t iv2_img[2][IV_W] PROGMEM = {
  { 0x03, 0x06, 0x03, 0x06, 0x03 },
  { 0x06, 0x03, 0x06, 0x03, 0x06 }
};

const uint8_t iv3_img[2][IV_W] PROGMEM = {
  { 0x03, 0x05, 0x06, 0x05, 0x03 },
  { 0x06, 0x05, 0x02, 0x05, 0x06 }
};

const uint8_t iv_hit_img[IV_W] PROGMEM =
{ 0x05, 0x00, 0x05, 0x00, 0x05 };

// iv_wait[0] = num of aliens, iv_wait[1] = wait in ticks
#define IV_WAIT_SIZE 10
const uint8_t iv_wait[2][IV_WAIT_SIZE] = {
  { 40, 34, 28, 22, 16, 12,  8,  4,  2,  1 },
  { 10, 9, 7, 6, 5, 4, 3, 2,  1,  0 }
};

struct aliens_t {
  boolean exist[IV_NUM];
  int16_t cur_left;
  int16_t cur_top;
  int16_t nxt_left;
  int16_t nxt_top;
  int16_t bottom;   // be updated in aliens_draw
  int16_t v;      // left move or right move
  uint8_t nxt_update; // which invader will be moved at the next tick
  uint8_t pose;   // index num for iv?_img[]
  boolean move_down;  // down-move flag (updated in aliens_draw)
  boolean touch_down; // an alien touched bottom line flag
  int8_t status;
  uint8_t hit_idx;  // which invader is been hit
  uint8_t alive;    // how many aliens alive
} aliens;

void aliens_init(struct aliens_t *a, uint8_t stg) {
  const int8_t top[9] = {0, 2, 4, 5, 5, 5, 6, 6, 6};
  int16_t y = IV_TOP + top[stg % 9] * IV_H;
  for (uint8_t i = 0; i < IV_NUM; i++)
    a->exist[i] = true;
  a->cur_left = SCRN_LEFT + 9;
  a->v = 1;
  a->nxt_left = a->cur_left + a->v;
  a->cur_top = y;
  a->nxt_top = y;
  a->bottom = 0;
  a->nxt_update = 0;
  a->pose = 0;
  a->move_down = false;
  a->touch_down = false;
  a->status = 0;
  a->hit_idx = 0;
  a->alive = IV_NUM;
}

#define A_ROW(a) (IV_ROW - 1 - (a) / IV_COLUMN)
#define A_COL(a) ((a) % IV_COLUMN)
#define A_IDX(r,c) ((c) + IV_COLUMN * (IV_ROW - (r) - 1))
#define A_XOFS(c) (c * (IV_W + IV_HSPACING))
#define A_YOFS(r) (r * (IV_H + IV_VSPACING))

void aliens_erase(struct aliens_t *a) {
  uint8_t row = A_ROW(a->nxt_update);
  uint8_t col = A_COL(a->nxt_update);
  int16_t x = a->cur_left + A_XOFS(col);
  int16_t y = a->cur_top + A_YOFS(row);
  FILL_RECT(x, y, IV_W, IV_H, BLACK);
}

void aliens_draw(struct aliens_t *a) {
  if (!a->exist[a->nxt_update])
    return;

  uint8_t row = A_ROW(a->nxt_update);
  uint8_t col = A_COL(a->nxt_update);
  int16_t x = a->nxt_left + col * (IV_W + IV_HSPACING);
  int16_t y = a->nxt_top + row * (IV_H + IV_VSPACING);
  uint8_t *img;

  switch (row) {
  case 0: img = (uint8_t *)iv1_img[a->pose]; break;
  case 1:
  case 2: img = (uint8_t *)iv2_img[a->pose]; break;
  case 3:
  case 4: img = (uint8_t *)iv3_img[a->pose]; break;
  }
  DRAW_BITMAP(x, y, img, IV_W, IV_H, WHITE);

  if (((x <= SCRN_LEFT) && (a->v < 0)) ||
    ((x >= SCRN_RIGHT - IV_W) && (a->v > 0)))
    a->move_down = true;

  a->bottom = max(a->bottom, y + IV_H);
}

void aliens_hitimg_erase_alien(struct aliens_t *a) {
  uint8_t row = A_ROW(a->hit_idx);
  uint8_t col = A_COL(a->hit_idx);
  int16_t x;
  int16_t y;

  if (a->hit_idx >= a->nxt_update) {
    x = a->cur_left;
    y = a->cur_top;
  }
  else {
    x = a->nxt_left;
    y = a->nxt_top;
  }
  x += A_XOFS(col);
  y += A_YOFS(row);;
  FILL_RECT(x, y, IV_W, IV_H, BLACK);
}

void aliens_hitimg_erase(struct aliens_t *a) {
  uint8_t row = A_ROW(a->hit_idx);
  uint8_t col = A_COL(a->hit_idx);

  int16_t x = a->nxt_left + A_XOFS(col);
  int16_t y = a->nxt_top + A_YOFS(row);
  FILL_RECT(x, y, IV_W, IV_H, BLACK);
}

void aliens_hitimg_draw(struct aliens_t *a) {
  uint8_t row = A_ROW(a->hit_idx);
  uint8_t col = A_COL(a->hit_idx);
  int16_t x = a->nxt_left + A_XOFS(col);
  int16_t y = a->nxt_top + A_YOFS(row);
  DRAW_BITMAP(x, y, iv_hit_img, IV_W, IV_H, WHITE);
}

void aliens_move(struct aliens_t *a) {
  uint8_t u = a->nxt_update;
  do {
    u++;
  } while ((u < IV_NUM) && (!a->exist[u]));

  if (u < IV_NUM) {
    a->nxt_update = u;
  }
  else {  // all aliens have moved
    if (a->bottom >= SCRN_BOTTOM)
      a->touch_down = true;
    for (u = 0; (u < IV_NUM) && (!a->exist[u]) ; u++);
    a->nxt_update = u;
    a->pose = (a->pose + 1) % 2;
    a->cur_left = a->nxt_left;
    a->nxt_left += a->v;
    a->cur_top = a->nxt_top;
    if (a->move_down) { // this flag had been set in aliens_draw()
      a->move_down = false;
      a->v = -a->v;
      a->nxt_top = a->cur_top + IV_VMOVE;
      a->nxt_left = a->cur_left + a->v;
    }
  }
}

void aliens_hit(struct aliens_t *a, uint16_t idx) {
  a->exist[idx] = false;
  a->hit_idx = idx;
  a->status = IV_SHOW_HIT;
  a->alive--;
}

int8_t aliens_hit_test(struct aliens_t *a, int16_t x, int16_t y) { // returns alien index
  int16_t lmin = min(a->cur_left, a->nxt_left);
  int16_t tmin = min(a->cur_top, a->nxt_top);

  if ((x > lmin) && (x < lmin + IV_W_OF_ALIENS) &&
    (y > tmin) && (y < tmin + IV_H_OF_ALIENS))
  {
    for (uint8_t i = 0; i < a->nxt_update ; i++) {
      if (a->exist[i]) {
        int16_t ax = a->nxt_left + A_XOFS(A_COL(i));
        int16_t ay = a->nxt_top + A_YOFS(A_ROW(i));
        if ((x >= ax) && (x < ax + IV_W) && (y >= ay) && (y < ay + IV_H)) {
          return i;
        }
      }
    }
    for (uint8_t i = a->nxt_update; i < IV_NUM ; i++) {
      if (a->exist[i]) {
        int16_t ax = a->cur_left + A_XOFS(A_COL(i));
        int16_t ay = a->cur_top + A_YOFS(A_ROW(i));
        if ((x >= ax) && (x < ax + IV_W) && (y >= ay) && (y < ay + IV_H)) {
          return i;
        }
      }
    }
  }
  return -1; // no aliens hits
}

void aliens_update(struct aliens_t *a) {
  switch (a->status) {
  case OBJ_ACTIVE:
    aliens_erase(&aliens);
    aliens_draw(&aliens);
    aliens_move(&aliens);
    break;
  case IV_SHOW_HIT:
    aliens_hitimg_erase_alien(a);
    aliens_hitimg_draw(a);
    a->status--;
    break;
  case OBJ_RECYCLE:
    aliens_hitimg_erase(&aliens);
    a->status = OBJ_ACTIVE;
    break;
  default:
    a->status--;

  }
}

/////////////////////////////////////////////////////////
//  UFO
/////////////////////////////////////////////////////////

#define UFO_TOP 0
#define UFO_BOTTOM 3
#define UFO_W 7
#define UFO_H 3
#define UFO_INTERVAL 1500 // 60 ticks * 25 sec
#define UFO_WAIT 3
#define UFO_SHOW_HIT 120

const uint8_t ufo_img[UFO_W] PROGMEM =
{ 0x06, 0x05, 0x07, 0x03, 0x07, 0x05, 0x06 };

const uint8_t ufo_score_img[4][UFO_W] PROGMEM = {
  { 0x00, 0x05, 0x03, 0x00, 0x06, 0x06, 0x00 },
  { 0x07, 0x00, 0x06, 0x06, 0x00, 0x06, 0x06 },
  { 0x07, 0x00, 0x05, 0x03, 0x00, 0x06, 0x06 },
  { 0x05, 0x07, 0x00, 0x06, 0x06, 0x06, 0x06 }
};

struct ufo_t {
  uint16_t wait_ctr;
  uint16_t x;
  int8_t v;
  int8_t l_or_r;
  int8_t status;
  uint8_t img_idx;  // index for ufo_score_img
} ufo;

void ufo_init(struct ufo_t *u) {
  u->wait_ctr = 0;
  u->status = OBJ_READY;
}

void ufo_appear(struct ufo_t *u) {

  if ((aliens.alive > 7) && (u->wait_ctr++ > UFO_INTERVAL)) {

    u->status = OBJ_ACTIVE;
    u->wait_ctr = 0;

    switch (u->l_or_r) {
    case 0:
      u->v = 1;
      u->x = SCRN_LEFT;
      break;
    default:
      u->v = -1;
      u->x = SCRN_RIGHT - UFO_W;
    }
  }
}

void ufo_draw(struct ufo_t *u) {
  DRAW_BITMAP(u->x, UFO_TOP, ufo_img, UFO_W, UFO_H, WHITE);
}

void ufo_erase(struct ufo_t *u) {
  FILL_RECT(u->x, UFO_TOP, UFO_W, UFO_H, BLACK);
}

void ufo_move(struct ufo_t *u) {
  u->x += u->v;
  if ((u->x <= SCRN_LEFT) || (u->x >= SCRN_RIGHT - UFO_W))
    u->status = OBJ_RECYCLE;
}

void ufo_hit(struct ufo_t *u, uint8_t idx) {
  ufo_init(u);
  u->status = UFO_SHOW_HIT;
  ufo_erase(u);
  u->img_idx = idx;
}

int16_t ufo_hit_test(struct ufo_t *u, uint16_t x, uint16_t y) {
  if ((u->status == OBJ_ACTIVE) && 
    (y < UFO_BOTTOM) && 
    (x >= u->x) && (x < u->x + UFO_W)) {

    ufo_init(u);
    u->status = UFO_SHOW_HIT;
    ufo_erase(u);
  }
  else
    return -1;
}

void ufo_update(struct ufo_t *u) {
  switch (u->status) {
  case OBJ_ACTIVE:
    if (u->wait_ctr++ > UFO_WAIT) {
      u->wait_ctr = 0;
      ufo_erase(u);
      ufo_move(u);
      ufo_draw(u);
    }
    break;
  case OBJ_READY:
    ufo_appear(u);
    break;
  case OBJ_RECYCLE:
    ufo_erase(u);
    u->status = OBJ_READY;
    break;
  case UFO_SHOW_HIT:
    DRAW_BITMAP(u->x, UFO_TOP, ufo_score_img[u->img_idx], UFO_W, UFO_H, WHITE);
  default:
    u->status--;
  }
}

/////////////////////////////////////////////////////////
//  Bunkers
/////////////////////////////////////////////////////////

#define BUNKER_TOP 52
#define BUNKER_BOTTOM (BUNKER_TOP + BUNKER_H)
#define BUNKER_W 8
#define BUNKER_H 6
#define BUNKER_LEFT 13
#define BUNKER_SPACE 10
#define BUNKER_NUM 4

const uint8_t bunker_img[BUNKER_W] PROGMEM =
{ 0x3c, 0x3e, 0x1f, 0x0f, 0x0f, 0x1f, 0x3e, 0x3c };

void draw_bunkers() {
  uint16_t x = BUNKER_LEFT + SCRN_LEFT;

  for (uint8_t i = 0; i < BUNKER_NUM; i++) {
    DRAW_BITMAP(x, BUNKER_TOP, bunker_img, BUNKER_W, BUNKER_H, WHITE);
    x += BUNKER_W + BUNKER_SPACE;
  }
}

/////////////////////////////////////////////////////////
//  Bombs
/////////////////////////////////////////////////////////

#define BOMB_NUM 3
#define BOMB_WAIT 3
#define BOMB_V 2
#define BOMB_SHOW_HIT 18

struct bomb_t {
  int16_t x;
  int16_t y;
  int8_t wait_ctr;
  int8_t status;
  void (*draw_func)(struct bomb_t *b, uint8_t color);
  void (*shot_func)(struct bomb_t *b);
} bombs[3];

void bomb_draw(struct bomb_t *b, uint8_t color) {
  b->draw_func(b, color);
}

void bomb_draw_a(struct bomb_t *b, uint8_t color) {
  if (b->status != OBJ_ACTIVE)
    return;
  DRAW_PIXEL(b->x + ((1+ b->y) % 2), b->y - 3, color);
  DRAW_PIXEL(b->x + (b->y/BOMB_V % 2), b->y - 2, color);
  DRAW_PIXEL(b->x + ((1+ b->y) % 2), b->y - 1, color);
  DRAW_PIXEL(b->x + (b->y/BOMB_V % 2), b->y, color);
}

void bomb_draw_b(struct bomb_t *b, uint8_t color) {
  if (b->status != OBJ_ACTIVE)
    return;
  DRAW_PIXEL(b->x + 1, b->y - 1 - (b->y / BOMB_V % 2), color);
  DRAW_PIXEL(b->x, b->y - 1 - (b->y / BOMB_V % 2), color);
  DRAW_PIXEL(b->x + 1, b->y, color);
  DRAW_PIXEL(b->x, b->y, color);
}

void bomb_draw_c(struct bomb_t *b, uint8_t color) {
  if (b->status != OBJ_ACTIVE)
    return;
  DRAW_PIXEL(b->x + ((1 + b->y) / BOMB_V % 2), b->y - 2, color);
  DRAW_PIXEL(b->x + 1, b->y - 1, color);
  DRAW_PIXEL(b->x, b->y - 1, color);
  DRAW_PIXEL(b->x + (b->y / BOMB_V % 2), b->y, color);
}

int8_t search_alien(struct aliens_t *a, int8_t x) {
  int8_t a_idx = -1;
  int8_t y = a->bottom - IV_H;

  for (uint8_t i = 0; i < IV_ROW; i++) {
    a_idx = aliens_hit_test(a, x + 2, y);
    if (a_idx > 0)
      break;
    else
      y -= IV_VSPACING + IV_H;
  }
  return a_idx;
}

void bomb_shot_from(struct bomb_t *b, uint8_t a_idx) {
  while ((a_idx > IV_COLUMN) && aliens.exist[a_idx - IV_COLUMN])
    a_idx -= IV_COLUMN;
  b->status = OBJ_ACTIVE;
  b->x = aliens.nxt_left + A_XOFS(A_COL(a_idx)) + IV_W / 2;
  b->y = aliens.nxt_top + A_YOFS(A_ROW(a_idx)) + (IV_H + IV_VSPACING) * 2;
}

void bomb_shot_a(struct bomb_t *b) {
  if (ufo.status == OBJ_READY)
    bomb_shot_from(b, aliens.nxt_update);
}

void bomb_shot_c(struct bomb_t *b) {
  int8_t a_idx = (aliens.nxt_update + IV_COLUMN + IV_NUM) % IV_NUM;
  if (!aliens.exist[a_idx])
    return;
  bomb_shot_from(b, a_idx);
}

void bomb_init(struct bomb_t *b) {
  b->wait_ctr = 0;
  b->status = BOMB_SHOW_HIT - 1;
  b->y = SCRN_BOTTOM;
  b->draw_func = bomb_draw_a;
  b->shot_func = bomb_shot_a;
}

void bomb_hitimg_draw(struct bomb_t *b, uint8_t color) {
  DRAW_PIXEL(b->x, b->y, color);
  DRAW_PIXEL(b->x+1, b->y, color);
  DRAW_PIXEL(b->x, b->y-1, color);
  DRAW_PIXEL(b->x+1, b->y-1, color);
}

boolean bomb_bunker_test(struct bomb_t *b) {
  if ((b->y < BUNKER_BOTTOM) && (b->y > BUNKER_TOP)) {
    if (GET_PIXEL(b->x, b->y) ||
      GET_PIXEL(b->x + 1, b->y ) ||
      GET_PIXEL(b->x, b->y - 1) ||
      GET_PIXEL(b->x + 1, b->y - 1)) {
      return true;
    }
  }
  return false;
}

void bomb_move(struct bomb_t *b) {
  if (b->status != OBJ_ACTIVE)
    return;
  if (b->wait_ctr++ < BOMB_WAIT)
    return;

  b->wait_ctr = 0;

  b->y += BOMB_V;
  if (b->y > SCRN_BOTTOM)
    b->status = BOMB_SHOW_HIT - 1;
  else if (bomb_bunker_test(b))
    b->status = BOMB_SHOW_HIT;
}

void bomb_shot(struct bomb_t *b) {
  static int8_t wait_ctr = 0;

  if (wait_ctr == 0) {
    b->shot_func(b);
    wait_ctr = 60;
  }
  else
    wait_ctr--;
}

/////////////////////////////////////////////////////////
//  Laser
/////////////////////////////////////////////////////////

#define LASER_V 2
#define LASER_WAIT 0
#define LASER_CNTR_INIT 14
#define LASER_CNTR_MOD 15

const uint8_t laser_misshot_img[3] PROGMEM = { 0x05, 0x02, 0x05 };

struct laser_t {
  int16_t x;
  int16_t y;
  int8_t wait_ctr;
  uint8_t shot_ctr; // for calculating ufo score
  int8_t status;
};

void laser_init(struct laser_t *l) {
  l->status = OBJ_READY;
  l->shot_ctr = LASER_CNTR_INIT;
  l->wait_ctr = 0;
}

void laser_draw_misshot(struct laser_t *l, uint8_t color) {
  DRAW_BITMAP(l->x - 1, l->y, laser_misshot_img, 3, 3, color);
}

boolean laser_bunker_test(struct laser_t *l) {
  if ((l->y < BUNKER_BOTTOM) && (l->y > BUNKER_TOP) && (l->y > aliens.bottom)) {
    if (GET_PIXEL(l->x, l->y) || (GET_PIXEL(l->x, l->y + 1))) {
      return true;
    }
  }
  return false;
}

void laser_move(struct laser_t *l) {
  if (l->wait_ctr++ < LASER_WAIT)
    return;

  l->wait_ctr = 0;
  if (l->status == OBJ_ACTIVE) {
    l->y -= LASER_V;
    if (l->y <= 0) {
      l->status = IV_SHOW_HIT;
    }
    else {
      if (laser_bunker_test(l)) {
        l->status = IV_SHOW_HIT;
      }
    }
  }
}

void laser_draw(struct laser_t *l, uint8_t color) {
  DRAW_PIXEL(l->x, l->y, color);
  DRAW_PIXEL(l->x, l->y + 1, color);
}

void laser_draw_mishit(struct laser_t *l, uint8_t color) {
  DRAW_BITMAP(l->x - 1, l->y, laser_misshot_img, 3, 3, color);
}

void laser_shoot(struct laser_t *l, uint16_t x, uint16_t y) {
  if ((l->status == OBJ_READY) && (aliens.status == OBJ_ACTIVE)) {
    if (arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON)) {
      l->status = OBJ_ACTIVE;
      l->x = x;
      l->y = y;
      l->shot_ctr = (l->shot_ctr + 1) % LASER_CNTR_MOD;
      ufo.l_or_r = l->shot_ctr & 0x1;
    }
  }
}

uint8_t laser_ufo_point(struct laser_t *l) {
  uint8_t idx;

  switch (l->shot_ctr) {
  case 0:
  case 1:
  case 6:
  case 11:
    idx = 0;
    break;

  case 3:
  case 12:
    idx = 2;
    break;

  case 7:
    idx = 3;
    break;

  default:
    idx = 1;
    break;
  }
  return idx;
}

uint8_t laser_update(struct laser_t *l) { // returns point if laser hits
  const uint8_t alien_points[5] = { 3, 2, 2, 1, 1 };
  const uint8_t ufo_points[4] = { 5, 10, 15, 30 };
  int8_t hit;
  uint8_t result = 0;

  switch (l->status) {
  case OBJ_READY:
    break;
  case OBJ_ACTIVE:
    laser_draw(l, BLACK);
    laser_move(l);
    laser_draw(l, WHITE);

    hit = aliens_hit_test(&aliens, l->x, l->y);
    if (hit >= 0) {
      aliens_hit(&aliens, hit);
      laser_draw(l, BLACK);
      result = alien_points[A_ROW(hit)];
      l->status = OBJ_READY;
    }

    if (ufo_hit_test(&ufo, l->x, l->y) >= 0) {
      laser_draw(l, BLACK);
      uint8_t idx = laser_ufo_point(l);
      ufo_hit(&ufo, idx);
      result = ufo_points[idx];
      l->status = OBJ_READY;
    }
    break;
  case IV_SHOW_HIT:
    laser_draw(l, BLACK);
    laser_draw_misshot(l, WHITE);
    l->status--;
    break;
  case OBJ_RECYCLE:
    laser_draw_misshot(l, BLACK);
    l->status = OBJ_READY;
    break;
  default:
    l->status--;
  }

  return result;
}

/////////////////////////////////////////////////////////
//  Cannon
/////////////////////////////////////////////////////////

#define CANNON_W 5
#define CANNON_H 3
#define CANNON_V 1
#define CANNON_C_OFS 2
#define CANNON_WAIT 0
#define CANNON_TOP 61
#define CANNON_SHOW_HIT 195

const uint8_t cannon_img[CANNON_W] PROGMEM =
{ 0x06, 0x06, 0x07, 0x06, 0x06 };

const uint8_t cannon_hit_img[2][CANNON_W] PROGMEM = {
  { 0x01, 0x05, 0x06, 0x06, 0x05 },
  { 0x04, 0x07, 0x04, 0x05, 0x02 }
};

struct cannon_t {
  uint16_t x;
  uint8_t left;
  struct laser_t laser;
  uint8_t wait_ctr;
  int8_t status;
} cannon;

void cannon_init(struct cannon_t *c) {
  c->x = 32;
  c->wait_ctr = 0;
  laser_init(&c->laser);
  c->status = OBJ_ACTIVE;
}

void cannon_erase(struct cannon_t *c) {
  FILL_RECT(c->x, CANNON_TOP, CANNON_W, CANNON_H, BLACK);
}

void cannon_draw(struct cannon_t *c) {
  DRAW_BITMAP(c->x, CANNON_TOP, cannon_img, CANNON_W, CANNON_H, WHITE);
}

void cannon_move(struct cannon_t *c) {
  if (arduboy.pressed(RIGHT_BUTTON)) {
    c->x += CANNON_V;
  }
  if (arduboy.pressed(LEFT_BUTTON)) {
    c->x -= CANNON_V;
  }
  c->x = constrain(c->x, SCRN_LEFT, SCRN_RIGHT - CANNON_W);
}

void cannon_hit(struct cannon_t *c) {
  c->status = CANNON_SHOW_HIT;
  c->laser.status = OBJ_RECYCLE;
}

boolean cannon_hit_test(struct cannon_t *c, uint16_t x, uint16_t y) {
  if ((y > CANNON_TOP) && (y < CANNON_TOP + CANNON_H) && (x >= c->x) && (x <= c->x + CANNON_W)) {
    return true;
  }
  else
    return false;
}

void cannon_update(struct cannon_t *c) {

  switch (c->status) {
  case OBJ_ACTIVE:
    if (c->wait_ctr++ > CANNON_WAIT) {
      c->wait_ctr = 0;
      cannon_erase(c);
      cannon_move(c);
      laser_shoot(&c->laser, c->x + CANNON_C_OFS, CANNON_TOP - 1);
      cannon_draw(c);
    }
    break;
  case OBJ_RECYCLE:
    laser_draw(&c->laser, BLACK);
    c->status = OBJ_READY;
    break;
  default:
    cannon_erase(c);
    uint8_t *img = (uint8_t *)cannon_hit_img[(c->status >> 2) % 2];
    DRAW_BITMAP(c->x, CANNON_TOP, img, CANNON_W, CANNON_H, WHITE);
    c->status--;
    break;
  }
}

void bomb_shot_b(struct bomb_t *b) {
  int8_t a_idx = search_alien(&aliens, cannon.x + 2);
  if (a_idx > 0)
    bomb_shot_from(b, a_idx);
}

void bomb_update(struct bomb_t *b) {
  switch (b->status) {
  case OBJ_READY:
    bomb_shot(b);
    break;
  case OBJ_ACTIVE:
    bomb_draw(b, BLACK);
    bomb_move(b);
    if (cannon_hit_test(&cannon, b->x + 1, b->y)) {
      cannon_hit(&cannon);
      b->status = OBJ_READY;
    }
    else {
      bomb_draw(b, WHITE);
    }
    break;
  case OBJ_RECYCLE:
    bomb_hitimg_draw(b, BLACK);
    b->status = OBJ_READY;
    break;
  case BOMB_SHOW_HIT:
    bomb_hitimg_draw(b, WHITE);
  default:
    b->status--;
  }
}

/////////////////////////////////////////////////////////
//  Game
/////////////////////////////////////////////////////////

enum game_status_t {
  GameLost,
  GameOnGoing,
  GameRestart,
  GameOver
};

struct game_t {
  int16_t score;
  uint8_t stage;
  uint8_t left;
  game_status_t status;
}g_game;

void print_int(int16_t x, int16_t y, uint16_t s, uint8_t digit) {
  x += 4 * (digit - 1);
  for (; digit > 0; digit--) {
    draw7seg(x, y, s % 10);
    x -= 4;
    s /= 10;
  }
}

void print_score() {
  print_int(109, 20, g_game.score, 4);
}

void print_cannon_left(uint8_t n) {
  draw7seg(13, 20, n);
}

void game_initialize() {
  g_game.score = 0;
  g_game.stage = 0;
  g_game.left = 3;
}

boolean game_new_game()
{
  static boolean pressed_A = false;
  static boolean pressed_B = false;
  boolean result;

  result = false;
  if (pressed_A && arduboy.notPressed(A_BUTTON)) {
    pressed_A = false;
    sound_on = true;
    result = true;
  }
  else if (!pressed_A && arduboy.pressed(A_BUTTON)) {
    pressed_A = true;
  }

  if (!result) {
    if (pressed_B && arduboy.notPressed(B_BUTTON)) {
      pressed_B = false;
      sound_on = false;
      result = true;
    }
    else if (!pressed_B && arduboy.pressed(B_BUTTON)) {
      pressed_B = true;
    }
  }

  return result;
}

void game_restart() {
  FILL_RECT(SCRN_LEFT, 0, SCRN_RIGHT - SCRN_LEFT, SCRN_BOTTOM, BLACK);

  cannon_init(&cannon);
  aliens_init(&aliens, g_game.stage);
  ufo_init(&ufo);
  draw_bunkers();

  bomb_init(&bombs[0]);
  bombs[1].draw_func = bomb_draw_a;
  bombs[0].draw_func = bomb_draw_a;

  bomb_init(&bombs[1]);
  bombs[1].draw_func = bomb_draw_b;
  bombs[1].shot_func = bomb_shot_b;

  bomb_init(&bombs[2]);
  bombs[2].draw_func = bomb_draw_c;
  bombs[2].shot_func = bomb_shot_c;
}

void game_main() {

  cannon_update(&cannon);

  if (cannon.status == OBJ_ACTIVE) {
    uint8_t sc = laser_update(&cannon.laser);
    if (sc > 0) {
      g_game.score += sc;
      print_score();
    }

    for (uint8_t i = 0; i < BOMB_NUM; i++)
      bomb_update(&bombs[i]);

    ufo_update(&ufo);

    aliens_update(&aliens);
  }
}

void print_game_over() {
  FILL_RECT(33,23,60,37,BLACK);
  arduboy.setCursor(37, 28);
  arduboy.print(F("GAME OVER"));
}

void setup() {
  arduboy.begin();
  arduboy.clear();

  DRAW_V_LINE(SCRN_LEFT - 1, 0, 64, WHITE);
  DRAW_V_LINE(SCRN_RIGHT, 0, 64, WHITE);

  print_int(125, 20, 0, 1);

  DRAW_BITMAP(4, 22, cannon_img, CANNON_W, CANNON_H, WHITE);
  print_cannon_left(0);

  arduboy.setFrameRate(60);
  g_game.status = GameOver;
}

void loop() {
  if (!(arduboy.nextFrame()))
    return;

  switch (g_game.status) {
  case GameRestart:
    game_restart();
    g_game.status = GameOnGoing;
    break;

  case GameOnGoing:
    game_main();
    if (cannon.status == OBJ_READY) {
      g_game.status = GameLost;
    }
    if ((aliens.alive == 0) && aliens.status == OBJ_ACTIVE) {
      g_game.stage++;
      g_game.status = GameRestart;
    }
    if (aliens.touch_down) {
      print_game_over();
      g_game.status = GameOver;
    }
    break;

  case GameLost:
    print_cannon_left(--g_game.left);
    cannon_erase(&cannon);
    cannon_init(&cannon);
    if (g_game.left > 0)
      g_game.status = GameOnGoing;
    else {
      print_game_over();
      g_game.status = GameOver;
    }
    break;

  case GameOver:
    if (game_new_game()) { // true if A or B button is pushed
      game_initialize();
      print_cannon_left(g_game.left);
      print_score();
      g_game.status = GameRestart;
    }
  }

  arduboy.display();
}
