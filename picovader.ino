#include "Arduboy.h"

Arduboy arduboy;

#define MAXINT16 65535

#define IV_ROW 5
#define IV_COLUMN 11
#define IV_TOP 7
#define IV_W 5
#define IV_H 3
#define IV_HSPACING 2
#define IV_VSPACING 2
#define IV_VMOVE 2

#define SCRN_LEFT  20
#define SCRN_RIGHT 108
#define SCRN_BOTTOM 64

struct alien_t {
  uint16_t left;
  uint16_t right;
  boolean exists[IV_COLUMN];
};

struct aliens_t {
  struct alien_t rows[IV_ROW];
  uint16_t top = IV_TOP;
  uint16_t left;
  uint16_t bottom;
  uint16_t right;
  uint8_t row_move;
  boolean move_down;
  int8_t v;
  uint8_t wait_ctr;
  uint8_t wait_idx;
  uint8_t pose;
} aliens;


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

// iv_wait[0] = num of aliens, iv_wait[1] = wait in ticks
#define IV_WAIT_SIZE 10
const uint8_t iv_wait[2][IV_WAIT_SIZE] = {
  { 40, 34, 28, 22, 16, 12,  8,  4,  2,  1 },
  { 10, 9, 7, 6, 5, 4, 3, 2,  1,  0 }
};

void alien_init(struct alien_t *iv) {
  iv->left = SCRN_LEFT;
  iv->right = iv->left + IV_W * IV_COLUMN + IV_HSPACING * (IV_COLUMN - 1);
  for (uint8_t i = 0; i < IV_COLUMN; i++)
    iv->exists[i] = true;
}

void alien_mov(struct alien_t *iv, uint16_t v) {
  iv->left += v;
  iv->right += v;
}

void alien_drawRow(struct alien_t *iv, uint16_t top, uint8_t bh[]) {
  uint16_t x = iv->left;
  uint8_t i = 0;

  for (; (!iv->exists[i]) && i < IV_COLUMN; i++);
  for (; i < IV_COLUMN; i++)
    if (iv->exists[i]) {
      arduboy.drawBitmap(x, top, bh, IV_W, IV_H, WHITE);
      x += (IV_W + IV_HSPACING);
    }
}

void aliens_set_wait(struct aliens_t *a) {
  if (a->left == 0)
    return;

  for (; (a->left < iv_wait[0][a->wait_idx]); a->wait_idx++);
  a->wait_ctr = 0;
}

void aliens_init(struct aliens_t *a) {
  for (uint8_t i = 0; i < IV_ROW; i++)
    alien_init(&a->rows[i]);
  a->row_move = IV_ROW - 1;
  a->move_down = false;
  a->v = 1;
  a->top = IV_TOP;
  aliens_set_wait(a);
  a->pose = 0;
}

boolean aliens_move(struct aliens_t *a) {
  a->wait_ctr++;
  if (a->wait_ctr < iv_wait[1][a->wait_idx])
    return false;

  a->wait_ctr = 0;

  return true;
}

void aliens_erase(struct aliens_t *a) {
  struct alien_t *iv = &a->rows[a->row_move];
  uint16_t y = a->row_move * (IV_H + IV_VSPACING) + a->top;

  if (a->move_down)
    y -= IV_VMOVE;
  arduboy.fillRect(iv->left, y, iv->right - iv->left, IV_H, BLACK);
}

void aliens_draw(struct aliens_t *a) {
  uint16_t y = a->top + a->row_move * (IV_H + IV_VSPACING);

  uint8_t *img;
  switch (a->row_move) {
  case 0: img = (uint8_t *)iv1_img[a->pose]; break;
  case 1:
  case 2: img = (uint8_t *)iv2_img[a->pose]; break;
  case 3:
  case 4: img = (uint8_t *)iv3_img[a->pose]; break;
  }
  alien_drawRow(&a->rows[a->row_move], y, img);
}

void aliens_dec_row(struct aliens_t *a) {
  if (a->row_move-- == 0) {
    a->row_move = IV_ROW - 1;
    a->pose = 1 - a->pose;
  }
}

void aliens_update_boundsrect(struct aliens_t *a) {
  a->left = MAXINT16;
  a->right = 0;
  for (uint8_t i = 0; i < IV_ROW; i++) {
    a->left = min(a->left, a->rows[i].left);
    a->right = max(a->right, a->rows[i].right);

    boolean exist = false;
    for (uint8_t j = 0; j < IV_COLUMN; j++) {
      exist = exist || a->rows[i].exists[j];
    }
    if (exist)
      a->bottom = max(a->bottom, a->top + IV_H + a->row_move * (IV_H + IV_VSPACING));
  }

  if (a->row_move == 0) {
    if ((a->left + a->v < SCRN_LEFT) || (a->right + a->v > SCRN_RIGHT)) {
      a->move_down = true;
      a->top += IV_VMOVE;
      a->v = -a->v;
    }
    else {
      a->move_down = false;
    }
  }
}

#define LASER_V 2
#define LASER_WAIT 0
#define LASER_OFS 2

#define CANNON_V 1
#define CANNON_WAIT 1
#define CANNON_TOP 61

const uint8_t cannon_img[IV_W] PROGMEM =
{ 0x06, 0x06, 0x07, 0x06, 0x06 };

const uint8_t broken_img[2][IV_W] PROGMEM = {
  { 0x00, 0x03, 0x06, 0x03, 0x00 },
  { 0x00, 0x06, 0x03, 0x06, 0x00 }
};

struct laser_t {
  int16_t x;
  int16_t y;
  int8_t wait_ctr;
  boolean exists;
};

struct cannon_t {
  uint16_t x;
  uint8_t left;
  struct laser_t laser;
  uint8_t wait_ctr;
} cannon;

void cannon_init(struct cannon_t *c) {
  c->x = 32;
  c->laser.exists = false;
  c->wait_ctr = 0;
}

void laser_move(struct laser_t *l) {
  if (l->wait_ctr++ < LASER_WAIT)
    return;

  l->wait_ctr = 0;
  if (l->exists) {
    l->y -= LASER_V;
    if (l->y < 0)
      l->exists = false;
  }
}

void laser_draw(struct laser_t *l, uint8_t color) {
  arduboy.drawPixel(l->x, l->y, color);
  arduboy.drawPixel(l->x, l->y + 1, color);
}

void cannon_erase(struct cannon_t *c) {
  arduboy.fillRect(c->x, CANNON_TOP, IV_W, IV_H, BLACK);
}

void cannon_draw(struct cannon_t *c) {
  arduboy.drawBitmap(c->x, CANNON_TOP, cannon_img, IV_W, IV_H, WHITE);
}

void cannon_move(struct cannon_t *c) {
  if (c->wait_ctr++ < CANNON_WAIT)
    return;

  c->wait_ctr = 0;

  if (!c->laser.exists && (arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON))) {
    c->laser.exists = true;
    c->laser.x = c->x + LASER_OFS;
    c->laser.y = CANNON_TOP - 1;
  }

  if (arduboy.pressed(RIGHT_BUTTON)) {
    c->x += CANNON_V;
  }
  if (arduboy.pressed(LEFT_BUTTON)) {
    c->x -= CANNON_V;
  }
  c->x = constrain(c->x, SCRN_LEFT, SCRN_RIGHT - IV_W);
}

#define BUNKER_TOP 52
#define BUNKER_BOTTOM 53
#define BUNKER_W 8
#define BUNKER_H 5
#define BUNKER_LEFT 13
#define BUNKER_SPACE 10
#define BUNKER_NUM 4

const uint8_t bunker_img[BUNKER_W] PROGMEM =
{ 0x1e, 0x1f, 0x0f, 0x07, 0x07, 0x0f, 0x1f, 0x1e };

void draw_bunkers() {
  uint16_t x = BUNKER_LEFT + SCRN_LEFT;

  for (uint8_t i = 0; i < BUNKER_NUM; i++) {
    arduboy.drawBitmap(x, BUNKER_TOP, bunker_img, BUNKER_W, BUNKER_H, WHITE);
    x += BUNKER_W + BUNKER_SPACE;
  }
}


void setup() {
  arduboy.begin();
  arduboy.clear();
  arduboy.drawFastVLine(19, 0, 64, WHITE);
  arduboy.drawFastVLine(108, 0, 64, WHITE);
  arduboy.setFrameRate(60);

  cannon_init(&cannon);
  aliens_init(&aliens);
  draw_bunkers();
}

void loop() {
  if (!(arduboy.nextFrame()))
    return;

  if (aliens_move(&aliens)) {
    aliens_erase(&aliens);
    alien_mov(&aliens.rows[aliens.row_move], aliens.v);
    aliens_draw(&aliens);
    aliens_update_boundsrect(&aliens);
    aliens_dec_row(&aliens);
  }

  cannon_erase(&cannon);
  cannon_move(&cannon);
  cannon_draw(&cannon);

  if (cannon.laser.exists) {
    laser_draw(&cannon.laser, BLACK);
    laser_move(&cannon.laser);
    laser_draw(&cannon.laser, WHITE);
  }

  if (aliens.bottom >= SCRN_BOTTOM) {
    arduboy.fillRect(SCRN_LEFT, 0, SCRN_RIGHT - SCRN_LEFT, SCRN_BOTTOM, BLACK);
    cannon_init(&cannon);
    aliens_init(&aliens);
    draw_bunkers();
  }
  arduboy.display();
}
