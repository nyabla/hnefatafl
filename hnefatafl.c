/* includes */
#include "hnefatafl.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>

/* defines */
#define BOARD_SIZE 11
#define ROW_DELTA 2
#define COLUMN_DELTA 4

#define FLAT_COORDS(x, y) (y) * BOARD_SIZE + (x)
#define IS_VALID_COORDS(x, y) ((x) >= 0 && (x) < BOARD_SIZE && (y) >= 0 && (y) < BOARD_SIZE)

#define SET_PIECE(x, y, piece) PIECE_AT((x), (y)) = (piece)
#define PIECE_AT(x, y) game_board[FLAT_COORDS((x), (y))]
#define TOGGLE_PIECE(x, y, piece) SET_PIECE((x), (y), PIECE_AT((x), (y)) ^ (piece))
#define IS_EMPTY(x, y) ((PIECE_AT((x), (y)) & PIECE_MASK) == 0)

/* globals */
enum TileDataFlags {
  EMPTY     = 0b0000,
  REFUGE    = 0b1000,
  KING      = 0b0110,
  DEFENDER  = 0b0010,
  ATTACKER  = 0b0001
};

enum TileDataMasks {
  COLOUR_MASK = 0b0011,
  PIECE_MASK  = 0b0111,
};

int game_board[BOARD_SIZE * BOARD_SIZE];
static WINDOW *board_window, *info_window, *status_window;
int turn;
// winner will be set in can_capture if a king is captured
// winner will be set in check_king_safety if king is in a refuge
int winner;

int main()
{
  bool run = true;
  turn = ATTACKER;
  winner = 0;

  // setup curses
  initscr();
  noecho();
  clear();
  keypad(stdscr, TRUE);

  // setup windows
  board_window = newwin(BOARD_SIZE * 2 + 1, BOARD_SIZE * 4 + 1, 1, 2);
  info_window = newwin(40, 40, 1, BOARD_SIZE * 4 + 6);
  status_window = newwin(2, 50, BOARD_SIZE * 2 + 3, 4);

  init_board();
  draw_board();

  draw_info();
  draw_status();

  refresh();
  wrefresh(board_window);
  wrefresh(info_window);
  wrefresh(status_window);

  int x, y;
  int key;

  x = 0; y = 0;

  // main loop
  while (run) {
    werase(status_window);
    draw_status();
    refresh();
    wrefresh(status_window);
    wmove(board_window, y * ROW_DELTA + 1, x * COLUMN_DELTA + 2);
    wrefresh(board_window);

    key = getch();
    switch (key) {
      case 'Q':
        run = false;
        break;
      case KEY_UP:
        if (y > 0)
          y--;
        break;
      case KEY_LEFT:
        if (x > 0)
          x--;
        break;
      case KEY_DOWN:
        if (y < BOARD_SIZE - 1)
          y++;
        break;
      case KEY_RIGHT:
        if (x < BOARD_SIZE - 1)
          x++;
        break;
      case ' ':
        // set cursor position to be consistent with where a piece was moved
        x = initiate_move(x, y);
        y = x / BOARD_SIZE;
        x = x % BOARD_SIZE;
        break;
      case KEY_RESIZE:
        redrawwin(board_window);
        redrawwin(status_window);
        redrawwin(info_window);
        draw_info();
        break;
    }
    draw_board();
    show_allowed(x, y);
    check_king_safety();
  }

  endwin();
  return 0;
}

void init_board()
{
  int max_coord = BOARD_SIZE - 1;
  // king
  SET_PIECE(5, 5, REFUGE);
  TOGGLE_PIECE(5, 5, KING);

  // defenders orthogonal to king
  for (int x = 3; x < 5; x++) {
    SET_PIECE(x, 5, DEFENDER);
    SET_PIECE(5, x, DEFENDER);
    SET_PIECE(max_coord - x, max_coord - 5, DEFENDER);
    SET_PIECE(max_coord - 5, max_coord - x, DEFENDER);
  }

  // defenders diagonal to king
  SET_PIECE(4, 4, DEFENDER);
  SET_PIECE(max_coord - 4, 4, DEFENDER);
  SET_PIECE(max_coord - 4, max_coord - 4, DEFENDER);
  SET_PIECE(4, max_coord - 4, DEFENDER);

  // attackers at edges
  for (int x = 3; x < 8; x++) {
    SET_PIECE(x, 0, ATTACKER);
    SET_PIECE(0, x, ATTACKER);
    SET_PIECE(x, max_coord, ATTACKER);
    SET_PIECE(max_coord, x, ATTACKER);
  }

  // other attackers
  SET_PIECE(5, 1, ATTACKER);
  SET_PIECE(1, 5, ATTACKER);
  SET_PIECE(max_coord - 5, max_coord - 1, ATTACKER);
  SET_PIECE(max_coord - 1, max_coord - 5, ATTACKER);

  // refuges
  SET_PIECE(0, 0, REFUGE);
  SET_PIECE(0, max_coord, REFUGE);
  SET_PIECE(max_coord, 0, REFUGE);
  SET_PIECE(max_coord, max_coord, REFUGE);
}

void draw_board()
{
  int total_width = BOARD_SIZE * 4;
  int total_height = BOARD_SIZE * 2;

  // corners
  mvwaddch(board_window, 0, 0, ACS_ULCORNER);
  mvwaddch(board_window, 0, total_width, ACS_URCORNER);
  mvwaddch(board_window, total_height, 0, ACS_LLCORNER);
  mvwaddch(board_window, total_height, total_width, ACS_LRCORNER);

  // top and bottom
  for (int dx = 1; dx < total_width; dx++) {
    if (dx % 4 == 0) {
      mvwaddch(board_window, 0, dx, ACS_TTEE);
      mvwaddch(board_window, total_height, dx, ACS_BTEE);
    } else {
      mvwaddch(board_window, 0, dx, ACS_HLINE);
      mvwaddch(board_window, total_height, dx, ACS_HLINE);
    }
  }

  // left and right
  for (int dy = 1; dy < total_height; dy++) {
    if (dy % 2 == 0) {
      mvwaddch(board_window, dy, 0, ACS_LTEE);
      mvwaddch(board_window, dy, total_width, ACS_RTEE);
    } else {
      mvwaddch(board_window, dy, 0, ACS_VLINE);
      mvwaddch(board_window, dy, total_width, ACS_VLINE);
    }
  }

  // inner grid
  int index = 0;
  for (int dy = 1; dy < total_height; dy++) {
    for (int dx = 1; dx < total_width; dx++) {
      if (dx % 4 == 0 && dy % 2 == 0) {
        mvwaddch(board_window, dy, dx, ACS_PLUS);
      } else if (dx % 4 == 0) {
        mvwaddch(board_window, dy, dx, ACS_VLINE);
      } else if (dy % 2 == 0) {
        mvwaddch(board_window, dy, dx, ACS_HLINE);
      } else if (dx % 2 == 0) {
        int piece = game_board[index];
        if ((piece & KING) == KING)
          mvwaddch(board_window, dy, dx, '#');
        else if ((piece & REFUGE) == REFUGE)
          mvwaddch(board_window, dy, dx, 'X');
        else if ((piece & DEFENDER) == DEFENDER)
          mvwaddch(board_window, dy, dx, 'O');
        else if ((piece & ATTACKER) == ATTACKER)
          mvwaddch(board_window, dy, dx, '@');
        else
          mvwaddch(board_window, dy, dx, ' ');
        index++;
      }
    }
  }
}

void draw_info()
{
  wprintw(info_window, "hnefatafl\n\n");
  wprintw(info_window, "Movement:\n");
  wprintw(info_window, " Use the arrow keys to move\n");
  wprintw(info_window, "Commands:\n");
  wprintw(info_window, " SPACE - Select and place piece\n");
  wprintw(info_window, "     Q - Quit\n");
  wprintw(info_window, "Key:\n");
  wprintw(info_window, " O - Defenders\n");
  wprintw(info_window, " # - King\n");
  wprintw(info_window, " @ - Attackers\n");
  wprintw(info_window, " X - Refuge\n ");
  waddch(info_window, ACS_CKBOARD);
  wprintw(info_window, " - Valid moves\n");
}

void draw_status()
{
  if (winner == ATTACKER)
    wprintw(status_window, "ATTACKER wins");
  else if (winner == DEFENDER)
    wprintw(status_window, "DEFENDER wins");
  else if (turn == ATTACKER)
    wprintw(status_window, "ATTACKER [@] to play");
  else
    wprintw(status_window, "DEFENDER [O]/[#] to play");
}

int opposite_piece(int piece)
{
  if ((piece & ATTACKER) == ATTACKER)
    return DEFENDER;
  if ((piece & DEFENDER) == DEFENDER)
    return ATTACKER;
  return EMPTY;
}

bool can_capture(int x, int y)
{
  // can't capture from invalid coordinates
  if (!IS_VALID_COORDS(x, y))
    return false;

  // can't capture nothing
  if (IS_EMPTY(x, y))
    return false;

  int up, down;
  // check up
  if (IS_VALID_COORDS(x, y - 1)) {
    // if the tile is empty and is a refuge a piece can be captured against it as if your own piece was there
    // (Rule 7)
    if (IS_EMPTY(x, y - 1) && (PIECE_AT(x, y - 1) & REFUGE) == REFUGE)
      up = opposite_piece(PIECE_AT(x, y));
    else
      up = PIECE_AT(x, y - 1);
  }

  // check down
  if (IS_VALID_COORDS(x, y + 1)) {
    // if the tile is empty and is a refuge a piece can be captured against it as if your own piece was there
    // (Rule 7)
    if (IS_EMPTY(x, y + 1) && (PIECE_AT(x, y + 1) & REFUGE) == REFUGE)
      down = opposite_piece(PIECE_AT(x, y));
    else
      down = PIECE_AT(x, y + 1);
  }

  // up and down are the same colour AND up isn't empty AND up is different colour than piece to capture
  bool vertical_capture = (up & COLOUR_MASK) == (down & COLOUR_MASK)
    && (up & PIECE_MASK) != 0 && (up & COLOUR_MASK) != (PIECE_AT(x, y) & COLOUR_MASK);

  int left, right;
  // check left
  if (IS_VALID_COORDS(x - 1, y)) {
    // if the tile is empty and is a refuge a piece can be captured against it as if your own piece was there
    // (Rule 7)
    if (IS_EMPTY(x - 1, y) && (PIECE_AT(x - 1, y) & REFUGE) == REFUGE)
      left = opposite_piece(PIECE_AT(x, y));
    else
      left = PIECE_AT(x - 1, y);
  }

  // check right
  if (IS_VALID_COORDS(x + 1, y)) {
    // if the tile is empty and is a refuge a piece can be captured against it as if your own piece was there
    // (Rule 7)
    if (IS_EMPTY(x + 1, y) && (PIECE_AT(x + 1, y) & REFUGE) == REFUGE)
      right = opposite_piece(PIECE_AT(x, y));
    else
      right = PIECE_AT(x + 1, y);
  }

  // left and right are the same colour AND left isn't empty AND left is different colour than piece to capture
  bool horizontal_capture = (left & COLOUR_MASK) == (right & COLOUR_MASK)
    && (left & PIECE_MASK) != 0 && (left & COLOUR_MASK) != (PIECE_AT(x, y) & COLOUR_MASK);

  // king must be surrounded from 4 sides to be captured
  if ((PIECE_AT(x, y) & KING) == KING) {
    // attacker wins on capture of king
    if (vertical_capture && horizontal_capture) {
      winner = ATTACKER;
      return true;
    } else {
      return false;
    }
  }

  // otherwise only 2 sides are required
  return vertical_capture || horizontal_capture;
}

void try_capture_neighbours(int x, int y)
{
  // check above
  if (can_capture(x, y - 1))
    clear_piece(x, y - 1);

  // check right
  if (can_capture(x + 1, y))
    clear_piece(x + 1, y);

  // check below
  if (can_capture(x, y + 1))
    clear_piece(x, y + 1);

  // check left
  if (can_capture(x - 1, y))
    clear_piece(x - 1, y);
}

void clear_piece(int x, int y)
{
  int piece = PIECE_AT(x, y) & PIECE_MASK;
  TOGGLE_PIECE(x, y, piece);
}

void move_piece(int old_x, int old_y, int new_x, int new_y)
{
  int piece = PIECE_AT(old_x, old_y) & PIECE_MASK;
  TOGGLE_PIECE(old_x, old_y, piece);
  TOGGLE_PIECE(new_x, new_y, piece);
}

bool is_valid_move(int old_x, int old_y, int new_x, int new_y)
{
  // if the cell is not empty its not a valid move
  if (!IS_EMPTY(new_x, new_y))
    return false;

  // if the piece is not moved orthogonally it is not a valid move
  if (old_x != new_x && old_y != new_y)
    return false;

  // if the piece is moved to a tile that is not a refuge then it is a valid
  // move
  if ((PIECE_AT(new_x, new_y) & REFUGE) != REFUGE)
    return true;

  // if the piece being moved to a refuge is a king then it is a valid move
  if ((PIECE_AT(old_x, old_y) & KING) == KING)
    return true;

  // otherwise it is not a valid move
  return false;
}

// up, right, down, left
int* allowed_moves(int x, int y)
{
  static int distances[4] = { 0, 0, 0, 0 };
  int dx, dy;

  if (IS_EMPTY(x, y) && false)
    return distances;

  // check distance up
  dy = 0;
  while (true) {
    if (!IS_VALID_COORDS(x, y - dy))
      break;

    if (!is_valid_move(x, y, x, y - dy) != (dy == 0))
      break;

    dy++;
  }

  distances[0] = dy;

  // check distance right
  dx = 0;
  while (true) {
    if (!IS_VALID_COORDS(x + dx, y))
      break;

    // break if the move is not valid XOR the distance is 0
    // a move to the same tile is invalid as it is taken
    if (!is_valid_move(x, y, x + dx, y) != (dx == 0))
      break;

    dx++;
  }

  distances[1] = dx;

  // check distance down
  dy = 0;
  while (true) {
    if (!IS_VALID_COORDS(x, y + dy))
      break;

    if (!is_valid_move(x, y, x, y + dy) != (dy == 0))
      break;

    dy++;
  }

  distances[2] = dy;

  // check distance right
  dx = 0;
  while (true) {
    if (!IS_VALID_COORDS(x - dx, y))
      break;

    if (!is_valid_move(x, y, x - dx, y) != (dx == 0))
      break;

    dx++;
  }

  distances[3] = dx;

  return distances;
}

void show_allowed(int x, int y)
{
  // do not show for opponent's pieces
  if ((PIECE_AT(x, y) & COLOUR_MASK) != turn)
    return;

  int* allowed = allowed_moves(x, y);

  if (allowed[0] != 0) {
    for (int dy = 1; dy < allowed[0]; dy++) {
      mvwaddch(board_window, (y - dy) * ROW_DELTA + 1, x * COLUMN_DELTA + 2, ACS_CKBOARD);
    }
  }

  if (allowed[1] != 0) {
    for (int dx = 1; dx < allowed[1]; dx++) {
      mvwaddch(board_window, y * ROW_DELTA + 1, (x + dx) * COLUMN_DELTA + 2, ACS_CKBOARD);
    }
  }

  if (allowed[2] != 0) {
    for (int dy = 1; dy < allowed[2]; dy++) {
      mvwaddch(board_window, (y + dy) * ROW_DELTA + 1, x * COLUMN_DELTA + 2, ACS_CKBOARD);
    }
  }

  if (allowed[3] != 0) {
    for (int dx = 1; dx < allowed[3]; dx++) {
      mvwaddch(board_window, y * ROW_DELTA + 1, (x - dx) * COLUMN_DELTA + 2, ACS_CKBOARD);
    }
  }
}

int initiate_move(int x, int y)
{
  // can't initiate move on a piece that isn't your own
  if ((PIECE_AT(x, y) & turn) != turn)
    return FLAT_COORDS(x, y);

  int* allowed = allowed_moves(x, y);
  int dx, dy;
  dx = 0; dy = 0;

  int key;

  // loop to choose where to place
  while (true) {
    refresh();
    wmove(board_window, (y + dy) * ROW_DELTA + 1, (x + dx) * COLUMN_DELTA + 2);
    wrefresh(board_window);

    key = getch();

    switch (key) {
      case ' ':
        if (dx == 0 && dy == 0)
          break;
        move_piece(x, y, x + dx, y + dy);
        try_capture_neighbours(x + dx, y + dy);
        turn = turn == ATTACKER ? DEFENDER : ATTACKER;
        return FLAT_COORDS(x + dx, y + dy);
      case KEY_UP:
        if (-dy < allowed[0] - 1) {
          dy--;
          dx = 0;
        }
        break;
      case KEY_RIGHT:
        if (dx < allowed[1] - 1) {
          dx++;
          dy = 0;
        }
        break;
      case KEY_DOWN:
        if (dy < allowed[2] - 1) {
          dy++;
          dx = 0;
        }
        break;
      case KEY_LEFT:
        if (-dx < allowed[3] - 1) {
          dx--;
          dy = 0;
        }
        break;
    }
  }
}

void check_king_safety()
{
  int max_coord = BOARD_SIZE - 1;

  // check all the corners to see if the king is there.
  bool win_condition = (PIECE_AT(0, 0) & KING) == KING
    || (PIECE_AT(0, max_coord) & KING) == KING
    || (PIECE_AT(max_coord, 0) & KING) == KING
    || (PIECE_AT(max_coord, max_coord) & KING) == KING;

  if (win_condition)
    winner = DEFENDER;
}
