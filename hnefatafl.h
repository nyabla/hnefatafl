#pragma once
#include <stdbool.h>

void init_board();
void draw_board();
void draw_info();
void draw_status();

bool can_capture(int x, int y);
void try_capture_neighbours(int x, int y);

void clear_piece(int x, int y);
void move_piece(int old_x, int old_y, int new_x, int new_y);

bool is_valid_move(int old_x, int old_y, int new_x, int new_y);
int* allowed_moves(int x, int y);
void show_allowed(int x, int y);

int initiate_move(int x, int y);
void check_king_safety();
