#ifndef PROJECT_DISPLAY_H
#define PROJECT_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The viewer shows a scene, lets your code change it, then shows it again:
 *
 *   do {
 *       scene_add_mesh(...);      // as many scene_add_*() as you want
 *       scene_add_nodes(...);
 *   } while (display() && myAlgorithmStep(mesh)); // 0 once the window closes
 *   display_wait();               // keep the last scene on screen
 *
 * To just look at a mesh: scene_add_mesh(...); display(); display_wait();
 *
 * The window opens at the first display() call and stays open until
 * display_wait(). Each display() call returns when the user presses space
 * (holding space runs through the steps), and the next scene_add_*() starts
 * a new scene.
 * The arrays are NOT copied: they must stay valid until display() returns,
 * and those of the last scene until display_wait() returns.
 */


/* Add a triangle mesh in the next frame
 *
 * A NULL fields pointer, zero nfields or an invalid render_field displays a
 * plain grey mesh.
 *
 * @param nodes: the coordinates of the nodes, in row major.
 *               Size = 2*nnodes
 * @param nnodes: the number of nodes (NOT the number of coordinates!)
 * @param elems: the triangles in the mesh, in row major.
 *               Size = 3*nelems
 * @param nelems: the number of elements
 * @param fields: the different fields to visualize, stored in row major (e.g. x1, y1, z1, x2, y2, z2, ...).
 *                Size = nnodes*nfields
 * @param nfields: the number of different fields that are stored.
 * @param render_field: the index of the field to render
 * @return id The id of the mesh in the scene (its index)
 */
int scene_add_mesh(
    double   const * nodes,  uint32_t nnodes,
    uint32_t const * elems,  uint32_t nelems,
    double   const * fields, uint32_t nfields,
    int render_field);

/* Add an edge list in the next frame
 *
 * @param nodes: the coordinates of the nodes, in row major.
 *               Size = 2*nnodes
 * @param nnodes: the number of nodes
 * @param edges: the edges to draw, in row major.
 *               Size = 2*nedges
 * @param nedges: the number of edges
 * @return id The id of the edge list in the scene (its index)
 */
int scene_add_edges(
    double   const * nodes, uint32_t nnodes,
    uint32_t const * edges, uint32_t nedges);

/* Add a node list in the next frame
 *
 * @param nodes: the coordinates of the nodes, in row major.
 *               Size = 2*nnodes
 * @param nnodes: the number of nodes
 * @return id The id of the node list in the scene (its index)
 */
int scene_add_nodes(double const * nodes, uint32_t nnodes);

/* Shows the created scene (using scene_add_...)
 *
 * @return 1 to go on, 0 if the window was closed (every later call then
 *         returns 0 at once, until display_wait())
 */
int display(void);

/* Keeps the last scene shown on screen until the user closes the window,
 * then frees the viewer. The next display() call opens a new window. */
void display_wait(void);

#ifdef __cplusplus
};
#endif

#endif // PROJECT_DISPLAY_H
