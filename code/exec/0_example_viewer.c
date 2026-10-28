/*
 * Small demo of the array-based half-edge data structure.
 *
 *   ./mesh 1   default triangle -> split one edge -> split one face
 *   ./mesh 2   build the structure from a hand written node/triangle list
 *   ./mesh 3   show a mesh in the viewer
 *   ./mesh 4   show its boundary edges in the viewer
 *   ./mesh 5   watch that mesh being built, one operation per step
 */

#include "HeGetters.h"
#include "HeObject.h"
#include "HeOperations.h"
#include "project_display.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* ========================================================================== */
/*  Printing, through the getters only                                        */
/* ========================================================================== */

static void printMesh(const Mesh *mesh, const char *title)
{
  int nNodes, nTriangles, nHe;
  const double *xy = getNodes(mesh, &nNodes);
  const int *tri = getTriangles(mesh, &nTriangles);
  const int *start = getStartHE(mesh, NULL);
  const int *twin = getHeTwin(mesh, &nHe);

  printf("\n===== %s =====\n", title);
  printf("  %d nodes (room for %d), %d triangles (room for %d)\n",
         nNodes,
         mesh->allocNodes,
         nTriangles,
         mesh->allocTriangles);

  printf("\n  nodes                      triangles\n");
  printf("  idx      x      y  startHE    idx   nodes\n");
  for (int i = 0; i < nNodes || i < nTriangles; i++)
  {
    if (i < nNodes)
      printf("  %3d %6.3f %6.3f  %5d ", i, xy[2 * i], xy[2 * i + 1], start[i]);
    else
      printf("                            ");

    if (i < nTriangles)
      printf("    %3d   %2d %2d %2d",
             i,
             tri[3 * i],
             tri[3 * i + 1],
             tri[3 * i + 2]);
    printf("\n");
  }

  printf("\n  half-edges\n");
  printf("   he  tri  origin -> dest   next   twin\n");
  for (int h = 0; h < nHe; h++)
  {
    printf("  %3d  %3d  %6d -> %4d  %5d  %5d",
           h,
           h / 3,
           tri[h],
           tri[heNext(h)],
           heNext(h),
           twin[h]);
    if (twin[h] == HE_BOUNDARY)
      printf("   (boundary)");
    printf("\n");
  }

  int errors = meshCheck(mesh);
  printf("\n  meshCheck: %s\n", errors == 0 ? "ok" : "BROKEN");
}

// EXAMPLE : computes the mid point of an edge.
static void edgeMidpoint(const Mesh *mesh, int he, double *x, double *y)
{
  int node1 = mesh->triangles[he];
  int node2 = mesh->triangles[heNext(he)];
  *x = 0.5 * (mesh->coords[2 * node1] + mesh->coords[2 * node2]);
  *y = 0.5 * (mesh->coords[2 * node1 + 1] + mesh->coords[2 * node2 + 1]);
}

// EXAMPLE : computes the centroid of a face. This time, we get the arrays
// through getters.
static void triangleCentroid(const Mesh *mesh, int t, double *x, double *y)
{
  const double *xy = getNodes(mesh, NULL);
  const int *tri = getTriangles(mesh, NULL);
  *x = *y = 0.0;
  for (int i = 0; i < 3; i++)
  {
    *x += xy[2 * tri[3 * t + i] + 0] / 3.0;
    *y += xy[2 * tri[3 * t + i] + 1] / 3.0;
  }
}

/* ========================================================================== */
/*  Case 1 : default triangle, then an edge split, then a face split          */
/* ========================================================================== */

static void demoSplits(void)
{
  double x, y;

  /* [1] the default triangle (0,0) (1,0) (0,1) */
  Mesh *mesh = meshInitDefault();

  /* [2] add a point that cuts an edge: the midpoint of half-edge 0 */
  int he = 0;
  edgeMidpoint(mesh, he, &x, &y);
  edgeSplit(mesh, he, x, y);

  /* [3] add a point that cuts a face: the centroid of triangle 1 */
  int tri = 1;
  triangleCentroid(mesh, tri, &x, &y);
  faceSplit(mesh, tri, x, y);

  printMesh(mesh, "Final mesh for demo1 :-).");
  /* Free the mesh when done with it! :-) */
  freeMesh(mesh);
}

/* ========================================================================== */
/*  Case 2 : build the structure from a hand written list                     */
/* ========================================================================== */

static void demoFromArrays(void)
{
  /*        3-----------2        a unit square, 5 nodes, 4 triangles.
   *        | \       / |        Node 4 is slightly off-centre so that the
   *        |   \   /   |        swap in step [3] is not degenerate.
   *        |     4     |
   *        |   /   \   |
   *        | /       \ |
   *        0-----------1
   */
  static const double coords[] = {0.00,
                                  0.00, // 0
                                  1.00,
                                  0.00, // 1
                                  1.00,
                                  1.00, // 2
                                  0.00,
                                  1.00, // 3
                                  0.45,
                                  0.55}; // 4
  static const int triangles[] = {0,
                                  1,
                                  4, // all counter-clockwise
                                  1,
                                  2,
                                  4,
                                  2,
                                  3,
                                  4,
                                  3,
                                  0,
                                  4};

  int nNodes = (int)(sizeof(coords) / (2 * sizeof(double)));
  int nTriangles = (int)(sizeof(triangles) / (3 * sizeof(int)));

  Mesh *mesh = meshInitFromArrays(coords, nNodes, triangles, nTriangles);
  printMesh(mesh, "[1] meshInitFromArrays()");

  /* [2] bonus: the third operation, on the interior edge of half-edge 1 */
  int he = 1;
  int status = edgeSwap(mesh, he);
  printf("\n>>> edgeSwap(he=%d) returned %d (%s)\n",
         he,
         status,
         status == 0    ? "swapped"
         : status == -1 ? "boundary edge"
                        : "refused, non convex quad");
  printMesh(mesh, "[2] after edgeSwap");

  freeMesh(mesh);
}

/* ========================================================================== */
/*  Case 3 and 4 : the OpenGL viewer                                          */
/* ========================================================================== */

/* M_PI is not in C11 with -pedantic */
#define PI 3.14159265358979323846

// The viewer takes uint32_t connectivity, the mesh stores int: copy over.
static uint32_t *toU32(const int *src, int n)
{
  uint32_t *dst = malloc((size_t)n * sizeof(uint32_t));
  for (int i = 0; i < n; i++)
    dst[i] = (uint32_t)src[i];
  return dst;
}

// shows the current mesh as the next step of the viewer, plain grey
static void showMesh(const Mesh *mesh)
{
  int nNodes, nTriangles;
  const double *xy = getNodes(mesh, &nNodes);
  const int *conn = getTriangles(mesh, &nTriangles);
  // ponytail: the viewer keeps the pointer, so tri lives until the next call
  // (the last one until exit, for display_wait())
  static uint32_t *tri = NULL;
  free(tri);
  tri = toU32(conn, 3 * nTriangles);

  scene_add_mesh(xy, (uint32_t)nNodes, tri, (uint32_t)nTriangles, NULL, 0, 0);
  display();
}

/* A unit square split in 4, then refined twice, so that there is something to
 * look at in the window. With watch set, the viewer shows every step. */
static Mesh *buildDemoMesh(int watch)
{
  static const double coords[] = {
      0.00, 0.00, 1.00, 0.00, 1.00, 1.00, 0.00, 1.00, 0.45, 0.55};
  static const int triangles[] = {0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4};

  Mesh *mesh = meshInitFromArrays(coords, 5, triangles, 4);
  if (watch)
    showMesh(mesh);

  /* split every face once, then split the first edge of each new face */
  for (int t = getNumTriangles(mesh) - 1; t >= 0; t--)
  {
    double x, y;
    triangleCentroid(mesh, t, &x, &y);
    faceSplit(mesh, t, x, y);
    if (watch)
      showMesh(mesh);
  }
  for (int t = getNumTriangles(mesh) - 1; t >= 0; t--)
  {
    double x, y;
    edgeMidpoint(mesh, 3 * t, &x, &y);
    edgeSplit(mesh, 3 * t, x, y);
    if (watch)
      showMesh(mesh);
  }
  return mesh;
}

static void demoDisplayMesh(void)
{
  Mesh *mesh = buildDemoMesh(0);
  int nNodes, nTriangles;
  const double *xy = getNodes(mesh, &nNodes);
  /* getTriangles() writes nTriangles, so it must run before 3*nTriangles is
   * read: two arguments of the same call are not sequenced. */
  const int *conn = getTriangles(mesh, &nTriangles);
  uint32_t *tri = toU32(conn, 3 * nTriangles);

  /* two nodal fields, interleaved the way project_display.h asks for:
   * fields[nfields*v + f] is field f of node v. We render field 1. */
  const uint32_t nfields = 2;
  double *fields = malloc((size_t)nNodes * nfields * sizeof(double));
  for (int v = 0; v < nNodes; v++)
  {
    double x = xy[2 * v], y = xy[2 * v + 1];
    fields[nfields * v] = sqrt((x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5));
    fields[nfields * v + 1] = sin(3 * PI * x) * cos(3 * PI * y);
  }

  printf("mesh: %d nodes, %d triangles -- "
         "drag/scroll to move, w f h n r to toggle, q to quit\n",
         nNodes,
         nTriangles);
  scene_add_mesh(
      xy, (uint32_t)nNodes, tri, (uint32_t)nTriangles, fields, nfields, 1);
  display();
  display_wait();

  free(fields);
  free(tri);
  freeMesh(mesh);
}

static void demoDisplayEdges(void)
{
  Mesh *mesh = buildDemoMesh(0);
  int nNodes, nHe;
  const double *xy = getNodes(mesh, &nNodes);
  const int *tri = getTriangles(mesh, NULL);
  const int *twin = getHeTwin(mesh, &nHe);

  /* the boundary of the mesh: every half-edge without a twin */
  uint32_t *edges = malloc(2 * (size_t)nHe * sizeof(uint32_t));
  uint32_t nEdges = 0;
  for (int h = 0; h < nHe; h++)
    if (twin[h] == HE_BOUNDARY)
    {
      edges[2 * nEdges] = (uint32_t)tri[h];
      edges[2 * nEdges + 1] = (uint32_t)tri[heNext(h)];
      nEdges++;
    }

  printf("boundary: %u edges out of %d half-edges\n", nEdges, nHe);
  scene_add_edges(xy, (uint32_t)nNodes, edges, nEdges);
  display();
  display_wait();

  free(edges);
  freeMesh(mesh);
}

/* ========================================================================== */
/*  Case 5 : step by step, the viewer inside the algorithm                    */
/* ========================================================================== */

/* Once the window is closed, display() returns at once: the rest of the
 * refinement simply runs without being shown. */
static void demoStepByStep(void)
{
  printf("space: next step (hold it to run), q: quit\n");
  Mesh *mesh = buildDemoMesh(1);
  printf("done: %d triangles\n", getNumTriangles(mesh));
  display_wait(); // the final mesh stays up until the window is closed
  freeMesh(mesh);
}

/* ========================================================================== */

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr,
            "usage: %s [1|2|3|4|5]\n"
            "  1  default triangle, edge split, face split\n"
            "  2  init from a node/triangle list\n"
            "  3  display a mesh, coloured by a nodal field\n"
            "  4  display the boundary edges of that mesh\n"
            "  5  watch that mesh being built, step by step\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  switch (atoi(argv[1]))
  {
  case 1:
    demoSplits();
    break;
  case 2:
    demoFromArrays();
    break;
  case 3:
    demoDisplayMesh();
    break;
  case 4:
    demoDisplayEdges();
    break;
  case 5:
    demoStepByStep();
    break;
  default:
    fprintf(stderr, "unknown case '%s', expected 1..5\n", argv[1]);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
