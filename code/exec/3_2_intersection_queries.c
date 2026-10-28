#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "project_display.h"
#include "student_spatial_search.h"
#include "student_triangulation.h"

/* This is a program to help you visualize part 3 of the project (quadtree queries)
 *
 * You can use it as a baseline to test: 
 * - The quadtree construction speed 
 * - The quadtree parameters 
 * - The intersection queries
 */

void print_usage(void) {
    printf(
            "USAGE: ./exec/3_2_intersection_queries [--help] --nb <number of nodes> --nq <number of queries> --bs <bucket size> --depth <maximum depth> --boundary <line|circle>\n"
            "\t --nb         INT     The number of boundary nodes to generate\n"
            "\t --nq         INT     The number of query nodes to generate \n"
            "\t --bs         INT     The maximum bucket size of the quadtree\n"
            "\t --depth      INT     The maximum depht of the quadtree\n"
            "\t --boundary   STRING  The type of boundary for which to query distances\n"
    );
}

typedef enum {
    BND_LINE = 0,
    BND_CIRCLE,
    BND_NUMBER
} bnd_type_et;

static char const * const bnd_name[BND_NUMBER] = {
    "line", "circle"
};

void generate_line(boundary_st * const bnd, uint32_t nb) {
    if (!bnd) return;
    if (bnd->nodes && bnd->nnodes) {
        free(bnd->nodes);
        bnd->nodes = NULL;
        bnd->nnodes = 0;
    }
    if (bnd->edges && bnd->nedges) {
        free(bnd->edges);
        bnd->edges = NULL;
        bnd->nedges = 0;
    }

    double * nodes = malloc(2*sizeof(*nodes)*nb);
    uint32_t ne = nb-1;
    uint32_t * edges = malloc(2*sizeof(*edges)*ne);

    // Generate a line from 0 to 1 discretized with nb nodes and ne segments
    for (uint32_t i = 0; i < nb; ++i){
        nodes[2*i] = 2.0 * ((double) i / nb - 0.5);
        nodes[2*i+1] = 2.0 * ((double) i / nb - 0.5);
    }
    for (uint32_t i = 0; i < ne; ++i){
        edges[2*i] = i;
        edges[2*i+1] = i+1;
    }

    bnd->nodes = nodes;
    bnd->nnodes = nb;
    bnd->edges = edges;
    bnd->nedges = ne;
}

void generate_circle(boundary_st * const bnd, uint32_t nb) {
    if (!bnd) return;
    if (bnd->nodes && bnd->nnodes) {
        free(bnd->nodes);
        bnd->nodes = NULL;
        bnd->nnodes = 0;
    }
    if (bnd->edges && bnd->nedges) {
        free(bnd->edges);
        bnd->edges = NULL;
        bnd->nedges = 0;
    }

    double * nodes = malloc(2*sizeof(*nodes)*nb);
    uint32_t ne = nb;
    uint32_t * edges = malloc(2*sizeof(*edges)*ne);

    // Generate a circle discretized with nb nodes and ne segments
    for (uint32_t i = 0; i < nb; ++i){
        double const theta = 2.0 * M_PI * i / nb;
        nodes[2*i]   = cos(theta);
        nodes[2*i+1] = sin(theta);
    }
    for (uint32_t i = 0; i < ne; ++i){
        edges[2*i] = i;
        edges[2*i+1] = (i+1)%nb;
    }

    bnd->nodes = nodes;
    bnd->nnodes = nb;
    bnd->edges = edges;
    bnd->nedges = ne;
}

typedef struct {
    bbox_st bb;
    double * nodes;
    uint32_t nnodes;
} hilbert_data_st;
static hilbert_data_st hb = {0};

uint64_t hilbert_code(uint32_t idx) {
    double const* nodes = hb.nodes;

    // Enlarge slightly the bounds to not have floating point issues 
    bbox_st const bb = {
        .min={hb.bb.min[0]-0.1, hb.bb.min[1]-0.1},
        .max={hb.bb.max[0]+0.1, hb.bb.max[1]+0.1}
    };
    double const px = nodes[2*idx + 0];
    double const py = nodes[2*idx + 1];

    uint32_t x = (px - bb.min[0]) / (bb.max[0] - bb.min[0]) * (1 << 31);
    uint32_t y = (py - bb.min[1]) / (bb.max[1] - bb.min[1]) * (1 << 31);

    // Compute Hilbert code
    uint64_t d = 0;
 
    for (uint32_t s = 0x80000000u; s > 0; s >>= 1) {
        uint32_t rx = (x & s) ? 1u : 0u;
        uint32_t ry = (y & s) ? 1u : 0u;
 
        /* Quadrant index in Hilbert order: (0,0)->0 (0,1)->1 (1,1)->2 (1,0)->3 */
        d += (uint64_t)s * (uint64_t)s * (uint64_t)((3u * rx) ^ ry);
 
        /* Rotate/flip the quadrant so the sub-curve has the right orientation. */
        if (ry == 0) {
            if (rx == 1) {
                x = ~x;     /* equivalent to (2^32 - 1) - x */
                y = ~y;
            }
            uint32_t tmp = x; x = y; y = tmp;
        }
    }
    return d;
}

int hilbert_compare(void const* a, void const* b) {
    uint32_t const p = *((uint32_t*) a);
    uint32_t const q = *((uint32_t*) b);

    return hilbert_code(p) < hilbert_code(q);
}

int main(int argc, char** argv) {

    uint32_t nb = 1000;  // The number of boundary nodes
    uint32_t nq = 1000;  // The number of query nodes
    uint32_t bucket_size = 8; // The maximum bucket size of the quadtree
    uint32_t max_depth = 32;  // The maximum depth of the quadtree
    bnd_type_et bnd_type = BND_CIRCLE;

    if (argc <= 1) {
        print_usage();
        return EXIT_FAILURE;
    }

    int cur_arg = 1;
    while (cur_arg < argc) {

        // There is a value to read 
        if (cur_arg + 1 < argc) {
            if (strcmp("--nb", argv[cur_arg]) == 0) {
                nb = atoi(argv[cur_arg+1]);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--nq", argv[cur_arg]) == 0) {
                nq = atoi(argv[cur_arg+1]);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--bs", argv[cur_arg]) == 0) {
                bucket_size = atoi(argv[cur_arg+1]);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--depth", argv[cur_arg]) == 0) {
                max_depth = atoi(argv[cur_arg+1]);
                cur_arg += 2;
                continue;
            }
            if (strcmp("--boundary", argv[cur_arg]) == 0) {
                if (strcmp("circle", argv[cur_arg+1]) == 0) {
                    bnd_type = BND_CIRCLE;
                    cur_arg += 2;
                    continue;
                }
                if (strcmp("line", argv[cur_arg+1]) == 0) {
                    bnd_type = BND_LINE;
                    cur_arg += 2;
                    continue;
                }
                printf("Non-supported boundary type '%s'. Please choose from one of the following: \n"
                        "\t circle : boundary is a circle\n"
                        "\t line : boundary is a straight line\n",
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

    boundary_st bnd = {0};
    switch (bnd_type) {
        case BND_CIRCLE:
            generate_circle(&bnd, nb);
            break;
        case BND_LINE:
            generate_line(&bnd, nb);
            break;
    }

    // Generate nq nodes
    double * query_nodes = malloc(2*sizeof(*query_nodes)*nq);
    uint32_t * qn_idx = malloc(sizeof(*qn_idx)*nq);
    for (uint32_t i = 0; i < nq; ++i){
        query_nodes[2*i] = 4*(((double) rand() / RAND_MAX) - 0.5);
        query_nodes[2*i+1] = 4*(((double) rand() / RAND_MAX) - 0.5);
        qn_idx[i] = i;
    }

    // Sort the nodes according to a Hilbert curve and create the edges
    // do not actually reorder the nodes, just compute a permutation array
    // We use globals for this, it's not elegant and I don't like it but it works
    hb.nodes = query_nodes;
    hb.nnodes = nq;
    hb.bb = (bbox_st) {{-2.0, -2.0}, {2.0, 2.0}};
    qsort(qn_idx, nq, sizeof(*qn_idx), hilbert_compare);

    uint32_t const nqe = nq-1; // number of query edges
    uint32_t * query_edges = malloc(2*sizeof(*query_edges)*nqe);
    for (uint32_t i = 0; i < nqe; ++i){
        query_edges[2*i] = qn_idx[i];
        query_edges[2*i+1] = qn_idx[i+1];
    }
    free(qn_idx);


    // ==================== Construct the quadtree ===================
    struct timespec t0, t1;
    timespec_get(&t0, TIME_UTC);
    quadtree_st * qt = quadtree_new(bucket_size, max_depth);
    quadtree_set_edges(qt, bnd.nodes, bnd.edges, bnd.nnodes, bnd.nedges);
    quadtree_build(qt);
    timespec_get(&t1, TIME_UTC);
    printf("Time to construct the quadtree: %.5f\n", 
            (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)*1e-9);

    
    // ====================== Perform the queries ====================
    
    // Allocate way too much memory to store intersections, just so we don't have to 
    // reallocate
    uint32_t* intersecting_edge = malloc(2*sizeof(*intersecting_edge)*nqe);
    uint32_t* non_intersecting_edge = malloc(2*sizeof(*non_intersecting_edge)*nqe);

    timespec_get(&t0, TIME_UTC);
    uint32_t n_intersections = 0;
    uint32_t n_non_intersections = 0;
    for (uint32_t qi = 0; qi < nqe; qi++){
        uint32_t const s = query_edges[2*qi];
        uint32_t const t = query_edges[2*qi+1];
        intersection_st const intersection = quadtree_find_intersecting_edge(qt, query_nodes, s, t);
        if (intersection.intersects) {
            intersecting_edge[2*n_intersections] = s;
            intersecting_edge[2*n_intersections+1] = t;
            n_intersections++;
        } else {
            non_intersecting_edge[2*n_non_intersections] = s;
            non_intersecting_edge[2*n_non_intersections+1] = t;
            n_non_intersections++;
        }
    }
    timespec_get(&t1, TIME_UTC);
    printf("Time to query all intersections: %.5f\n", 
            (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)*1e-9);
    
    // Alternate between drawing the intersecting and non-intersecting edges
    int draw_intersection = 0;
    do {
        scene_add_edges(bnd.nodes, bnd.nnodes, bnd.edges, bnd.nedges); // Boundary
        if (draw_intersection) {
            printf("Displaying the intersecting edges\n");
            scene_add_edges(query_nodes, nq, intersecting_edge, n_intersections); // Intersecting edges

            // You could additionally draw the point of intersection with scene_add_nodes
        } else {
            printf("Displaying the non-intersecting edges\n");
            scene_add_edges(query_nodes, nq, non_intersecting_edge, n_non_intersections); // Non-intersecting edges
        }
        draw_intersection = !draw_intersection;
    } while (display());

    free(intersecting_edge);
    free(non_intersecting_edge);

    quadtree_destroy(qt);
    free(query_nodes);
    free(query_edges);
    free(bnd.nodes);
    free(bnd.edges);

    return EXIT_SUCCESS;
}
