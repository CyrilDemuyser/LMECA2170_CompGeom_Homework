# How the viewer works

The viewer displays 2D triangle meshes, edge lists and node lists, and is made
for watching an algorithm run: show the mesh, change it, show it again. GLFW
handles the window and user input; OpenGL 1.1 draws the geometry using simple
`glBegin`/`glEnd` calls. The public API is in
[project_display.h](../include/project_display.h), and the implementation is
in [project_display.c](../src/project_display.c).

## Try it

With GLFW and OpenGL installed, run these commands from `code/`:

```sh
cmake -B build && cmake --build build
./build/exec/0_example_viewer 3   # triangle mesh coloured by a nodal field
./build/exec/0_example_viewer 4   # boundary edges of the same mesh
./build/exec/0_example_viewer 5   # the same mesh being built, one operation per step
```

## The calls

| Call                | What it does                                                    |
| ------------------- | --------------------------------------------------------------- |
| `scene_add_mesh()`  | Adds a triangle mesh to the scene, returns its index            |
| `scene_add_edges()` | Adds a list of edges to the scene, returns its index            |
| `scene_add_nodes()` | Adds a list of nodes to the scene, returns its index            |
| `display()`         | Shows the scene as the next step, returns on space              |
| `display_wait()`    | Keeps the last scene up until the window is closed, closes it   |

Build a scene with as many `scene_add_*()` calls as you like, then call
`display()` to draw all of its objects together. The next `scene_add_*()`
after a `display()` starts a new, empty scene. The window opens at the first
`display()` call and stays open until `display_wait()`. `display()` returns
`1` when you press space, and `0` once the window has been closed.

To just look at a mesh:

```c
scene_add_mesh(nodes, nnodes, elems, nelems, NULL, 0, 0);
display();
display_wait();
```

To show a mesh together with some points, for example the nodes of a
boundary you are working on:

```c
scene_add_mesh(nodes, nnodes, elems, nelems, NULL, 0, 0);
scene_add_edges(bnd_nodes, bnd_nnodes, bnd_edges, bnd_nedges);
scene_add_nodes(query_nodes, nq);
display();
```

To watch your algorithm, put the display call in your own loop:

```c
do {
    /* refresh nodes, nnodes, elems, nelems from the mesh */
    scene_add_mesh(nodes, nnodes, elems, nelems, NULL, 0, 0);
} while (display() && myAlgorithmStep(mesh)); /* change the mesh here */
display_wait();                   /* keep the final mesh on screen */
```

- **The arrays are not copied.** The scene only keeps your pointers, so the
  arrays must stay valid until `display()` returns. Once it has returned you
  may change, reallocate or free them, and add them again for the next step.
  The exception is the last scene: `display_wait()` draws it again, so its
  arrays must stay valid until `display_wait()` returns.
- **Closing the window ends the loop.** After that every `display()` returns
  `0` at once, so the rest of your program runs without being shown.
  `display_wait()` then frees the viewer; a later `display()` opens a new
  window.
- **The view stays put between steps.** Pan, zoom and framing are kept, so
  the picture does not jump as the mesh changes. Press `r` to frame the
  current scene again.
- **Hold space to run through the steps.** It goes at the keyboard repeat
  rate, about 30 steps per second. For a long run, only show every Nth step:
  `if (step % 100 == 0) { scene_add_mesh(...); if (!display()) break; }`.
- The window title shows the step number.

`buildDemoMesh()` in [exec/0_example_viewer.c](../exec/0_example_viewer.c)
(case 5) calls the viewer after every split; with those calls removed it is
the plain algorithm.

## Passing data to the viewer

`scene_add_mesh(nodes, nnodes, elems, nelems, fields, nfields, render_field)`
takes flat arrays:

- `nodes`: coordinates as `x0, y0, x1, y1, ...` (`double`).
- `elems`: three node indices per triangle, in counter-clockwise order
  (`uint32_t`). Indices start at zero.
- `fields`: values grouped by node. Field `f` at node `v` is stored at
  `fields[nfields * v + f]`.
- `render_field`: the zero-based index of the field used for colouring.
  Pass `fields = NULL` for a plain grey mesh.

For example, this displays one triangle with one scalar value per node:

```c
#include "project_display.h"

int main(void) {
    double nodes[] = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    uint32_t elems[] = {0, 1, 2};
    double fields[] = {0.0, 0.5, 1.0};

    scene_add_mesh(nodes, 3, elems, 1, fields, 1, 0);
    display();
    display_wait();
    return 0;
}
```

`scene_add_edges(nodes, nnodes, edges, nedges)` uses the same coordinates, with
two `uint32_t` node indices per edge. It draws black lines.
`scene_add_nodes(nodes, nnodes)` only takes the coordinates and draws black
points.

The mesh data structure stores connectivity as `int`;
`showMesh()` in [exec/0_example_viewer.c](../exec/0_example_viewer.c) shows how
to copy it into a `uint32_t` array, and keeps that copy alive until the next
step since the viewer does not copy it. If triangle connectivity is stored as
`uint32_t` and exposed by the getters with that type, it can be passed
directly to `scene_add_mesh()` without a conversion copy.

## Controls

| Input            | Action                                               |
| ---------------- | ---------------------------------------------------- |
| Space            | Next step (hold it to run through the steps)         |
| Left-button drag | Pan                                                  |
| Scroll           | Zoom                                                 |
| `w`              | Toggle triangle outlines (`z` in azerty layout)      |
| `f`              | Toggle filled triangles                              |
| `h`              | Cycle through no arrows, arrows, arrows with numbers |
| `n`              | Toggle node indices                                  |
| `r`              | Reset pan and zoom, and frame the current scene      |
| `q` or Escape    | Close the window (`a` in azerty layout)              |

The keys apply to every object of the scene. `w` and `f` only affect
triangle meshes; edge lists always show their lines. During `display_wait()`, space does nothing.

## Inside the viewer

`project_display.c` keeps its state in a few globals: `window` (the GLFW
window, `NULL` until the first call), `scene` (the list of objects to draw,
each one holding the caller's pointers), `step` (the step counter) and `view`
(pan, zoom, framing and the toggles set by the keys).

1. **`sceneAdd()`** is what every `scene_add_*()` calls; they only differ in
   the number of nodes per element (3 for triangles, 2 for edges, no elements
   for nodes). If the scene was already displayed it is emptied first. The
   object is appended with the caller's pointers, and the range of the
   selected field (`fields[nfields * v + render_field]`) is computed.
2. **`display()`** opens the window if needed, and returns `0` at once if it
   has already been closed. It computes the bounds of all the objects with
   `sceneBox()`; on the first step, `viewReset()` frames the view on them.
   It marks the scene as displayed, so the next `scene_add_*()` starts a new
   one.
3. **`handleEvents()`** draws the scene, then sleeps until the next event
   (key, mouse, resize, ...). GLFW calls `onKey()`, `onScroll()` or
   `onButton()` for it, which update `view`, and the loop draws again. It
   returns when space is pressed or the window is closed; in
   `display_wait()` only closing ends it.
4. **`drawScene()`** draws one frame. `beginFrame()` sets a flat,
   aspect-ratio-correct view, centred on the frame and adjusted by pan and
   zoom. Then, for each object in the order it was added, come the layers,
   according to the enabled controls: nodes, filled triangles, black
   outlines, blue half-edge arrows, and node labels. Field values are scaled
   between their minimum and maximum and mapped to a jet-like colour scale;
   OpenGL interpolates the node colours inside each triangle.

Half-edge arrows follow the triangle's node order: local side `k` of triangle
`t` runs from node `k` to node `(k + 1) % 3` and is numbered `3*t + k`, matching
[HeObject.h](../include/HeObject.h). Arrows sit inside their triangle so that
the two directions along a shared edge remain distinct. For an edge list,
arrows follow each stored node pair and labels use the edge index. Numbers
are local to each object: two objects of a scene both start at 0.

Numbers are drawn with a small built-in seven-segment font. They are most
useful on small meshes, where the labels stay readable; on big meshes they can
crowd the view. Feel free to improve the viewer and make your own adjustments.
