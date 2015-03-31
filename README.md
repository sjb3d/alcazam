# Alcazam

An [Alcazar](http://www.theincrediblecompany.com/alcazar-1/) puzzle solver in C.

Uses forward logical steps only, so any solution must be a unique solution.

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
