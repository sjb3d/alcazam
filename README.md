# Alcazam

An [Alcazar](http://www.theincrediblecompany.com/alcazar-1/) puzzle solver in C.

Uses forward logical steps only, so any solution must be a unique solution.  The steps used by the solver are:

* Check single cells:
  * If two edges are crossed, mark the remaining two edges as blocked (red in verbose output)
  * If two edges are blocked, mark the remaining two edges as crossed
* Check for loops or early exits:
  * Block any edge that could be used to make a loop or solution that does not touch all cells
  * If two edges of a cell would create a loop but the other is blocked, mark the remaining edge as crossed
* Partition check:
  * If adding a single blocked edge would partition the level into two disconnected sets of cells, mark this edge as crossed instead
* Parity check:
  * Consider all NxM blocks of cells, then check [checkerboard parity](http://edderiofer.blogspot.co.uk/2014/11/on-subject-of-parity-when-we-refer-to.html) on islands of connected cells within them

Once one of the steps succeeds, the solver goes back to the first step with the new partially solved puzzle.  If all steps fail then the solver gives up and outputs what it has so far.

## Usage

```
alcazam [-f filename] [-r] [-v]
   -f filename	Reads puzzle from the given file, otherwise use stdin.
   -r           Remove as many edges as possible without making unsolveable.
   -v           Verbose output, show all the steps used to find solution.
```

## Puzzle Format

Puzzle files are written in ASCII text, here is an example:

```
# www.theincrediblecompany.com/2014/11/13/introducing-the-ball-rooms/
+   +---+   +---+---+---+---+---+---+   +
|                               |       |
+   +   +   +   +---+   +   +   +   +   +
|                                       |
+   +   +---+---+---+---+   +   +   +   +
        |       |       |               |
+   +   +   +   +   +   +   +   +   +   +
|                                       |
+   +   +---+   +   +   +   +---+   +   +
|                               |       |
+   +   +---+   +   +   +   +   +   +   +
|                                       |
+---+   +   +   +---+   +   +---+   +   +
|               |           |           |
+   +   +   +   +   +   +   +   +   +   +
|       |                               |
+   +   +   +   +   +   +   +   +   +   +
|                                   |   |
+   +   +   +   +   +   +   +   +   +   +
|               |           |           |
+---+---+   +---+---+   +---+---+---+   +
```

Any blank line or line starting with a _#_ is ignored.  Characters other than +,- or | are ignored.

### Solutions

The solution is output as ASCII using ANSI color codes:

![example solution](http://sjb3d.github.io/alcazam/img/ball_room_example_solved.png)

If using the verbose option, each step to the solution is shown.  Here is an example step used in the solution of the above puzzle:

![example step](http://sjb3d.github.io/alcazam/img/ball_room_example_step.png)

Enjoy!
