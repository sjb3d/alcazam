#include "io.h"
#include <stdlib.h>
#include <memory.h>

typedef struct
{
	uint is_corner : 1;
	uint is_boundary_h : 1;
	uint is_boundary_v : 1;
	uint is_barrier_h : 1;
	uint is_barrier_v : 1;
	uint is_path_n : 1;
	uint is_path_s : 1;
	uint is_path_w : 1;
	uint is_path_e : 1;
	uint is_lit : 1;
	uint is_new : 1;
} raster_t;

typedef enum
{
	COLOR_CORNER,
	COLOR_CORNER_LIT,
	COLOR_BOUNDARY,
	COLOR_BOUNDARY_LIT,
	COLOR_BARRIER,
	COLOR_BARRIER_LIT,
	COLOR_BARRIER_NEW,
	COLOR_PATH,
	COLOR_PATH_LIT,
	COLOR_PATH_NEW,
	COLOR_OFF
} color_t;

bool scan_board(board_t *board, FILE *fp)
{
	char buf[1024];

	uint width = 0;
	uint *edge_h = NULL;
	uint *edge_v = NULL;

	// read lines, expand the board as we go
	uint line_index = 0;
	uint first_edge = 0;
	for (;;) {
		memset(buf, 0, first_edge + width*4 + 1);
		if (!fgets(buf, sizeof(buf), fp)) {
			break;
		}

		// ignore blank or comment lines
		if (*buf == '#' || *buf == '\r' || *buf == '\n') {
			continue;
		}

		// first line specifies character offset and board width
		if (width == 0) {
			char *p = buf;
			for (;; ++first_edge) {
				char const c = p[first_edge];
				if (c == '\0' || c == '+') {
					break;
				}
			}
			uint last_edge = first_edge;
			for (uint i = first_edge + 1;; ++i) {
				char const c = p[i];
				if (c == '\0') {
					break;
				}
				if (c == '+') {
					last_edge = i;
				}
			}
			last_edge -= first_edge;
			if ((last_edge % 4) != 0 || last_edge == 0) {
				fprintf(stderr, "invalid board width");
				return false;
			}
			width = last_edge/4;
			edge_h = malloc(width*sizeof(uint));
		}

		// read vertical or horizontal marks
		if (line_index & 1) {
			uint const height = (line_index + 1)/2;
			edge_h = (uint *)realloc(edge_h, width*(height + 1)*sizeof(uint));
			edge_v = (uint *)realloc(edge_v, (width + 1)*height*sizeof(uint));
			uint *const v = edge_v + (width + 1)*(height - 1);
			for (uint x = 0; x <= width; ++x) {
				v[x] = (buf[first_edge + 4*x] == '|') ? (EDGE_BOUNDARY | EDGE_BARRIER) : 0;
			}
		} else {
			uint const height = line_index/2;
			uint *const h = edge_h + width*height;
			for (uint x = 0; x < width; ++x) {
				h[x] = (buf[first_edge + 4*x + 2] == '-') ? (EDGE_BOUNDARY | EDGE_BARRIER) : 0;
			}
		}
		++line_index;
	}

	if ((line_index & 1) == 0 || line_index == 1) {
		fprintf(stderr, "invalid board height");
		return false;
	}

	board->width = width;
	board->height = (line_index - 1)/2;
	board->edge_h = edge_h;
	board->edge_v = edge_v;
	return true;
}

void print_board(solver_t const *solver, board_t const *board, uint bits)
{
	uint const width = board->width;
	uint const height = board->height;
	uint const *const edge_h = board->edge_h;
	uint const *const edge_v = board->edge_v;
	uint const *const edge_h_old = solver->edge_h_old;
	uint const *const edge_v_old = solver->edge_v_old;
	uint const *const highlights = solver->tmp1;

	uint const boundary_bit = bits & EDGE_BOUNDARY;
	uint const barrier_bit = bits & EDGE_BARRIER;
	uint const path_bit = bits & EDGE_PATH;
	bool const show_highlight = (bits & EDGE_HIGHLIGHT);
	bool const show_new = (bits & EDGE_NEW);

	uint const raster_width = 4*width + 1;
	uint const raster_height = 2*height + 1;
	uint const raster_count = raster_width*raster_height;
	raster_t *const raster = (raster_t *)malloc(raster_count*sizeof(raster_t));
	memset(raster, 0, raster_count*sizeof(raster_t));

	// write tick marks
	for (uint y = 0; y <= height; ++y)
	for (uint x = 0; x <= width; ++x) {
		uint const ir = 2*y*raster_width + 4*x;
		raster[ir].is_corner = 1;
	}

	// write edges
	for (uint y = 0; y <= height; ++y)
	for (uint x = 0; x < width; ++x) {
		uint const ih = y*width + x;
		uint const ir = 2*y*raster_width + 4*x;
		uint const is_new = (show_new && (edge_h_old[ih] & (EDGE_BARRIER | EDGE_PATH)) == 0) ? 1 : 0;
		if (edge_h[ih] & boundary_bit) {
			raster[ir + 1].is_boundary_h = 1;
			raster[ir + 2].is_boundary_h = 1;
			raster[ir + 3].is_boundary_h = 1;
		} else if (edge_h[ih] & barrier_bit) {
			raster[ir + 1].is_barrier_h = 1;
			raster[ir + 1].is_new |= is_new;
			raster[ir + 2].is_barrier_h = 1;
			raster[ir + 2].is_new |= is_new;
			raster[ir + 3].is_barrier_h = 1;
			raster[ir + 3].is_new |= is_new;
		} else if (edge_h[ih] & path_bit) {
			if (y > 0) {
				raster[ir + 2 - raster_width].is_path_s = 1;
				raster[ir + 2 - raster_width].is_new |= is_new;
			}
			raster[ir + 2].is_path_n = 1;
			raster[ir + 2].is_path_s = 1;
			raster[ir + 2].is_new |= is_new;
			if (y < height) {
				raster[ir + 2 + raster_width].is_path_n = 1;
				raster[ir + 2 + raster_width].is_new |= is_new;
			}
		}
	}
	for (uint y = 0; y < height; ++y)
	for (uint x = 0; x <= width; ++x) {
		uint const iv = y*(width + 1) + x;
		uint const ir = 2*y*raster_width + 4*x;
		uint const is_new = (show_new && (edge_v_old[iv] & (EDGE_BARRIER | EDGE_PATH)) == 0) ? 1 : 0;
		if (edge_v[iv] & boundary_bit) {
			raster[ir + raster_width].is_boundary_v = 1;
		} else if (edge_v[iv] & barrier_bit) {
			raster[ir + raster_width].is_barrier_v = 1;
			raster[ir + raster_width].is_new |= is_new;
		} else if (edge_v[iv] & path_bit) {
			if (x > 0) {
				raster[ir + raster_width - 2].is_path_e = 1;
				raster[ir + raster_width - 2].is_new |= is_new;
				raster[ir + raster_width - 1].is_path_w = 1;
				raster[ir + raster_width - 1].is_path_e = 1;
				raster[ir + raster_width - 1].is_new |= is_new;
			}
			raster[ir + raster_width].is_path_w = 1;
			raster[ir + raster_width].is_path_e = 1;
			raster[ir + raster_width].is_new |= is_new;
			if (x < width) {
				raster[ir + raster_width + 1].is_path_w = 1;
				raster[ir + raster_width + 1].is_path_e = 1;
				raster[ir + raster_width + 1].is_new |= is_new;
				raster[ir + raster_width + 2].is_path_w = 1;
				raster[ir + raster_width + 2].is_new |= is_new;
			}
		}
	}

	// write highlights
	if (show_highlight) {
		for (uint y = 0; y < height; ++y)
		for (uint x = 0; x < width; ++x) {
			if (highlights[y*width + x]) {
				uint const ir = 2*y*raster_width + 4*x;
				for (uint oy = 0; oy <= 2; ++oy)
				for (uint ox = 0; ox <= 4; ++ox) {
					raster[ir + oy*raster_width + ox].is_lit = 1;
				}
			}
		}
	}

	// print to screen
	raster_t const *r = raster;
	for (uint y = 0; y < raster_height; ++y) {
		color_t last_col = COLOR_OFF;
		for (uint x = 0; x < raster_width; ++x, ++r) {
			char sym = ' ';
			color_t col = r->is_lit ? COLOR_BOUNDARY_LIT : COLOR_OFF;
			if (r->is_corner) {
				sym = '+';
				col = r->is_lit ? COLOR_CORNER_LIT : COLOR_CORNER;
			} else if (r->is_boundary_h || r->is_boundary_v) {
				sym = r->is_boundary_h ? '-' : '|';
				col = r->is_lit ? COLOR_BOUNDARY_LIT : COLOR_BOUNDARY;
			} else if (r->is_barrier_h || r->is_barrier_v) {
				sym = r->is_barrier_h ? '-' : '|';
				col = r->is_new ? COLOR_BARRIER_NEW : r->is_lit ? COLOR_BARRIER_LIT : COLOR_BARRIER;
			} else {
				uint const h_path_count = r->is_path_w + r->is_path_e;
				uint const v_path_count = r->is_path_n + r->is_path_s;
				if (h_path_count + v_path_count == 2) {
					if (h_path_count == 2) {
						sym = '-';
					} else if (v_path_count == 2) {
						sym = '|';
					} else {
						sym = '+';
					}
					col = r->is_new ? COLOR_PATH_NEW : r->is_lit ? COLOR_PATH_LIT : COLOR_PATH;
				}
			}

			if (col != last_col) {
				char const *str = NULL;
				switch (col) {
					case COLOR_CORNER:			str = "\033[0m\x1b[37m";		break;
					case COLOR_CORNER_LIT:		str = "\033[0m\x1b[37;43m";		break;
					case COLOR_BOUNDARY:		str = "\033[0m";				break;
					case COLOR_BOUNDARY_LIT:	str = "\033[0m\x1b[43m";		break;
					case COLOR_BARRIER:			str = "\033[0m\x1b[31m";		break;
					case COLOR_BARRIER_LIT:		str = "\033[0m\x1b[31;43;1m";	break;
					case COLOR_BARRIER_NEW:		str = "\033[0m\x1b[31;41;1m";	break;
					case COLOR_PATH:			str = "\033[0m\x1b[36;1m";		break;
					case COLOR_PATH_LIT:		str = "\033[0m\x1b[36;43;1m";	break;
					case COLOR_PATH_NEW:		str = "\033[0m\x1b[36;46;1m";	break;
					default:
					case COLOR_OFF:				str = "\033[0m";				break;
				}
				fputs(str, stdout);
				last_col = col;
			}
			fputc(sym, stdout);
		}
		fputs("\033[0m\n", stdout);
	}

	free(raster);
}
