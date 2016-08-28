#include "Arduboy.h"

Arduboy arduboy;

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
  arduboy.drawFastHLine(x, y, 2, img & 1);
  arduboy.drawFastVLine(x+2, y, 3, (img = img >> 1) & 1);
  arduboy.drawFastVLine(x+2, y+3, 3, (img = img >> 1) & 1);
  arduboy.drawFastHLine(x, y+5, 2, (img = img >> 1) & 1);
  arduboy.drawFastVLine(x, y+3, 2, (img = img >> 1) & 1);
  arduboy.drawPixel(x + 1, y + 2, (img = img >> 1) & 1);
  arduboy.drawFastVLine(x, y+1, 2, (img = img >> 1) & 1);
}


#define IV_ROW 5
#define IV_COLUMN 10
#define IV_NUM (IV_ROW * IV_COLUMN)
#define IV_TOP 6
#define IV_W 5
#define IV_H 3
#define IV_HSPACING 2
#define IV_VSPACING 2
#define IV_VMOVE 2
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
  { 0x03, 0x05, 0x02, 0x05, 0x03 },
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
  int16_t bottom;
  int16_t v;      // left move or right move
  uint8_t nxt_update; // which invader will be moved at next tick
  uint8_t pose;   // index num for iv?_img[]
  boolean move_down;  // down-move flag
  boolean touch_down; // an alien touched bottom line flag
  uint8_t show_hit; // show hit image while >0
  uint8_t hit_idx;  // which invader is been hit
  uint8_t alive;    // how many aliens alive
} aliens;

void aliens_init(struct aliens_t *a) {
  for (uint8_t i = 0; i < IV_NUM; i++)
    a->exist[i] = true;
  a->cur_left = SCRN_LEFT + 9;
  a->v = 1;
  a->nxt_left = a->cur_left + a->v;
  a->cur_top = IV_TOP;
  a->nxt_top = IV_TOP;
  a->bottom = 0; // will be updated in aliens_draw
  a->nxt_update = 0;
  a->pose = 0;
  a->move_down = false;
  a->touch_down = false;
  a->show_hit = 0;
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
  arduboy.fillRect(x, y, IV_W, IV_H, BLACK);
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
  arduboy.drawBitmap(x, y, img, IV_W, IV_H, WHITE);

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
  arduboy.fillRect(x, y, IV_W, IV_H, BLACK);
}

void aliens_hitimg_erase(struct aliens_t *a) {
  uint8_t row = A_ROW(a->hit_idx);
  uint8_t col = A_COL(a->hit_idx);

  int16_t x = a->nxt_left + A_XOFS(col);
  int16_t y = a->nxt_top + A_YOFS(row);
  arduboy.fillRect(x, y, IV_W, IV_H, BLACK);
}

void aliens_hitimg_draw(struct aliens_t *a) {
  uint8_t row = A_ROW(a->hit_idx);
  uint8_t col = A_COL(a->hit_idx);
  int16_t x = a->nxt_left + A_XOFS(col);
  int16_t y = a->nxt_top + A_YOFS(row);
  arduboy.drawBitmap(x, y, iv_hit_img, IV_W, IV_H, WHITE);
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
  a->show_hit = IV_SHOW_HIT;
  a->alive--;
}

int8_t aliens_hit_test(struct aliens_t *a, uint16_t x, uint16_t y) {
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
  return -1;
}

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
    arduboy.drawBitmap(x, BUNKER_TOP, bunker_img, BUNKER_W, BUNKER_H, WHITE);
    x += BUNKER_W + BUNKER_SPACE;
  }
}

#define BOMB_A 0
#define BOMB_B 1
#define BOMB_C 2
#define BOMB_WAIT 3
#define BOMB_V 1
#define BOMB_SHOW_HIT 18

struct bomb_t {
  int16_t x;
  int16_t y;
  uint8_t type; 
  int8_t wait_ctr;
  boolean exists;
  uint8_t show_hit; // show hit image while >0
} bombs[3];

void bomb_init(struct bomb_t *b, uint8_t t) {
  b->exists = false;
  b->type = t;
  b->wait_ctr = 0;
  b->show_hit = 0;
}

void bomb_draw(struct bomb_t *b, uint8_t color) {
  if (!b->exists)
    return;
  arduboy.drawPixel(b->x + ((1+ b->y) % 2), b->y - 3, color);
  arduboy.drawPixel(b->x + (b->y % 2), b->y - 2, color);
  arduboy.drawPixel(b->x + ((1+ b->y) % 2), b->y - 1, color);
  arduboy.drawPixel(b->x + (b->y % 2), b->y, color);
}

void bomb_draw_b(struct bomb_t *b, uint8_t color) {
  if (!b->exists)
    return;
  arduboy.drawPixel(b->x + ((1 + b->y) % 2), b->y - 2, color);
  arduboy.drawPixel(b->x + 1, b->y - 1, color);
  arduboy.drawPixel(b->x, b->y - 1, color);
  arduboy.drawPixel(b->x + (b->y % 2), b->y, color);
}

void bomb_draw_c(struct bomb_t *b, uint8_t color) {
  if (!b->exists)
    return;
  arduboy.drawPixel(b->x + 1, b->y - 1 - (b->y % 2), color);
  arduboy.drawPixel(b->x, b->y - 1 - (b->y % 2), color);
  arduboy.drawPixel(b->x + 1, b->y, color);
  arduboy.drawPixel(b->x, b->y, color);
}


void bomb_hitimg_draw(struct bomb_t *b, uint8_t color) {
  arduboy.drawPixel(b->x, b->y, color);
  arduboy.drawPixel(b->x+1, b->y, color);
  arduboy.drawPixel(b->x, b->y+1, color);
  arduboy.drawPixel(b->x+1, b->y+1, color);
}

boolean bomb_bunker_test(struct bomb_t *b) {
  if ((b->y < BUNKER_BOTTOM) && (b->y > BUNKER_TOP)) {
    if (arduboy.getPixel(b->x + ((1 + b->y) % 2), b->y - 3) ||
      arduboy.getPixel(b->x + (b->y % 2), b->y - 2) ||
      arduboy.getPixel(b->x + ((1 + b->y) % 2), b->y - 1) ||
      arduboy.getPixel(b->x + (b->y % 2), b->y)) {
      return true;
    }
  }
  return false;
}

void bomb_move(struct bomb_t *b) {
  if (b->wait_ctr++ < BOMB_WAIT)
    return;

  b->wait_ctr = 0;

  if (b->exists) {
    b->y += BOMB_V;
    if (b->y > SCRN_BOTTOM) {
      b->exists = false;
    }
    else {
      if (bomb_bunker_test(b)) {
        b->exists = false;
        b->show_hit = BOMB_SHOW_HIT;
      }
    }
  }
}

#define LASER_V 2
#define LASER_WAIT 0
#define LASER_OFS 2
#define LASER_CNTR_INIT 7
#define LASER_CNTR_MOD 15

#define CANNON_V 1
#define CANNON_WAIT 2
#define CANNON_TOP 61
#define CANNON_SHOW_HIT 195

const uint8_t cannon_img[IV_W] PROGMEM =
{ 0x06, 0x06, 0x07, 0x06, 0x06 };

const uint8_t cannon_hit_img[2][IV_W] PROGMEM = {
  { 0x01, 0x05, 0x06, 0x06, 0x05 },
  { 0x04, 0x07, 0x04, 0x05, 0x02 }
};

struct laser_t {
  int16_t x;
  int16_t y;
  int8_t wait_ctr;
  boolean exists;
  uint8_t shot_ctr; // for calculating ufo score
  uint8_t show_misshot; // wait until charge!
};

void laser_init(struct laser_t *l) {
  l->exists = false;
  l->shot_ctr = LASER_CNTR_INIT;
  l->wait_ctr = 0;
}

struct cannon_t {
  uint16_t x;
  uint8_t left;
  struct laser_t laser;
  uint8_t wait_ctr;
  uint8_t show_hit;
} cannon;

void cannon_init(struct cannon_t *c) {
  c->x = 32;
  c->wait_ctr = 0;
  laser_init(&c->laser);
  c->show_hit = 0;
}

const uint8_t laser_misshot_img[3] = { 0x05, 0x02, 0x05 };

void laser_draw_misshot(struct laser_t *l, uint8_t color) {
  arduboy.drawBitmap(l->x - 1, l->y, laser_misshot_img, 3, 3, color);
}

boolean laser_bunker_test(struct laser_t *l) {
  if ((l->y < BUNKER_BOTTOM) && (l->y > BUNKER_TOP) && (l->y > aliens.bottom)) {
    if (arduboy.getPixel(l->x, l->y) || (arduboy.getPixel(l->x, l->y + 1))) {
      return true;
    }
  }
  return false;
}

void laser_move(struct laser_t *l) {
  if (l->wait_ctr++ < LASER_WAIT)
    return;

  l->wait_ctr = 0;
  if (l->exists) {
    l->y -= LASER_V;
    if (l->y <= 0) {
      l->exists = false;
      l->show_misshot = IV_SHOW_HIT;
    }
    else {
      if (laser_bunker_test(l)) {
        l->exists = false;
        l->show_misshot = IV_SHOW_HIT;
      }
    }
  }
}

void laser_draw(struct laser_t *l, uint8_t color) {
  arduboy.drawPixel(l->x, l->y, color);
  arduboy.drawPixel(l->x, l->y + 1, color);
}

void laser_draw_mishit(struct laser_t *l, uint8_t color) {
  arduboy.drawBitmap(l->x-1, l->y, laser_misshot_img, 3, 3, color);
}

void laser_shoot(struct laser_t *l, uint16_t x) {
  if ((!l->exists) && (l->show_misshot == 0) && (aliens.show_hit == 0)) {
    if (arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON)) {
      l->exists = true;
      l->x = x + LASER_OFS;
      l->y = CANNON_TOP - 1;
      l->shot_ctr = (l->shot_ctr + 1) % LASER_CNTR_MOD;
    }
  }
}

void cannon_erase(struct cannon_t *c) {
  arduboy.fillRect(c->x, CANNON_TOP, IV_W, IV_H, BLACK);
}

void cannon_draw(struct cannon_t *c) {
  arduboy.drawBitmap(c->x, CANNON_TOP, cannon_img, IV_W, IV_H, WHITE);
}

void cannon_move(struct cannon_t *c) {
  if (arduboy.pressed(RIGHT_BUTTON)) {
    c->x += CANNON_V;
  }
  if (arduboy.pressed(LEFT_BUTTON)) {
    c->x -= CANNON_V;
  }
  c->x = constrain(c->x, SCRN_LEFT, SCRN_RIGHT - IV_W);
}

void cannon_hit(struct cannon_t *c) {
  c->show_hit = CANNON_SHOW_HIT;
}

boolean cannon_hit_test(struct cannon_t *c, uint16_t x, uint16_t y) {
  if ((y > CANNON_TOP) && (y < CANNON_TOP + IV_H) && (x >= c->x) && (x <= c->x + IV_W)) {
    return true;
  }
  else
    return false;
}

boolean cannon_process(struct cannon_t *c) {
  if (c->wait_ctr++ < CANNON_WAIT) {
    return false;
  }
  else {
    c->wait_ctr = 0;
    return true;
  }
}


void bomb_shot(struct bomb_t *b) {
  int8_t a_idx = -1;
  uint8_t y = aliens.bottom - IV_H;

  for (uint8_t i = 0; i < IV_ROW; i++) {
    a_idx = aliens_hit_test(&aliens, cannon.x + 2, y);
    if (a_idx > 0)
      break;
    else
      y -= IV_VSPACING + IV_H;
  }
  if (a_idx > 0) {
    b->exists = true;
    b->x = aliens.nxt_left + A_XOFS(A_COL(a_idx)) + IV_W / 2;
    b->y = aliens.nxt_top + A_YOFS(A_ROW(a_idx)) + IV_H * 2 + IV_VSPACING;
  }
}


#define UFO_TOP 0
#define UFO_BOTTOM 3
#define UFO_W 7
#define UFO_H 3
#define UFO_INTERVAL 25
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
  boolean exists;
  uint8_t show_hit; // show hit image while >0
  uint8_t img_idx;  // index for ufo_score_img
} ufo;

void ufo_init(struct ufo_t *u) {
  u->wait_ctr = 60 * UFO_INTERVAL;
  u->exists = false;
  u->show_hit = 0;
}

void ufo_appear(struct ufo_t *u) {
  u->exists = true;
  if (cannon.laser.shot_ctr & 0x1) {
    u->v = 1;
    u->x = SCRN_LEFT;
  }
  else {
    u->v = -1;
    u->x = SCRN_RIGHT - UFO_W;
  }
}

void ufo_countdown(struct ufo_t *u) {
  if ((aliens.alive > 7) && (u->wait_ctr-- == 0)) {
    ufo_appear(u);
  }
}

boolean ufo_process(struct ufo_t *u) {
  if (u->wait_ctr++ < UFO_WAIT) {
    return false;
  }
  else {
    u->wait_ctr = 0;
    return true;
  }
}

void ufo_draw(struct ufo_t *u) {
  if (u->exists)
    arduboy.drawBitmap(u->x, UFO_TOP, ufo_img, UFO_W, UFO_H, WHITE);
}

void ufo_erase(struct ufo_t *u) {
  arduboy.fillRect(u->x, UFO_TOP, UFO_W, UFO_H, BLACK);
}

void ufo_move(struct ufo_t *u) {
  u->x += u->v;
  if ((u->x < SCRN_LEFT) || (u->x > SCRN_RIGHT - UFO_W))
    ufo_init(u);
}

int16_t ufo_hit_test(struct ufo_t *u, uint16_t x, uint16_t y) {
  if (u->exists && (y < UFO_BOTTOM) && (x >= u->x) && (x < u->x + UFO_W)) {
    ufo_init(u);
    u->show_hit = UFO_SHOW_HIT;
    ufo_erase(u);

    int16_t sc;
    switch (cannon.laser.shot_ctr) {
    case 0:
    case 1:
    case 6:
    case 11:
      u->img_idx = 0;
      sc = 50;
      break;

    case 3:
    case 12:
      u->img_idx = 2;
      sc = 150;
      break;

    case 7: 
      u->img_idx = 3;
      sc = 300;
      break;

    default:
      u->img_idx = 1;
      sc = 100;
      break;
    }
  }
  else
    return -1;
}

enum GameStatus_t {
  GAME_LOST,
  GAME_ONGOING,
  GAME_RESTART,
  GAME_OVER
} gameStatus;

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

boolean game_restart() {
  arduboy.fillRect(SCRN_LEFT, 0, SCRN_RIGHT - SCRN_LEFT, SCRN_BOTTOM, BLACK);
  cannon_init(&cannon);
  aliens_init(&aliens);
  ufo_init(&ufo);
  draw_bunkers();
  bomb_init(&bombs[0], BOMB_A);

  return true;
}

void game_main() {

  if (cannon.show_hit > 1) {
    cannon_erase(&cannon);
    arduboy.drawBitmap(cannon.x, CANNON_TOP, cannon_hit_img[cannon.show_hit % 2], IV_W, IV_H, WHITE);
    cannon.show_hit--;
    return;
  }
  else if (cannon.show_hit == 1) {
    gameStatus = GAME_LOST;
    return;
  }

  if (cannon_process(&cannon)) {
    cannon_erase(&cannon);
    cannon_move(&cannon);
    laser_shoot(&cannon.laser, cannon.x);
    cannon_draw(&cannon);
  }

  if (cannon.laser.exists) {
    laser_draw(&cannon.laser, BLACK);
    laser_move(&cannon.laser);
    laser_draw(&cannon.laser, WHITE);
    int8_t hit = aliens_hit_test(&aliens, cannon.laser.x, cannon.laser.y);
    if (hit >= 0) {
      aliens_hit(&aliens, hit);
      laser_draw(&cannon.laser, BLACK);
      cannon.laser.exists = false;
    }
    if (ufo_hit_test(&ufo, cannon.laser.x, cannon.laser.y) >= 0) {
      laser_draw(&cannon.laser, BLACK);
      cannon.laser.exists = false;
    }
  }
  else if (cannon.laser.show_misshot > 0) {
    if (cannon.laser.show_misshot == IV_SHOW_HIT) {
      laser_draw(&cannon.laser, BLACK);
      laser_draw_misshot(&cannon.laser, WHITE);
    }
    else if (cannon.laser.show_misshot == 1)
      laser_draw_misshot(&cannon.laser, BLACK);
    cannon.laser.show_misshot--;
  }

  if (bombs[0].show_hit == BOMB_SHOW_HIT) {
    bomb_hitimg_draw(&bombs[0], WHITE);
    bombs[0].show_hit--;
  }
  else if (bombs[0].show_hit > 1) {
    bombs[0].show_hit--;
  }
  else if (bombs[0].show_hit == 1) {
    bomb_hitimg_draw(&bombs[0], BLACK);
    bombs[0].show_hit = 0;
  }
  else if (bombs[0].exists) {
    bomb_draw_c(&bombs[0], BLACK);
    bomb_move(&bombs[0]);
    bomb_draw_c(&bombs[0], WHITE);
    if (cannon_hit_test(&cannon, bombs[0].x + 1, bombs[0].y))
      cannon_hit(&cannon);
  }
  else {
    bomb_shot(&bombs[0]);
  }

  if (ufo.exists) {
    if (ufo_process(&ufo)) {
      ufo_erase(&ufo);
      ufo_move(&ufo);
      ufo_draw(&ufo);
    }
  }
  else if (ufo.show_hit > 0) {
    if (ufo.show_hit == UFO_SHOW_HIT)
      arduboy.drawBitmap(ufo.x, UFO_TOP, ufo_score_img[ufo.img_idx], UFO_W, UFO_H, WHITE);
    else if (ufo.show_hit == 1)
      ufo_erase(&ufo);
    ufo.show_hit--;
  }
  else {
    ufo_countdown(&ufo);
  }

  if (aliens.show_hit == IV_SHOW_HIT) {
    aliens_hitimg_erase_alien(&aliens);
    aliens_hitimg_draw(&aliens);
    aliens.show_hit--;
  }
  else if (aliens.show_hit > 1) {
    aliens.show_hit--;
  }
  else if (aliens.show_hit == 1) {
    aliens_hitimg_erase(&aliens);
    aliens.show_hit = 0;
  }
  else {
    aliens_erase(&aliens);
    aliens_draw(&aliens);
    aliens_move(&aliens);
  }
}

void setup() {
  arduboy.begin();
  arduboy.clear();
  arduboy.drawFastVLine(SCRN_LEFT - 1, 0, 64, WHITE);
  arduboy.drawFastVLine(SCRN_RIGHT, 0, 64, WHITE);
  draw7seg(109, 20, 0);
  draw7seg(113, 20, 1);
  draw7seg(117, 20, 2);
  draw7seg(121, 20, 3);
  draw7seg(125, 20, 4);
  draw7seg(109, 28, 5);
  draw7seg(113, 28, 6);
  draw7seg(117, 28, 7);
  draw7seg(121, 28, 8);
  draw7seg(125, 28, 9);
  arduboy.setFrameRate(60);
  gameStatus = GAME_OVER;
}

void loop() {
  if (!(arduboy.nextFrame()))
    return;

  switch (gameStatus) {
  case GAME_RESTART:
    if (game_restart()) { // true when initialization is done
      gameStatus = GAME_ONGOING;
    }
    break;

  case GAME_ONGOING:
    game_main();
    if ((aliens.alive == 0) && aliens.show_hit == 0) {
      gameStatus = GAME_RESTART;
    }
    if (aliens.touch_down) {
      gameStatus = GAME_OVER;
    }
    break;

  case GAME_LOST:
    cannon_erase(&cannon);
    cannon_init(&cannon);
    gameStatus = GAME_ONGOING;
    break;

  case GAME_OVER:
    if (game_new_game()) { // true if A or B button is pushed
      gameStatus = GAME_RESTART;
      //erase_game_over();
      //game_initialize();
    }
  }

  arduboy.display();
}
