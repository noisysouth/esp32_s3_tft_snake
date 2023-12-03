/**************************************************************************
  This is a library for several Adafruit displays based on ST77* drivers.

  Works with the Adafruit TFT Gizmo
    ----> http://www.adafruit.com/products/4367

  Check out the links above for our tutorials and wiring diagrams.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 **************************************************************************/

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <Wire.h> // for i2c keyboard

//#define DEBUG_BUTTONS
//#define DEBUG_MOVING
//#define DEBUG_SEGMENTS

// Use dedicated hardware SPI pins
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define PIXELS_X 240
#define PIXELS_Y 135

//#define CELL_PIXELS 5 // tiny
#define CELL_PIXELS 15 // bigger
//#define CELL_PIXELS 10
//#define CELL_PIXELS 24

#define CELLS_X (PIXELS_X/CELL_PIXELS)
#define CELLS_Y (PIXELS_Y/CELL_PIXELS)
//#define MAX_SEGMENTS 100
#define MAX_SEGMENTS 999

#define COS30 0.866 // cos (30 degrees)
#define SIN30 0.6   // sin (30 degrees)
#define COS60 0.6   // cos (60 degrees)
#define SIN60 0.866 // sin (60 degrees)

enum color_e {
  color_backgr = ST77XX_BLACK,
  
  color_intro1 = ST77XX_WHITE,
  color_intro2 = ST77XX_MAGENTA,
  color_intro3 = ST77XX_GREEN,

  color_worm = ST77XX_GREEN, // aka the snake
  color_pill = ST77XX_YELLOW,
  color_score = ST77XX_GREEN,
  color_count,
};

enum direction_e {
  dir_north = 0,
  dir_east,
  dir_south,
  dir_west,
  direction_count,
};

enum cell_img_e {
  mouth_north = dir_north,
  mouth_east = dir_east,
  mouth_south = dir_south,
  mouth_west = dir_west,
  body_north,
  body_east,
  body_south,
  body_west,
  tail_north,
  tail_east,
  tail_south,
  tail_west,
  img_pill,
  img_erase,
  cell_img_count,
};

float p = 3.1415926;

// game state
struct game_cell_s {
  int x, y;
  int img;
};
struct game_cell_s player_cell;
int player_dir; // -1 to turn left, +1 to turn right: so use int for this math, not enum
struct game_cell_s pill_cell; // pill the worm can eat go grow longer at this x,y

struct game_cell_s tail_cell;
// do the worm `-_/~
struct game_cell_s worm_cells[MAX_SEGMENTS];
int worm_cell_count;
//int worm_delay;

// when the counter reaches the target, we will do an event.
struct game_event_s {
  int counter;
  int target;
};

//struct game_event_s pill_event;
struct game_event_s walk_event;

// print ":(x, y) " coordinates of worm_cell
// Helper for print_worm()
void print_cell(struct game_cell_s *worm_cell) {
    Serial.print(":(");
    Serial.print(worm_cell->x);
    Serial.print(",");
    Serial.print(worm_cell->y);
    Serial.print(",");
    Serial.print(worm_cell->img);
    Serial.print(") ");
}

// Debug output of tail_cell, worm_cells[] x, y coordinates
void print_worm(void) {
#ifdef DEBUG_SEGMENTS
  int cell_idx;

  Serial.print("tail_cell");
  print_cell(&tail_cell);
  Serial.print(" ");
  Serial.print(worm_cell_count);
  Serial.print(" worm_cells: ");
  for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
    Serial.print(cell_idx);
    print_cell(&worm_cells[cell_idx]);
  }
  Serial.println();
#endif
}

void init_event(struct game_event_s *event_in, int init_target) {
  event_in->counter = 0;
  event_in->target = init_target;
}

bool incr_event(struct game_event_s *event_in) {
  event_in->counter++;
  if (event_in->counter >= event_in->target) {
      event_in->counter = 0;
      return true; // do the event. counter reached target.
  }
  return false; // have not reached target yet. don't do it, man.
}

// initialize player and world state to starting conditions
void start_game(void) {
  int cell_idx;
  //worm_cell_count = 99; // worm starts long
  //worm_cell_count = 15; // worm starts moderately long
  worm_cell_count = 4; // worm starts short
  //worm_cell_count = 1; // worm starts stubby

  // set where head of worm will be
  player_cell.x = CELLS_X/2;
  player_cell.y = CELLS_Y/2;
  player_cell.img = dir_east;

  // add worm's starting segments
  for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
    worm_cells[cell_idx].x = player_cell.x-cell_idx-1;
    worm_cells[cell_idx].y = player_cell.y;
    if (cell_idx == 0) {
      worm_cells[cell_idx].img = mouth_east;
    } else if (cell_idx == worm_cell_count-1) {
      worm_cells[cell_idx].img = tail_east;
    } else {
      worm_cells[cell_idx].img = body_east;
    }
  }
  // not trying to erase a tail right now
  tail_cell.x = -1;
  tail_cell.y = -1;
  // no pill on screen yet
  pill_cell.x = -1;
  pill_cell.y = -1;
//  pill_event.counter = 0;
//  pill_event.target = 100;
  //walk_event.counter = 0;
  //walk_event.target = 100;
  init_event (&walk_event, 3);
  print_worm();
}

void walk_player(void) {
  int cell_idx;
  static bool did_walk = false;
  bool         do_walk;

  do_walk = incr_event (&walk_event);
  if (!do_walk) {
    return;
  }

  switch (worm_cells[0].img) {
  case dir_east:
    //is_in_score
    player_cell.x++;
    break;
  case dir_west:
    player_cell.x--;
    break;
  case dir_north:
    player_cell.y--;
    break;
  case dir_south:
    player_cell.y++;
    break;
  default: // should not occur
    break;
  }

  // stay in cell bounds
  if (player_cell.x < 0) {
    player_cell.x = 0;
    do_walk = false; // no change to cells
  }
  if (player_cell.y < 0) {
    player_cell.y = 0;
    do_walk = false;
  }
  if (player_cell.x >= CELLS_X) {
    player_cell.x = CELLS_X-1;
    do_walk = false;
  }
  if (player_cell.y >= CELLS_Y) {
    player_cell.y = CELLS_Y-1;
    do_walk = false;
  }
  player_cell.img = worm_cells[0].img;

  if (did_walk && !do_walk) {
    //start_sound (sound_stop);
  }
  if (do_walk && !did_walk) {
    //start_sound (sound_go);
  }

  if (do_walk) {
    if (player_cell.x == pill_cell.x &&
        player_cell.y == pill_cell.y) {
      pill_cell.x   = -1;
      pill_cell.y   = -1;
      pill_cell.img = -1;
      //start_sound (sound_pill);
      if (worm_cell_count < MAX_SEGMENTS) {
        worm_cell_count++;
      }
    }

    tail_cell.x   = worm_cells[worm_cell_count-1].x;
    tail_cell.y   = worm_cells[worm_cell_count-1].y;
    tail_cell.img = img_erase;
    for (cell_idx = worm_cell_count-1; cell_idx >= 1; cell_idx--) {
      worm_cells[cell_idx].x   = worm_cells[cell_idx-1].x;
      worm_cells[cell_idx].y   = worm_cells[cell_idx-1].y;
      if (cell_idx == 1) {
        worm_cells[cell_idx].img = worm_cells[cell_idx-1].img + direction_count; // change FROM: mouth TO: body
      } else if (cell_idx == worm_cell_count-1) {
        worm_cells[cell_idx].img = worm_cells[cell_idx-1].img + direction_count; // change FROM: body TO: tail
      } else {
        worm_cells[cell_idx].img = worm_cells[cell_idx-1].img;
      }
    }
    worm_cells[0].x   = player_cell.x;
    worm_cells[0].y   = player_cell.y;
    worm_cells[0].img = player_cell.img;
  } else { // don't try to erase tail anymore: not moving.
    tail_cell.x   = -1;
    tail_cell.y   = -1;
    tail_cell.img = -1;
  }
  print_worm();

  did_walk = do_walk;
}

#define SCORE_WIDTH  70
#define SCORE_HEIGHT 28
bool is_in_score(struct game_cell_s *test_cell) {
  int x, y;

  x = (test_cell->x+1) * CELL_PIXELS;
  y = (test_cell->y  ) * CELL_PIXELS;
  if (x <= SCORE_WIDTH &&
      y <= SCORE_HEIGHT) {
        return true;
  }
  return false;
}

void draw_cell(struct game_cell_s *the_cell) {
  struct game_cell_s center; // center of the cell, in pixel x, y
  // points for mouth cells, in pixels
  struct game_cell_s upper_lip;
  struct game_cell_s lower_lip;
  // points for body cells, in pixels
  struct game_cell_s upper_left;
  struct game_cell_s lower_right;
  struct game_cell_s body1;
  struct game_cell_s body2;
  int radius;

  if (the_cell->x < 0 || the_cell->x >= CELLS_X ||
      the_cell->y < 0 || the_cell->y >= CELLS_Y ||
      is_in_score (the_cell)) {
      return; // off-screen location, don't draw
  }

  radius = CELL_PIXELS/2;
  center.x = the_cell->x * CELL_PIXELS + radius;
  center.y = the_cell->y * CELL_PIXELS + radius;
  upper_left.x = the_cell->x * CELL_PIXELS;
  upper_left.y = the_cell->y * CELL_PIXELS;
  lower_right.x = ((the_cell->x+1) * CELL_PIXELS)-1;
  lower_right.y = ((the_cell->y+1) * CELL_PIXELS)-1;

  switch (the_cell->img) {
  case img_pill:
    tft.fillCircle(center.x, center.y, radius, color_pill);
    break;
  case mouth_east:
    upper_lip.x = center.x + radius;
    upper_lip.y = center.y - radius;
    lower_lip.x = center.x + radius;
    lower_lip.y = center.y + radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case mouth_west:
    upper_lip.x = center.x - radius;
    upper_lip.y = center.y - radius;
    lower_lip.x = center.x - radius;
    lower_lip.y = center.y + radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case mouth_north:
    upper_lip.x = center.x + radius;
    upper_lip.y = center.y - radius;
    lower_lip.x = center.x - radius;
    lower_lip.y = center.y - radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case mouth_south:
    upper_lip.x = center.x + radius;
    upper_lip.y = center.y + radius;
    lower_lip.x = center.x - radius;
    lower_lip.y = center.y + radius;
    tft.fillCircle(center.x, center.y, radius, color_worm);
    tft.fillTriangle(center.x, center.y, upper_lip.x, upper_lip.y,
                                        lower_lip.x, lower_lip.y, color_backgr);
    break;
  case body_east:
    body1.x = lower_right.x - 3*radius/5;
    body1.y = center.y-CELL_PIXELS/5;
    body2.x = upper_left.x + 2*radius/5;
    body2.y = center.y+CELL_PIXELS/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case body_west:
    body1.x = upper_left.x + 3*radius/5;
    body1.y = center.y-CELL_PIXELS/5;
    body2.x = lower_right.x - 2*radius/5;
    body2.y = center.y+CELL_PIXELS/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case body_north:
    body1.x = center.x - CELL_PIXELS/5;
    body1.y = upper_left.y + 3*radius/5;
    body2.x = center.x + CELL_PIXELS/5;
    body2.y = lower_right.y - 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case body_south:
    body1.x = center.x - CELL_PIXELS/5;
    body1.y = lower_right.y - 3*radius/5;
    body2.x = center.x + CELL_PIXELS/5;
    body2.y = upper_left.y + 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_east:
    body1.x = lower_right.x - 3*radius/5;
    body1.y = center.y+1;
    body2.x = upper_left.x + 2*radius/5;
    body2.y = center.y-1;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_west:
    body1.x = upper_left.x + 3*radius/5;
    body1.y = center.y+1;
    body2.x = lower_right.x - 2*radius/5;
    body2.y = center.y-1;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_north:
    body1.x = center.x-1;
    body1.y = upper_left.y + 3*radius/5;
    body2.x = center.x+1;
    body2.y = lower_right.y - 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  case tail_south:
    body1.x = center.x-1;
    body1.y = lower_right.y - 3*radius/5;
    body2.x = center.x+1;
    body2.y = upper_left.y + 2*radius/5;
    tft.fillCircle(body1.x, body1.y, 3*radius/5, color_worm);
    tft.fillCircle(body2.x, body2.y, 2*radius/5, color_worm);
    break;
  default:
    break;
  }
}

void erase_cell(struct game_cell_s *the_cell) {
  int x, y;

  if (the_cell->x < 0 || the_cell->x >= CELLS_X ||
      the_cell->y < 0 || the_cell->y >= CELLS_Y ||
      is_in_score (the_cell)) {
      return; // off-screen location, don't draw
  }
  x = the_cell->x * CELL_PIXELS;
  y = the_cell->y * CELL_PIXELS;
  tft.fillRect(x, y, CELL_PIXELS+1, CELL_PIXELS+1, color_backgr);
}

void draw_game(void) {
  // draw score
  static int score_old = -1;
  static game_cell_s head_old = { -1, -1, -1 };
  static game_cell_s pill_old = { -1, -1, -1 };

  if (score_old != worm_cell_count) { // score changed; redraw it
    // background
    tft.fillRect(0, 0, SCORE_WIDTH, SCORE_HEIGHT, color_backgr);
    // number
    our_drawnum (worm_cell_count, color_score);

    score_old = worm_cell_count; // save the new score
  }

  if (head_old.x   != worm_cells[0].x ||
      head_old.y   != worm_cells[0].y ||
      head_old.img != worm_cells[0].img) {
      // worm head
      draw_cell (&worm_cells[0]);
      // worm body behind head
      if (worm_cell_count > 1) {
        erase_cell (&worm_cells[1]); // hide old head image
        draw_cell (&worm_cells[1]);
      }
      if (worm_cell_count > 2) {
        erase_cell (&worm_cells[worm_cell_count-1]); // end of worm
        draw_cell (&worm_cells[worm_cell_count-1]);
      }
      // erase old worm tail
      erase_cell (&tail_cell);

      head_old.x   = worm_cells[0].x;
      head_old.y   = worm_cells[0].y;
      head_old.img = worm_cells[0].img;
  }

  // pill!
  if (pill_old.x   != pill_cell.x ||
      pill_old.y   != pill_cell.y) {
    draw_cell (&pill_cell);
    pill_old.x = pill_cell.x;
    pill_old.y = pill_cell.y;
  }
}

void draw_banner (uint16_t color) {
  our_drawtext("Welcome to", 0, color);
  our_drawtext("Wormy!", 1, color);
}

void setup(void) {
  Serial.begin(9600);
  Serial.print(F("Hello! Welcome to wormy serial port!"));

  // turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  //pinMode(TFT_BACKLIGHT, OUTPUT);
  //digitalWrite(TFT_BACKLIGHT, HIGH); // Backlight on

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  tft.init(PIXELS_Y, PIXELS_X);                // Init ST7789 ST7789 240x135
  tft.setRotation(3);//tft.setRotation(2);
  tft.fillScreen(color_backgr);

  Serial.println(F("Initialized"));

  uint16_t time = millis();
  tft.fillScreen(color_backgr);
  time = millis() - time;

  Wire.begin();        // join i2c bus (address optional for master)

  Serial.println(time, DEC);
  draw_banner (color_intro1);
  //start_sound (sound_intro1);

  draw_banner (color_intro2);
  //start_sound (sound_intro2);

  draw_banner (color_intro3);
  //start_sound (sound_intro3);
  delay(2000);
  tft.fillScreen(color_backgr);
  start_game();
  draw_game();
}

#define KB_ADDR 0x5f
#define MIN_PRINTABLE ' '
#define MAX_PRINTABLE '~'

void loop() {
  float x_move;
  float y_move;

  Wire.requestFrom(KB_ADDR, 1);    // request 1 byte from peripheral device KB_ADDR

  x_move = 0;
  y_move = 0;

  if (Wire.available()) { // peripheral may send less than requested
    char kb_in = Wire.read(); // receive a byte as character
    switch (kb_in) {
    case 0x00: // nothing pressed. do nothing.
      break;
    case 0xd: // Enter
      kb_in = '\n';
      break;
    case 0x1b: // Escape
      break;
    case 0x8: // Backspace
      break;
    case 0x9: // Tab
      break;
    case 0xb4: // <- Left
      x_move = -1;
      break;
    case 0xb5: // ^ Up
      y_move = -1;
      break;
    case 0xb6: // v Down
      y_move = 1;
      break;
    case 0xb7: // Right ->
      x_move = 1;
      break;
    default:
      if (kb_in >= MIN_PRINTABLE && kb_in <= MAX_PRINTABLE) {
        // A-Z, 1-9, punctuation, space
      } else {
        // Someone probably pressed "Function" + key, got 0x80 thru 0xAF
        // See https://docs.m5stack.com/en/unit/cardkb_1.1
      }
    }
    //Serial.print(kb_in);         // print the character
  }

#ifdef DEBUG_BUTTONS
  Serial.print("Left Button: ");
  if (left_btn) {
    Serial.print("DOWN");
  } else {
    Serial.print("  UP");
  }
  Serial.print("   Right Button: ");
  if (right_btn) {
    Serial.print("DOWN");
  } else {
    Serial.print("  UP");    
  }
  Serial.println();
#endif

#if 0
  if (left_btn && !left_old) {
    worm_cells[0].img = worm_cells[0].img-1;
    if (worm_cells[0].img < 0) {
      worm_cells[0].img = direction_count-1; // kept turning left from first direction; now looking in last direction.
    }
  }
  if (right_btn && !right_old) {
    worm_cells[0].img = worm_cells[0].img+1;
    if (worm_cells[0].img >= direction_count) {
      worm_cells[0].img = 0; // kept turning right from last direction, now looking back in first direction again.
    }
  }
#endif

  // face the direction that was tilted the fastest,
  //  or if none tilted fast, the way we are most tilted.
  if (x_move > 0) {
    worm_cells[0].img = dir_east;
  }
  if (x_move < 0) {
    worm_cells[0].img = dir_west;
  }
  if (y_move > 0) {
    worm_cells[0].img = dir_south;
  }
  if (y_move < 0) {
    worm_cells[0].img = dir_north;
  }

#ifdef DEBUG_MOVING
  Serial.print("player_cell.x: ");
  Serial.print(player_cell.x);
  Serial.print(", player_cell.y: ");
  Serial.println(player_cell.y);
#endif
  //right_old = right_btn;
  //left_old  =  left_btn;

  walk_player();

  if (pill_cell.x == -1 &&
      pill_cell.y == -1) {
    //pill_event.counter++;
    //if (pill_event.counter >= pill_event.target) {
      bool pill_ok;
      int cell_idx;

      //pill_event.counter = 0;
      // place a new pill, but not on the worm.
      do {
        pill_cell.x   = random(0, CELLS_X);
        pill_cell.y   = random(0, CELLS_Y);
        pill_cell.img = img_pill;
        pill_ok = true;
        if (is_in_score(&pill_cell)) {
          pill_ok = false;
        }
        for (cell_idx = 0; cell_idx < worm_cell_count; cell_idx++) {
          if (worm_cells[cell_idx].x == pill_cell.x &&
              worm_cells[cell_idx].y == pill_cell.y) {
            pill_ok = false;
            break;
          }
        }
      } while (!pill_ok);
    //}
  } // end if pill_cell.x == -1, .y == -1
  draw_game();

  delay(100);
}

#define LINE_PIXELS 20
void our_drawtext(const char *text, int line, uint16_t color) {
  //int char_count;

  //char_count = strlen(text);
  tft.setTextSize(3);
  tft.setCursor(10, 10 + line*24);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

void our_drawnum(int num, uint16_t color) {
  tft.setTextSize(4);
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(num);
}