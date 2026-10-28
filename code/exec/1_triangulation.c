#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "student_triangulation.h"
#include "project_display.h"

/* This is a program to help you visualize the first part of the project (triangulation). 
 * You can use it as a baseline to test your triangulation (and its speed).
 *
 * To give you an idea: 
 *    scipy.spatial.Delaunay takes 4.7s for 1 million uniform 2D points on my machine.
 *    An ok-ish C implementation can do 2s.
 *    A good C implementation can reach 0.5s.
 *
 * You can choose between different point set distributions:
 * 1. point on a perfect circle 
 * 2. points inside a disk
 * 3. uniform sampling in a square 
 * 4. perfect grid
 * 5. points along a line with a single point out of line
 * 6. points along a jagged line
 *
 * Some of these point set are really stress testing the triangulation and might 
 * require you to really think about the edge cases. 
 *
 * I advise you to make sure your triangulation works on the sets 1-4 and come 
 * back to the 5 and 6 only when you've finished the rest of the project.
 *
 * Note also that for 1, 4, 5, 6 you can compute a priori the exact number 
 * of triangles and boundary edges. This can help you make sure it is correct.
 *
 * The program is designed to show you the complete triangulation at once, but 
 * you can tweak it to show each point insertion once you've implemented the functions
 */

void print_usage(void) {
    printf(
            "USAGE: ./exec/1_triangulation [--help] -n <number of nodes> --dist <circle|disk|uniform|grid|line|jagged> --eps <epsilon value>\n"
            "\t -n  INT       The number of nodes to generate \n"
            "\t --dist STRING The point distribution\n"
            "\t --eps FLOAT   The epsilon value for 'line' and 'jagged' distributions"
    );
}

typedef enum {
    DIST_CIRCLE = 0,
    DIST_DISK,
    DIST_UNIFORM,
    DIST_GRID,
    DIST_LINE,
    DIST_JAGGED,
    DIST_NUMBER
} distribution_et;

static const char* const distribution_names[DIST_NUMBER] = {
    "circle", "disk", "uniform", "grid", "line", "jagged line"
};

/* Generates points on an exact circle 
 * Difficulty level: Hard, but should work
 */
void generate_circle(uint32_t * n, double * nodes) {
    for (uint32_t i = 0; i < *n; ++i){
        double const theta = i * 2.0 * M_PI / (*n);
        nodes[2*i] = cos(theta);
        nodes[2*i+1] = sin(theta);
    }
}

/* Generates points inside a disk 
 * Difficulty level: Medium 
 */
void generate_disk(uint32_t * n, double * nodes) {
    double const phi = (1.0 + sqrt(5.0))/2.0;
    for (uint32_t i = 0; i < *n; ++i) {
        double const theta = i * 2.0 * M_PI * (1.0 - 1.0/phi);
        double const r = sqrt((i + 0.5)/(*n));

        nodes[2*i] = r * cos(theta);
        nodes[2*i+1] = r * sin(theta);
    }
}

/* Sample points uniformly inside [0, 1]^2 
 * Difficulty level: Easy
 */
void generate_uniform(uint32_t * n, double * nodes) {
    for (uint32_t i = 0; i < *n; ++i) {
        nodes[2*i] = (double) rand() / RAND_MAX;
        nodes[2*i+1] = (double) rand() /RAND_MAX;
    }
}

/* Generate points on an exact grid.
 * Difficulty level: Hard, but should work
 */
void generate_grid(uint32_t * n, double * nodes) {
    uint32_t side = (uint32_t) floor(sqrt(*n));
    *n = side * side;
    
    for (uint32_t i = 0; i < side; ++i){
        for (uint32_t j = 0; j < side; ++j){
            nodes[2*(side*i + j)] = i;
            nodes[2*(side*i + j)+1] = j;
        }
    }
}

/* Generate n-1 points along a line and 1 point eps above the line.
 *
 * Increase eps or lower the number of points to make the test easier
 *
 * Difficulty level: Very Very hard (it's ok if it doesn't work on it)
 */
void generate_line(uint32_t * n, double * nodes, double const eps) {
    for (uint32_t i = 0; i < *n - 1; ++i){
        nodes[2*i] = i;
        nodes[2*i+1] = 0.0;
    }
    nodes[2*(*n - 1)] = 0.0;
    nodes[2*(*n - 1) + 1] = eps;
}

/* Generate points along an epsilon jagged line (1 point epsilon up, 1 point epsilon down 
 * and so on)
 *
 * Increase eps to make the test easier
 *
 * Difficulty level: Very hard (it's ok if it doesn't work on it)
 */
void generate_jagged_line(uint32_t *n, double * nodes, double const eps) {
    for (uint32_t i = 0; i < *n; ++i){
        nodes[2*i] = i;
        nodes[2*i+1] = (i % 2) ? -eps : eps;
    }
}

int main(int argc, char** argv) {

    uint32_t n = 1000;  // The number of nodes
    distribution_et dist = DIST_UNIFORM;  // Uniform distribution by default
    double eps = 1e-8;

    if (argc <= 1) {
        print_usage();
        return EXIT_FAILURE;
    }

    int cur_arg = 1;
    while (cur_arg < argc) {

        // There is a value to read 
        if (cur_arg + 1 < argc) {
            if (strcmp("-n", argv[cur_arg]) == 0) {
                n = atoi(argv[cur_arg+1]);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--eps", argv[cur_arg]) == 0) {
                eps = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--dist", argv[cur_arg]) == 0) {
                if (strcmp("circle", argv[cur_arg+1]) == 0) {
                    dist = DIST_CIRCLE;
                    cur_arg += 2;
                    continue;
                }
                if (strcmp("disk", argv[cur_arg+1]) == 0) {
                    dist = DIST_DISK;
                    cur_arg += 2;
                    continue;
                }
                if (strcmp("uniform", argv[cur_arg+1]) == 0) {
                    dist = DIST_UNIFORM;
                    cur_arg += 2;
                    continue;
                }
                if (strcmp("grid", argv[cur_arg+1]) == 0) {
                    dist = DIST_GRID;
                    cur_arg += 2;
                    continue;
                }
                if (strcmp("line", argv[cur_arg+1]) == 0) {
                    dist = DIST_LINE;
                    cur_arg += 2;
                    continue;
                }
                if (strcmp("jagged", argv[cur_arg+1]) == 0) {
                    dist = DIST_JAGGED;
                    cur_arg += 2;
                    continue;
                }
                printf("Non-supported distribution type '%s'. Please choose from one of the following: \n"
                        "\t circle : points along a circle (hard)\n"
                        "\t disk : points inside a disk (medium)\n"
                        "\t uniform : points sampled uniformly (easy)\n"
                        "\t grid : points in a regular grid (hard)\n"
                        "\t line : points in a line (very very hard)\n"
                        "\t jagged : points in a jagged line (very very hard)\n", 
                        argv[cur_arg+1]
                    );
                return EXIT_FAILURE;
            }

        }

        // There is no value to read
        print_usage();
        return EXIT_FAILURE;
    }

    printf(
        "Commands (AZERTY Layout):\n"
        "\tMouse Drag:   move\n"
        "\tMouse Scroll: zoom\n"
        "\tr:            reset position\n"
        "\tz:            Show/Hide mesh\n"
        "\tf:            Show/Hide field\n"
        "\th:            Show/hide half-edge and half-edge indices\n"
        "\tn:            Show/hide node indices\n"
        "\ts:            Advance a single step\n"
        "\tspace:        Play/pause\n"
    );
    
    srand(42); // seed the random number generator to always get the same nodes

    // Generation of the nodes: 
    double * nodes = malloc(2*n*sizeof(*nodes));
    switch (dist) {
        case DIST_CIRCLE:
            generate_circle(&n, nodes);
            break;
        case DIST_DISK:
            generate_disk(&n, nodes);
            break;
        case DIST_UNIFORM:
            generate_uniform(&n, nodes);
            break;
        case DIST_GRID:
            generate_grid(&n, nodes);
            break;
        case DIST_LINE:
            generate_line(&n, nodes, eps);
            break;
        case DIST_JAGGED:
            generate_jagged_line(&n, nodes, eps);
            break;
    }

    printf("Generated %u nodes with '%s' distribution\n", n, distribution_names[dist]);

    mesh_st mesh = {.nodes = nodes, 
                    .elems = NULL, 
                    .size_field = NULL,
                    .nnodes = n, 
                    .nelems = 0};

    struct timespec t0, t1;
    timespec_get(&t0, TIME_UTC);
    mesh_triangulate(&mesh);
    timespec_get(&t1, TIME_UTC);
    double const t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)*1e-9;

    printf("Time taken to compute the mesh: %.5f s\n", t);

    scene_add_mesh(mesh.nodes, mesh.nnodes, mesh.elems, mesh.nelems, NULL, 0, 0);
    display();
    display_wait();

    free(nodes);
    free(mesh.elems);

    return EXIT_SUCCESS;
}
