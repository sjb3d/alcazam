#pragma once

#include <stdbool.h>

typedef unsigned int uint;

#define EDGE_BOUNDARY		0x01U
#define EDGE_BARRIER		0x02U
#define EDGE_PATH			0x04U
#define EDGE_SOLUTION		(EDGE_BOUNDARY | EDGE_PATH)
#define EDGE_ALL			(EDGE_BOUNDARY | EDGE_BARRIER | EDGE_PATH)
#define EDGE_HIGHLIGHT		0x08U
#define EDGE_NEW			0x10U

typedef struct
{
	uint width;
	uint height;
	uint *edge_h;	// horizontal edge bits: width*(height + 1)
	uint *edge_v;	// vertical edge bits: (width + 1)*height
} board_t;

typedef struct
{
	uint *edge_h_old;
	uint *edge_v_old;
	uint *tmp1;			// temp storage: (width + 1)*(height + 1)
	uint *tmp2;
	bool verbose;
} solver_t;
