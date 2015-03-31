#pragma once

#include "board.h"
#include <stdio.h>

bool scan_board(board_t *board, FILE *fp);
void print_board(solver_t const *solver, board_t const *board, uint bits);
