#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "student_triangulation.h"
#include "project_display.h"

/* This is a program to help you visualize part 2 of the project 
 * (alpha-shape + mesh adaptation). Your need a functioning triangulation for this step.
 *
 * You can use it as a baseline to test: 
 * - Your triangulation (and its speed)
 * - Your alpha-shape
 * - Your mesh adaptation
 *
 * It is called the "vortex in a box" and is a classical test-case for research in 
 * surface recovery and surface tracking. 
 *
 * It is simply a set of nodes (forming a disk) that is advected through a divergence-free 
 * vortex. You'll quickly see that the point set becomes distorted as time advances. 
 * Hence the mesh quality degrades as well: that's where mesh adaptation comes in !
 *
 * The goal is to keep the mesh's surface as constant as possible. 
 * The better your mesh-adaptation is, the longer you'll be able to keep the surface.
 */

#ifdef _WIN32
    #include <windows.h>
#else
    #include <errno.h>
#endif

void sleep_ms(int64_t milliseconds) {
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;

    // nanosleep can be interrupted by a signal; retry if so
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        continue;
    }
#endif
}

/* Advect the nodes using Range-Kutta 4
 *
 * @param nodes [in/out]: the nodes to advect. Size = 2*nnodes
 * @param nnodes [in]: the number of nodes
 * @param dt [in]: the time step
 *
 */
static void advect_node_rk4(double * const nodes, uint32_t nnodes, double dt, double t, double T) {

    for (uint32_t i = 0; i < nnodes; ++i) {
        double const x0 = nodes[2*i];
        double const y0 = nodes[2*i+1];
        double xk1 =  sinf(2.*M_PI*y0)*sinf(M_PI*x0)*sinf(M_PI*x0)*sinf(2.*M_PI*t/T);
        double yk1 = -sinf(2.*M_PI*x0)*sinf(M_PI*y0)*sinf(M_PI*y0)*sinf(2.*M_PI*t/T);

        double xk2 =  sinf(2.*M_PI*(y0 + 0.5*dt*yk1))*sinf(M_PI*(x0 + 0.5*dt*xk1))*sinf(M_PI*(x0 + 0.5*dt*xk1))*sinf(2.*M_PI*(t+0.5*dt)/T);
        double yk2 = -sinf(2.*M_PI*(x0 + 0.5*dt*xk1))*sinf(M_PI*(y0 + 0.5*dt*yk1))*sinf(M_PI*(y0 + 0.5*dt*yk1))*sinf(2.*M_PI*(t+0.5*dt)/T);

        double xk3 =  sinf(2.*M_PI*(y0 + 0.5*dt*yk2))*sinf(M_PI*(x0 + 0.5*dt*xk2))*sinf(M_PI*(x0 + 0.5*dt*xk2))*sinf(2.*M_PI*(t+0.5*dt)/T);
        double yk3 = -sinf(2.f*M_PI*(x0 + 0.5*dt*xk2))*sinf(M_PI*(y0 + 0.5*dt*yk2))*sinf(M_PI*(y0 + 0.5*dt*yk2))*sinf(2.*M_PI*(t+0.5*dt)/T);

        double xk4 =  sinf(2.*M_PI*(y0 + dt*yk3))*sinf(M_PI*(x0 + dt*xk3))*sinf(M_PI*(x0 + dt*xk3))*sinf(2.*M_PI*(t+dt)/T);
        double yk4 = -sinf(2.f*M_PI*(x0 + dt*xk3))*sinf(M_PI*(y0 + dt*yk3))*sinf(M_PI*(y0 + dt*yk3))*sinf(2.f*M_PI*(t+dt)/T);

        double const vx = (1.0/6.0)*(xk1 + 2.0*xk2 + 2.0*xk3 + xk4);
        double const vy = (1.0/6.0)*(yk1 + 2.0*yk2 + 2.0*yk3 + yk4);
        nodes[2*i] += dt*vx;
        nodes[2*i+1] += dt*vy;
    }
}

void print_usage(void) {
    printf(
            "USAGE: ./exec/2_ashape_adaptation [--help] -n <number of nodes> -T <final time> -dt <time step> -f <max fps> -i -d\n"
            "\t -n  INT    The number of nodes to generate \n"
            "\t -T  FLOAT  The total time (higher = more winding)\n"
            "\t -dt FLOAT  The time steps\n"
            "\t -f  FLOAT  The maximum frames per second\n"
            "\t -i  /      Display the initial mesh at every time step\n"
            "\t -d  /      Display the mesh prior to adaptation at every time step\n"
    );
}


int main(int argc, char** argv) {

    uint32_t n = 1000;  // The number of nodes
    double T = 5.0;  // The time for a cycle
    double dt = 0.05;  // A single time step
    double fps = 60.0; // Frame per second

    int show_initial_mesh = 0;
    int show_deleted_elems = 0;
    
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
            if (strcmp("-T", argv[cur_arg]) == 0) {
                T = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
            if (strcmp("-dt", argv[cur_arg]) == 0) {
                dt = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
            if (strcmp("-f", argv[cur_arg]) == 0) {
                fps = strtod(argv[cur_arg+1], NULL);
                cur_arg += 2;
                continue;
            }
        }

        if (strcmp("-i", argv[cur_arg]) == 0) {
            cur_arg += 1;
            show_initial_mesh = 1;
            continue;
        }

        if (strcmp("-d", argv[cur_arg]) == 0) {
            cur_arg += 1;
            show_deleted_elems = 1;
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

    // Generation of the nodes: 
    //      Fibonacci disk of radius r centered at (0.5, 0.75)
    double * nodes = malloc(2*n*sizeof(*nodes));
    double const phi = (1.0 + sqrt(5.0))/2.0;
    for (uint32_t i = 0; i < n; ++i) {
        double const theta = i * 2.0 * M_PI * (1.0 - 1.0/phi);
        double const r = 0.15 * sqrt((i + 0.5)/n);

        nodes[2*i] = 0.5 + r * cos(theta);
        nodes[2*i+1] = 0.75 + r * sin(theta);
    }

    mesh_st mesh = {.nodes = nodes, 
                    .elems = NULL, 
                    .size_field = NULL,
                    .nnodes = n, 
                    .nelems = 0};

    uint8_t* to_delete = NULL;

    struct timespec t0, t1;
    int display_continue = 1;
    for (double t = 0.0; t < T && display_continue; t += dt) {
        timespec_get(&t0, TIME_UTC);

        // ======================= Compute the initial mesh ===================
        mesh_triangulate(&mesh);

        if (show_initial_mesh) {
            scene_add_mesh(mesh.nodes, mesh.nnodes, mesh.elems, mesh.nelems, NULL, 0, 0);
            display_continue = display();
            if (!display_continue) {
                break;
            }
        }

        // ====================== Compute the alpha-shape =====================
        // Choose which elements are deleted (keep all by default)
        uint8_t* to_delete_realloc = realloc(to_delete, sizeof(*to_delete)*mesh.nelems);
        to_delete = to_delete_realloc;
        memset(to_delete, 0, sizeof(*to_delete)*mesh.nelems);

        mesh_delete_elements(&mesh, to_delete);

        if (show_deleted_elems) {
            scene_add_mesh(mesh.nodes, mesh.nnodes, mesh.elems, mesh.nelems, NULL, 0, 0);
            display_continue = display();
            if (!display_continue) {
                break;
            }
        }

        // ====================== Adapt the mesh ==============================
        mesh_adapt(&mesh, NULL, 0);

        // By default only show the final adapted mesh
        scene_add_mesh(mesh.nodes, mesh.nnodes, mesh.elems, mesh.nelems, NULL, 0, 0);
        display_continue = display();
        if (!display_continue) {
            break;
        }

        advect_node_rk4(nodes, n, dt, t, T);

        timespec_get(&t1, TIME_UTC);
        double const ft = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)*1e-9;
        // Frame was generated fast: wait a bit to ensure target fps)
        if (ft < 1.0/fps) {
            sleep_ms(1000.0 * (1.0/fps - ft));         
        }
    }
    display_wait();

    free(to_delete);
    free(nodes);
    free(mesh.elems);
    return EXIT_SUCCESS;
}
