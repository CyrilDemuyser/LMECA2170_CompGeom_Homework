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
 * - The node to curve distance queries
 */

void print_usage(void) {
    printf(
            "USAGE: ./exec/3_1_distance_queries [--help] --nb <number of nodes> --nq <number of queries> --bs <bucket size> --depth <maximum depth> --boundary <line|circle> --one-by-one\n"
            "\t --nb         INT     The number of boundary nodes to generate\n"
            "\t --nq         INT     The number of query nodes to generate \n"
            "\t --bs         INT     The maximum bucket size of the quadtree\n"
            "\t --depth      INT     The maximum depht of the quadtree\n"
            "\t --boundary   STRING  The type of boundary for which to query distances\n"
            "\t --one-by-one /       Show the queries one by one instead of all at once\n"
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
        nodes[2*i] = (double) i / nb - 0.5;
        nodes[2*i+1] = 0.0;
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

int main(int argc, char** argv) {

    uint32_t nb = 1000;  // The number of boundary nodes
    uint32_t nq = 1000;  // The number of query nodes
    uint32_t bucket_size = 8; // The maximum bucket size of the quadtree
    uint32_t max_depth = 32;  // The maximum depth of the quadtree
    bnd_type_et bnd_type = BND_CIRCLE;
    int one_by_one = 0; // Display the queries one by one

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

        if (strcmp("--one-by-one", argv[cur_arg]) == 0) {
            one_by_one = 1;
            cur_arg += 1;
            continue;
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

    double * query_nodes = malloc(2*sizeof(*query_nodes)*nq);
    for (uint32_t i = 0; i < nq; ++i){
        query_nodes[2*i] = 4*(((double) rand() / RAND_MAX) - 0.5);
        query_nodes[2*i+1] = 4*(((double) rand() / RAND_MAX) - 0.5);
    }


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
    
    if (!one_by_one) {
        // Show the queries in bulk, with timing
        double* neighbor_nodes = malloc(4*sizeof(*neighbor_nodes)*nq);
        uint32_t* neighbor_edges = malloc(2*sizeof(*neighbor_edges)*nq);

        timespec_get(&t0, TIME_UTC);
        for (uint32_t qi = 0; qi < nq; qi++){
            neirest_edge_st const q = quadtree_find_closest_edge(qt, query_nodes[2*qi], query_nodes[2*qi+1]);

            neighbor_nodes[4*qi + 0] = query_nodes[2*qi];
            neighbor_nodes[4*qi + 1] = query_nodes[2*qi+1];
            neighbor_nodes[4*qi + 2] = q.p[0];
            neighbor_nodes[4*qi + 3] = q.p[1];

            neighbor_edges[2*qi + 0] = 2*qi + 0;
            neighbor_edges[2*qi + 1] = 2*qi + 1;
        }
        timespec_get(&t1, TIME_UTC);
        printf("Time to query all distances: %.5f\n", 
                (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)*1e-9);
        
        scene_add_edges(bnd.nodes, bnd.nnodes, bnd.edges, bnd.nedges); // Boundary
        scene_add_edges(neighbor_nodes, 2*nq, neighbor_edges, nq); // neirest node segment
        scene_add_nodes(query_nodes, nq);
        display();
        display_wait();

        free(neighbor_edges);
        free(neighbor_nodes);
    } else {
        // Display each query one by one
        double neighbor_nodes[4] = {0};
        uint32_t neighbor_edges[2] = {0};

        for (uint32_t qi = 0; qi < nq; qi++){
            timespec_get(&t0, TIME_UTC);
            neirest_edge_st const q = quadtree_find_closest_edge(qt, query_nodes[2*qi], query_nodes[2*qi+1]);
            timespec_get(&t1, TIME_UTC);
            printf("Time to query single distances: %.5f\n", 
                    (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)*1e-9);

            neighbor_nodes[0] = query_nodes[2*qi];
            neighbor_nodes[1] = query_nodes[2*qi+1];
            neighbor_nodes[2] = q.p[0];
            neighbor_nodes[3] = q.p[1];

            neighbor_edges[0] = 0;
            neighbor_edges[1] = 1;

            scene_add_edges(bnd.nodes, bnd.nnodes, bnd.edges, bnd.nedges); // Boundary
            scene_add_edges(neighbor_nodes, 2, neighbor_edges, 1); // neirest node segment
            scene_add_nodes(query_nodes, nq);
            if (!display())
                break;
        }
        display_wait();
        
    }

    quadtree_destroy(qt);
    free(query_nodes);
    free(bnd.nodes);
    free(bnd.edges);

    return EXIT_SUCCESS;
}
