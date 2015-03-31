#include "board.h"
#include "io.h"
#include <stdlib.h>
#include <memory.h>

void init_genrand(unsigned long s);
unsigned long genrand_int32(void);

static inline
uint min(uint a, uint b)
{
	return (a < b) ? a : b;
}

static inline
uint max(uint a, uint b)
{
	return (a > b) ? a : b;
}

void reset_to_boundary(board_t *board)
{
	uint const width = board->width;
	uint const height = board->height;
	uint *const edge_h = board->edge_h;
	uint *const edge_v = board->edge_v;
	for (uint i = 0; i < width*(height + 1); ++i) {
		edge_h[i] = (edge_h[i] & EDGE_BOUNDARY) ? (EDGE_BOUNDARY | EDGE_BARRIER) : 0;
	}
	for (uint i = 0; i < (width + 1)*height; ++i) {
		edge_v[i] = (edge_v[i] & EDGE_BOUNDARY) ? (EDGE_BOUNDARY | EDGE_BARRIER) : 0;
	}
}

void copy_board(board_t *dst, board_t const *src)
{
	uint const width = src->width;
	uint const height = src->height;

	dst->width = width;
	dst->height = height;
	dst->edge_h = (uint *)malloc(width*(height + 1)*sizeof(uint));
	dst->edge_v = (uint *)malloc((width + 1)*height*sizeof(uint));

	memcpy(dst->edge_h, src->edge_h, width*(height + 1)*sizeof(uint));
	memcpy(dst->edge_v, src->edge_v, (width + 1)*height*sizeof(uint));
}

void swap_board(board_t *a, board_t *b)
{
	board_t tmp;
	tmp.width = a->width;
	tmp.height = a->height;
	tmp.edge_h = a->edge_h;
	tmp.edge_v = a->edge_v;

	a->width = b->width;
	a->height = b->height;
	a->edge_h = b->edge_h;
	a->edge_v = b->edge_v;

	b->width = tmp.width;
	b->height = tmp.height;
	b->edge_h = tmp.edge_h;
	b->edge_v = tmp.edge_v;
}

void free_board(board_t *board)
{
	free(board->edge_h);
	free(board->edge_v);
	memset(board, 0, sizeof(board_t));
}

void copy_edges_to_solver(solver_t const *solver, board_t const *board)
{
	uint const width = board->width;
	uint const height = board->height;

	memcpy(solver->edge_h_old, board->edge_h, width*(height + 1)*sizeof(uint));
	memcpy(solver->edge_v_old, board->edge_v, (width + 1)*height*sizeof(uint));
}

void init_solver(solver_t *solver, board_t const *board)
{
	uint const width = board->width;
	uint const height = board->height;

	memset(solver, 0, sizeof(solver_t));
	solver->edge_h_old = (uint *)malloc(width*(height + 1)*sizeof(uint));
	solver->edge_v_old = (uint *)malloc((width + 1)*height*sizeof(uint));
	solver->tmp1 = (uint *)malloc((width + 1)*(height + 1)*sizeof(uint));
	solver->tmp2 = (uint *)malloc((width + 1)*(height + 1)*sizeof(uint));
}

bool check_single_cells(solver_t const *solver, board_t const *board)
{
	uint const width = board->width;
	uint const height = board->height;
	uint *const edge_h = board->edge_h;
	uint *const edge_v = board->edge_v;
	uint *const cells = solver->tmp1;

	memset(cells, 0, width*height*sizeof(uint));

	bool changed = false;
	for (uint y = 0; y < height; ++y)
	for (uint x = 0; x < width; ++x) {
		// check the number of useable edges
		uint *edges[4];
		edges[0] = edge_h + y*width + x;
		edges[1] = edges[0] + width;
		edges[2] = edge_v + y*(width + 1) + x;
		edges[3] = edges[2] + 1;

		uint available_mask = 0;
		uint path_mask = 0;
		for (uint i = 0; i < 4; ++i) {
			uint const e = *edges[i];
			if ((e & EDGE_BARRIER) == 0) {
				available_mask |= (1U << i);
			}
			if (e & EDGE_PATH) {
				path_mask |= (1U << i);
			}
		}

		uint const available_count = __builtin_popcount(available_mask);
		uint const path_count = __builtin_popcount(path_mask);

		if (available_count == 2 && path_count < 2) {
			for (uint i = 0; i < 4; ++i) {
				if (available_mask & (1U << i)) {
					*edges[i] |= EDGE_PATH;
				} else {
					*edges[i] |= EDGE_BARRIER;
				}
			}
			cells[y*width + x] = 1;
			changed = true;
		} else if (path_count == 2 && available_count > 2) {
			for (uint i = 0; i < 4; ++i) {
				if ((path_mask & (1U << i)) == 0) {
					*edges[i] |= EDGE_BARRIER;
				}
			}
			cells[y*width + x] = 1;
			changed = true;
		}
	}

	if (changed && solver->verbose) {
		fputs("\nsingle cells:\n", stdout);
		print_board(solver, board, EDGE_ALL | EDGE_HIGHLIGHT | EDGE_NEW);
	}

	return changed;
}

static inline
uint parity(uint x, uint y)
{
	return (x ^ y) & 1;
}

bool parity_check_block_island(solver_t const *solver, board_t const *board, uint x0, uint y0, uint x1, uint y1, uint island_index)
{
	uint const width = board->width;
	uint const height = board->height;
	uint *const edge_h = board->edge_h;
	uint *const edge_v = board->edge_v;
	uint const *const cells = solver->tmp2;

	uint const w = x1 - x0;
	uint const h = y1 - y0;

	// count island cells
	uint cell_count[2] = { 0, 0 };
	for (uint y = 0; y < h; ++y)
	for (uint x = 0; x < w; ++x) {
		if (cells[y*w + x] == island_index) {
			uint const p = parity(x0 + x, y0 + y);
			++cell_count[p];
		}
	}

	// count available edges
	uint available_count[2] = { 0, 0 };
	uint path_count[2] = { 0, 0 };
	for (uint i = 0; i < w; ++i) {
		if (cells[i] == island_index) {
			uint const k = y0*width + x0 + i;
			uint const p = parity(x0 + i, y0);
			if ((edge_h[k] & EDGE_BARRIER) == 0) {
				++available_count[p];
			}
			if (edge_h[k] & EDGE_PATH) {
				++path_count[p];
			}
		}
		if (cells[(h - 1)*w + i] == island_index) {
			uint const k = y1*width + x0 + i;
			uint const p = parity(x0 + i, y1 - 1);
			if ((edge_h[k] & EDGE_BARRIER) == 0) {
				++available_count[p];
			}
			if (edge_h[k] & EDGE_PATH) {
				++path_count[p];
			}
		}
	}
	for (uint i = 0; i < h; ++i) {
		if (cells[i*w] == island_index) {
			uint const k = (y0 + i)*(width + 1) + x0;
			uint const p = parity(x0, y0 + i);
			if ((edge_v[k] & EDGE_BARRIER) == 0) {
				++available_count[p];
			}
			if (edge_v[k] & EDGE_PATH) {
				++path_count[p];
			}
		}
		if (cells[i*w + (w - 1)] == island_index) {
			uint const k = (y0 + i)*(width + 1) + x1;
			uint const p = parity(x1 - 1, y0 + i);
			if ((edge_v[k] & EDGE_BARRIER) == 0) {
				++available_count[p];
			}
			if (edge_v[k] & EDGE_PATH) {
				++path_count[p];
			}
		}
	}

	uint const min_cell_count = min(cell_count[0], cell_count[1]);
	uint const extra_cells[2] = {
		cell_count[0] - min_cell_count,
		cell_count[1] - min_cell_count
	};
	if (available_count[0] < 2*extra_cells[0] || available_count[1] < 2*extra_cells[1]) {
		fprintf(stderr, "odd parity block went wrong\n");
		exit(-1);
	}

	uint const min_count[2] = {
		1 + extra_cells[0] - extra_cells[1],
		1 + extra_cells[1] - extra_cells[0],
	};

	uint max_count[2] = {
		min(available_count[0], available_count[1] + 2*(extra_cells[0] - extra_cells[1])),
		min(available_count[1], available_count[0] + 2*(extra_cells[1] - extra_cells[0]))
	};
	if (cell_count[0] + cell_count[1] == width*height) {
		max_count[0] = min(max_count[0], min_count[0]);
		max_count[1] = min(max_count[1], min_count[1]);
	}

	bool const only_path_min_available[2] = {
		available_count[0] == min_count[0] && path_count[0] < min_count[0],
		available_count[1] == min_count[1] && path_count[1] < min_count[1]
	};
	bool const other_parity_used_path_max[2] = {
		path_count[1] == max_count[1] && available_count[0] == max_count[0] && path_count[0] < max_count[0],
		path_count[0] == max_count[0] && available_count[1] == max_count[1] && path_count[1] < max_count[1]
	};

	bool const make_path[2] = {
		only_path_min_available[0] || other_parity_used_path_max[0],
		only_path_min_available[1] || other_parity_used_path_max[1],
	};
	bool const make_barrier[2] = {
		path_count[0] == max_count[0] && available_count[0] > max_count[0],
		path_count[1] == max_count[1] && available_count[1] > max_count[1]
	};

	if (!(make_path[0] || make_path[1] || make_barrier[0] || make_barrier[1])) {
		return false;
	}

	for (uint i = 0; i < w; ++i) {
		if (cells[i] == island_index) {
			uint const k = y0*width + x0 + i;
			uint const p = parity(x0 + i, y0);
			if (make_path[p] && (edge_h[k] & EDGE_BARRIER) == 0) {
				edge_h[k] |= EDGE_PATH;
			} else if (make_barrier[p] && (edge_h[k] & EDGE_PATH) == 0) {
				edge_h[k] |= EDGE_BARRIER;
			}
		}
		if (cells[(h - 1)*w + i] == island_index) {
			uint const k = y1*width + x0 + i;
			uint const p = parity(x0 + i, y1 - 1);
			if (make_path[p] && (edge_h[k] & EDGE_BARRIER) == 0) {
				edge_h[k] |= EDGE_PATH;
			} else if (make_barrier[p] && (edge_h[k] & EDGE_PATH) == 0) {
				edge_h[k] |= EDGE_BARRIER;
			}
		}
	}
	for (uint i = 0; i < h; ++i) {
		if (cells[i*w] == island_index) {
			uint const k = (y0 + i)*(width + 1) + x0;
			uint const p = parity(x0, y0 + i);
			if (make_path[p] && (edge_v[k] & EDGE_BARRIER) == 0) {
				edge_v[k] |= EDGE_PATH;
			} else if (make_barrier[p] && (edge_v[k] & EDGE_PATH) == 0) {
				edge_v[k] |= EDGE_BARRIER;
			}
		}
		if (cells[i*w + w - 1] == island_index) {
			uint const k = (y0 + i)*(width + 1) + x1;
			uint const p = parity(x1 - 1, y0 + i);
			if (make_path[p] && (edge_v[k] & EDGE_BARRIER) == 0) {
				edge_v[k] |= EDGE_PATH;
			} else if (make_barrier[p] && (edge_v[k] & EDGE_PATH) == 0) {
				edge_v[k] |= EDGE_BARRIER;
			}
		}
	}

	if (solver->verbose) {
		uint *const highlights = solver->tmp1;
		memset(highlights, 0, width*height*sizeof(uint));
		for (uint y = 0; y < h; ++y)
		for (uint x = 0; x < w; ++x) {
			if (cells[y*w + x] == island_index) {
				highlights[(y0 + y)*width + (x0 + x)] = 1;
			}
		}
		fputs("\nparity check:\n", stdout);
		print_board(solver, board, EDGE_ALL | EDGE_HIGHLIGHT | EDGE_NEW);
	}

	return true;
}

bool parity_check_block(solver_t const *solver, board_t const *board, uint x0, uint y0, uint x1, uint y1)
{
	uint const width = board->width;
	uint *const edge_h = board->edge_h;
	uint *const edge_v = board->edge_v;
	uint *const coords = solver->tmp1;
	uint *const cells = solver->tmp2;

	uint const w = x1 - x0;
	uint const h = y1 - y0;
	memset(cells, 0, w*h*sizeof(uint));

	// colour all islands
	uint next_island_index = 1;
	for (uint sy = 0; sy < h; ++sy)
	for (uint sx = 0; sx < w; ++sx) {
		if (cells[sy*w + sx] != 0) {
			continue;
		}

		cells[sy*w + sx] = next_island_index;
		coords[0] = (sy << 16) | sx;
		uint start = 0;
		uint end = 1;
		while (start != end) {
			uint const x = coords[start] & 0xffffU;
			uint const y = coords[start] >> 16;
			uint const ic = y*w + x;
			uint const ih = (y0 + y)*width + (x0 + x);
			uint const iv = (y0 + y)*(width + 1) + (x0 + x);

			if (x > 0 && cells[ic - 1] == 0 && (edge_v[iv] & EDGE_BARRIER) == 0) {
				cells[ic - 1] = next_island_index;
				coords[end++] = (y << 16) | (x - 1);
			}
			if (x < w - 1 && cells[ic + 1] == 0 && (edge_v[iv + 1] & EDGE_BARRIER) == 0) {
				cells[ic + 1] = next_island_index;
				coords[end++] = (y << 16) | (x + 1);
			}
			if (y > 0 && cells[ic - w] == 0 && (edge_h[ih] & EDGE_BARRIER) == 0) {
				cells[ic - w] = next_island_index;
				coords[end++] = ((y - 1) << 16) | x;
			}
			if (y < h - 1 && cells[ic + w] == 0 && (edge_h[ih + width] & EDGE_BARRIER) == 0) {
				cells[ic + w] = next_island_index;
				coords[end++] = ((y + 1) << 16) | x;
			}

			++start;
		}
		++next_island_index;
	}

	// solve each one
	for (uint i = 1; i < next_island_index; ++i) {
		if (parity_check_block_island(solver, board, x0, y0, x1, y1, i)) {
			return true;
		}
	}
	return false;
}

bool parity_check_all_blocks(solver_t const *solver, board_t const *board, uint w, uint h)
{
	uint const xn = board->width - w;
	uint const yn = board->height - h;
	for (uint y = 0; y <= yn; ++y)
	for (uint x = 0; x <= xn; ++x) {
		if (parity_check_block(solver, board, x, y, x + w, y + h)) {
			return true;
		}
	}
	return false;
}

bool parity_check_all_block_sizes(solver_t const *solver, board_t const *board)
{
	uint const width = board->width;
	uint const height = board->height;
	for (uint h = 2; h <= height; ++h)
	for (uint w = 2; w <= width; ++w) {
		if (parity_check_all_blocks(solver, board, w, h)) {
			return true;
		}
	}
	return false;
}

bool check_loops(solver_t const *solver, board_t const *board, bool *is_solved)
{
	uint const width = board->width;
	uint const height = board->height;
	uint *const edge_h = board->edge_h;
	uint *const edge_v = board->edge_v;
	uint *const coords = solver->tmp1;
	uint *const cells = solver->tmp2;
	uint *const highlights = coords;

	memset(cells, 0, width*height*sizeof(uint));

	// colour all paths
	uint exit_path_count = 0;
	uint exit_path_indices[2] = { 0, 0 };
	uint next_path_index = 1;
	for (uint sy = 0; sy < height; ++sy)
	for (uint sx = 0; sx < width; ++sx) {
		if (cells[sy*width + sx] != 0) {
			continue;
		}

		uint all_path_bits = 0;
		all_path_bits |= edge_h[sy*width + sx];
		all_path_bits |= edge_h[(sy + 1)*width + sx];
		all_path_bits |= edge_v[sy*(width + 1) + sx];
		all_path_bits |= edge_v[sy*(width + 1) + sx + 1];
		if ((all_path_bits & EDGE_PATH) == 0) {
			continue;
		}

		cells[sy*width + sx] = next_path_index;
		coords[0] = (sy << 16) | sx;
		uint start = 0;
		uint end = 1;
		while (start != end) {
			uint const x = coords[start] & 0xffffU;
			uint const y = coords[start] >> 16;
			uint const ic = y*width + x;
			uint const ih = ic;
			uint const iv = y*(width + 1) + x;

			if (edge_v[iv] & EDGE_PATH) {
				if (x == 0) {
					exit_path_indices[exit_path_count++] = next_path_index;
				} else if (cells[ic - 1] == 0) {
					cells[ic - 1] = next_path_index;
					coords[end++] = (y << 16) | (x - 1);
				}
			}
			if (edge_v[iv + 1] & EDGE_PATH) {
				if (x + 1 == width) {
					exit_path_indices[exit_path_count++] = next_path_index;
				} else if (cells[ic + 1] == 0) {
					cells[ic + 1] = next_path_index;
					coords[end++] = (y << 16) | (x + 1);
				}
			}
			if (edge_h[ih] & EDGE_PATH) {
				if (y == 0) {
					exit_path_indices[exit_path_count++] = next_path_index;
				} else if (cells[ic - width] == 0) {
					cells[ic - width] = next_path_index;
					coords[end++] = ((y - 1) << 16) | x;
				}
			}
			if (edge_h[ih + width] & EDGE_PATH) {
				if (y + 1 == height) {
					exit_path_indices[exit_path_count++] = next_path_index;
				} else if (cells[ic + width] == 0) {
					cells[ic + width] = next_path_index;
					coords[end++] = ((y + 1) << 16) | x;
				}
			}

			++start;
		}
		++next_path_index;
	}
	if (exit_path_count > 2) {
		fprintf(stderr, "exit path counting went wrong\n");
		exit(-1);
	}
	memset(highlights, 0, width*height*sizeof(uint));

	// count path lengths
	uint exit_path_length_total = 0;
	for (uint i = 0; i < width*height; ++i) {
		uint const index = cells[i];
		if ((exit_path_count > 0 && exit_path_indices[0] == index) || (exit_path_count > 1 && exit_path_indices[1] == index)) {
			++exit_path_length_total;
		}
	}

	// early out if solved completely
	*is_solved = (exit_path_count == 2 && next_path_index == 2 && exit_path_length_total == width*height);
	if (*is_solved) {
		return false;
	}

	// add barriers to prevent loops or short paths
	bool changed = false;
	for (uint y = 0; y < height; ++y)
	for (uint x = 0; x < width; ++x) {
		uint const index = cells[y*width + x];
		if (index == 0) {
			continue;
		}
		bool const is_exit = (exit_path_length_total < width*height && exit_path_count == 2 && (index == exit_path_indices[0] || index == exit_path_indices[1]));

		uint const kv = y*(width + 1) + x + 1;
		uint const kh = (y + 1)*width + x;

		if (x + 1 < width && (edge_v[kv] & EDGE_PATH) == 0) {
			uint const other_index = cells[y*width + x + 1];
			bool const other_is_exit = (exit_path_count == 2 && (other_index == exit_path_indices[0] || other_index == exit_path_indices[1]));
			if ((index == other_index || (is_exit && other_is_exit)) && (edge_v[kv] & EDGE_BARRIER) == 0) {
				edge_v[kv] |= EDGE_BARRIER;
				changed = true;
			}
		}
		if (y + 1 < height && (edge_h[kh] & EDGE_PATH) == 0) {
			uint const other_index = cells[(y + 1)*width + x];
			bool const other_is_exit = (exit_path_count == 2 && (other_index == exit_path_indices[0] || other_index == exit_path_indices[1]));
			if ((index == other_index || (is_exit && other_is_exit)) && (edge_h[kh] & EDGE_BARRIER) == 0) {
				edge_h[kh] |= EDGE_BARRIER;
				changed = true;
			}
		}
	}

	// for cells where 2 out of 3 available edges would make a loop, add a path edge for the remaining one
	for (uint y = 0; y < height; ++y)
	for (uint x = 0; x < width; ++x) {
		if (cells[y*width + x] != 0) {
			continue;
		}

		// get adjacent edges
		uint *edges[4];
		edges[0] = edge_h + y*width + x;
		edges[1] = edges[0] + width;
		edges[2] = edge_v + y*(width + 1) + x;
		edges[3] = edges[2] + 1;

		// get derived stuff
		uint available_count = 0;
		uint barrier_index = 0;
		uint barrier_count = 0;
		for (uint i = 0; i < 4; ++i) {
			uint const e = *edges[i];
			if (e & EDGE_BARRIER) {
				++barrier_count;
				barrier_index = i;
			} else if ((e & EDGE_PATH) == 0) {
				++available_count;
			}
		}
		if (barrier_count != 1 || available_count != 3) {
			continue;
		}

		// get adjacent cells and their path index in matching order
		uint const *adj[4];
		uint index[4];
		for (int i = 0; i < 4; ++i) {
			uint const px = (uint)((int)x + ((i >= 2) ? (2*i - 5) : 0));
			uint const py = (uint)((int)y + ((i < 2) ? (2*i - 1) : 0));
			if (px < width && py < height) {
				adj[i] = cells + py*width + px;
				index[i] = *adj[i];
			} else {
				adj[i] = NULL;
				index[i] = 0;
			}
		}

		// find two matching path ends over available edges, select the other available edge
		uint const offsets[] = { 1, 2, 3, 1, 2 };
		uint new_index = 4;
		for (uint k = 0; k < 3; ++k) {
			uint const i0 = (barrier_index + offsets[k + 0]) % 4;
			uint const i1 = (barrier_index + offsets[k + 1]) % 4;
			uint const i2 = (barrier_index + offsets[k + 2]) % 4;
			if (index[i0] != 0 && index[i0] == index[i1]) {
				new_index = i2;
				break;
			}
		}

		// force the other edge to be a path
		if (new_index < 4) {
			*edges[new_index] |= EDGE_PATH;
			changed = true;
			if (solver->verbose) {
				highlights[y*width + x] = 1;
				for (uint i = 0; i < 4; ++i) {
					if (i == barrier_index || i == new_index) {
						continue;
					}
					if (adj[i]) {
						highlights[adj[i] - cells] = 1;
					}
				}
			}
		}
	}

	// add barrier to prevent early exits
	if (exit_path_count > 0 && exit_path_length_total < width*height) {
		if (exit_path_count == 1) {
			exit_path_indices[1] = exit_path_indices[0];
		}
		for (uint x = 0; x < width; ++x) {
			uint const index0 = cells[x];
			uint const index1 = cells[(height - 1)*width + x];
			bool const is_exit0 = (index0 == exit_path_indices[0] || index0 == exit_path_indices[1]);
			bool const is_exit1 = (index1 == exit_path_indices[0] || index1 == exit_path_indices[1]);
			uint const k0 = x;
			uint const k1 = height*width + x;
			if (is_exit0 && (edge_h[k0] & (EDGE_BARRIER | EDGE_PATH)) == 0) {
				edge_h[k0] |= EDGE_BARRIER;
				changed = true;
			}
			if (is_exit1 && (edge_h[k1] & (EDGE_BARRIER | EDGE_PATH)) == 0) {
				edge_h[k1] |= EDGE_BARRIER;
				changed = true;
			}
		}
		for (uint y = 0; y < height; ++y) {
			uint const index0 = cells[y*width];
			uint const index1 = cells[y*width + width - 1];
			bool const is_exit0 = (index0 == exit_path_indices[0] || index0 == exit_path_indices[1]);
			bool const is_exit1 = (index1 == exit_path_indices[0] || index1 == exit_path_indices[1]);
			uint const k0 = y*(width + 1);
			uint const k1 = y*(width + 1) + width;
			if (is_exit0 && (edge_v[k0] & (EDGE_BARRIER | EDGE_PATH)) == 0) {
				edge_v[k0] |= EDGE_BARRIER;
				changed = true;
			}
			if (is_exit1 && (edge_v[k1] & (EDGE_BARRIER | EDGE_PATH)) == 0) {
				edge_v[k1] |= EDGE_BARRIER;
				changed = true;
			}
		}
	}

	if (solver->verbose && changed) {
		fputs("\navoid loops and short paths:\n", stdout);
		print_board(solver, board, EDGE_ALL | EDGE_HIGHLIGHT | EDGE_NEW);
	}

	return changed;
}

bool check_partitions(solver_t const *solver, board_t const *board)
{
	uint const width = board->width;
	uint const height = board->height;
	uint *const edge_h = board->edge_h;
	uint *const edge_v = board->edge_v;
	uint *const corners = solver->tmp1;
	uint *const coords = solver->tmp2;

	memset(corners, 0, (width + 1)*(height + 1)*sizeof(uint));

	// set initial state of flood fill from boundary
	uint end = 0;
	for (uint x = 1; x < width; ++x) {
		corners[x] = 1;
		corners[height*(width + 1) + x] = 1;
		coords[end++] = x;
		coords[end++] = (height << 16) | x;
	}
	for (uint y = 1; y < height; ++y) {
		corners[y*(width + 1)] = 1;
		corners[y*(width + 1) + width] = 1;
		coords[end++] = (y << 16);
		coords[end++] = (y << 16) | width;
	}

	// do flood fill along edges
	uint start = 0;
	while (start != end) {
		uint const x = coords[start] & 0xffffU;
		uint const y = coords[start] >> 16;
		uint const i = y*(width + 1) + x;
		uint const ih = y*width + x;
		uint const iv = i;
		uint const s = width + 1;

		if (x > 0 && corners[i - 1] == 0 && (edge_h[ih - 1] & EDGE_BARRIER)) {
			corners[i - 1] = 1;
			coords[end++] = (y << 16) | (x - 1);
		}
		if (x < width && corners[i + 1] == 0 && (edge_h[ih] & EDGE_BARRIER)) {
			corners[i + 1] = 1;
			coords[end++] = (y << 16) | (x + 1);
		}
		if (y > 0 && corners[i - s] == 0 && (edge_v[iv - s] & EDGE_BARRIER)) {
			corners[i - s] = 1;
			coords[end++] = ((y - 1) << 16) | x;
		}
		if (y < height && corners[i + s] == 0 && (edge_v[iv] & EDGE_BARRIER)) {
			corners[i + s] = 1;
			coords[end++] = ((y + 1) << 16) | x;
		}

		++start;
	}

	// check for barriers that would partition the board
	bool changed = false;
	for (uint y = 1; y < height; ++y)
	for (uint x = 0; x < width; ++x) {
		uint const ih = y*width + x;
		uint const i = y*(width + 1) + x;
		uint const iv = i;
		if ((edge_h[ih] & (EDGE_BARRIER | EDGE_PATH)) == 0 && corners[iv] && corners[iv + 1]) {
			edge_h[ih] |= EDGE_PATH;
			changed = true;
		}
	}
	for (uint y = 0; y < height; ++y)
	for (uint x = 1; x < width; ++x) {
		uint const i = y*(width + 1) + x;
		uint const iv = i;
		uint const s = width + 1;
		if ((edge_v[iv] & (EDGE_BARRIER | EDGE_PATH)) == 0 && corners[iv] && corners[iv + s]) {
			edge_v[iv] |= EDGE_PATH;
			changed = true;
		}
	}

	if (changed && solver->verbose) {
		fputs("\navoid partitioning:\n", stdout);
		print_board(solver, board, EDGE_ALL | EDGE_NEW);
	}

	return changed;
}

uint solve(solver_t const *solver, board_t const *board, bool *is_solved)
{
	if (solver->verbose) {
		fputs("\ninitial conditions:\n", stdout);
		print_board(solver, board, EDGE_BOUNDARY);
	}

	uint step_count = 0;
	for (;; ++step_count) {
		if (solver->verbose) {
			copy_edges_to_solver(solver, board);
		}

		if (check_single_cells(solver, board)) {
			continue;
		}

		if (check_loops(solver, board, is_solved)) {
			continue;
		}
		if (*is_solved) {
			break;
		}

		if (check_partitions(solver, board)) {
			continue;
		}

		if (parity_check_all_block_sizes(solver, board)) {
			continue;
		}

		break;
	}
	return step_count;
}

uint harden(solver_t const *solver, board_t *board)
{
	// check boundary locations on initial board
	uint const width = board->width;
	uint const height = board->height;
	uint const *const edge_h = board->edge_h;
	uint const *const edge_v = board->edge_v;
	uint *const trials = (uint *)malloc(2*(width + 1)*(height + 1)*sizeof(uint));
	uint trial_count = 0;
	for (uint y = 0; y <= height; ++y) {
		for (uint x = 0; x < width; ++x) {
			if (edge_h[y*width + x] & EDGE_BOUNDARY) {
				trials[trial_count++] = (y << 16) | (x << 1);
			}
		}
	}
	for (uint y = 0; y < height; ++y) {
		for (uint x = 0; x <= width; ++x) {
			if (edge_v[y*(width + 1) + x] & EDGE_BOUNDARY) {
				trials[trial_count++] = (y << 16) | (x << 1) | 1;
			}
		}
	}

	// shuffle order
	for (uint shuffle_index = 0; shuffle_index < 1000; ++shuffle_index) {
		uint const i = rand() % trial_count;
		uint const j = rand() % trial_count;
		uint const tmp = trials[i];
		trials[i] = trials[j];
		trials[j] = tmp;
	}

	// try and remove them in this order
	uint success_count = 0;
	for (uint trial_index = 0; trial_index < trial_count; ++trial_index) {
		// copy existing board initial conditions
		board_t test;
		copy_board(&test, board);
		reset_to_boundary(&test);

		// knock out the edge
		uint const tmp = trials[trial_index];
		uint const x = (tmp >> 1) & 0x7fffU;
		uint const y = tmp >> 16;
		bool const is_vertical = ((tmp & 1) != 0);
		if (is_vertical) {
			test.edge_v[y*(width + 1) + x] &= ~(EDGE_BOUNDARY | EDGE_BARRIER);
		} else {
			test.edge_h[y*width + x] &= ~(EDGE_BOUNDARY | EDGE_BARRIER);
		}
		reset_to_boundary(&test);

		// keep if still solveable
		bool is_solved = false;
		solve(solver, &test, &is_solved);
		if (is_solved) {
			swap_board(board, &test);
			++success_count;
		}
		free_board(&test);
	}
	reset_to_boundary(board);
	return success_count;
}

int main(int argc, char *argv[])
{
	FILE *fp = stdin;
	char const *puzzle = NULL;
	bool verbose = false;
	bool try_removing_edges = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-f") == 0) {
			++i;
			if (i < argc) {
				fp = fopen(argv[i], "r");
				if (!fp) {
					fprintf(stderr, "failed to open \"%s\" for reading!\n", argv[i]);
					return -1;
				}
			}
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose = true;
		} else if (strcmp(argv[i], "-r") == 0) {
			try_removing_edges = true;
		} else {
			fprintf(stderr, "unknown option \"%s\"!\n", argv[i]);
			return -1;
		}
	}

	// read in a test level
	board_t board;
	if (!scan_board(&board, fp)) {
		return -1;
	}

	// set up solver
	solver_t solver;
	init_solver(&solver, &board);
	if (puzzle) {
		if (verbose) {
			fputs("\ndecoded puzzle:\n", stdout);
			print_board(&solver, &board, EDGE_ALL);
		}
		reset_to_boundary(&board);
	}

	// try to optimise
	if (try_removing_edges) {
		uint const success_count = harden(&solver, &board);
		printf("removed %d edges!\n", success_count);
	}

	// iterate until solved or not progressing
	solver.verbose = verbose;
	bool is_solved = false;
	uint step_count = solve(&solver, &board, &is_solved);
	printf("\n%s after %d steps!\n", is_solved ? "solved" : "given up", step_count);
	print_board(&solver, &board, EDGE_SOLUTION);
	return 0;
}
