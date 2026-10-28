"""
Test case of the fluid solver: a single time step on a hard-coded mesh.

    python3 test.py      (after compiling the project in code/build)

A column of water at rest, 1 m wide and 0.5 m high, in a container: the
bottom and the sides are walls, the top is a free surface. Under gravity,
the water should stay at rest, with the hydrostatic pressure
p = density * g * (0.5 - y).

Only needs numpy. If matplotlib is installed, the pressure is also plotted.
"""
import numpy as np

from lib import *


# The nodes, on a 5 x 3 grid:
#
#   10 -- 11 -- 12 -- 13 -- 14     y = 0.5   (free surface)
#    |  /  |  /  |  /  |  /  |
#    5 --  6 --  7 --  8 --  9     y = 0.25
#    |  /  |  /  |  /  |  /  |
#    0 --  1 --  2 --  3 --  4     y = 0     (bottom wall)
nodes = np.array([
    [0.00, 0.00], [0.25, 0.00], [0.50, 0.00], [0.75, 0.00], [1.00, 0.00],
    [0.00, 0.25], [0.25, 0.25], [0.50, 0.25], [0.75, 0.25], [1.00, 0.25],
    [0.00, 0.50], [0.25, 0.50], [0.50, 0.50], [0.75, 0.50], [1.00, 0.50],
])

# The triangles, counter-clockwise, two per square of the grid
elems = np.array([
    [0, 1, 6],   [0, 6, 5],   [1, 2, 7],   [1, 7, 6],
    [2, 3, 8],   [2, 8, 7],   [3, 4, 9],   [3, 9, 8],
    [5, 6, 11],  [5, 11, 10], [6, 7, 12],  [6, 12, 11],
    [7, 8, 13],  [7, 13, 12], [8, 9, 14],  [8, 14, 13],
])

# ALL the edges on the boundary of the mesh: walls and free surface
bnd_edges = np.array([
    [0, 1], [1, 2], [2, 3], [3, 4],             # bottom
    [4, 9], [9, 14],                            # right side
    [14, 13], [13, 12], [12, 11], [11, 10],     # top
    [10, 5], [5, 0],                            # left side
])

# The type of each node. The top corners touch the walls: they are WALL nodes
node_type = np.full(len(nodes), WALL, dtype=np.int32)
node_type[[6, 7, 8]] = BULK
node_type[[11, 12, 13]] = FREE_SURFACE

props = PhysicalProps(density=1000.0, viscosity=1e-3, gravity=(0.0, -9.81), sigma=0.072, dt=0.01)
uvp = np.zeros(3 * len(nodes))                 # (u, v, p) per node, at rest

solver = FluidSolver()
status = solver.solve(nodes, elems, node_type, bnd_edges, props, uvp)
if not status.success:
    raise RuntimeError("the solver failed, see the message above")

u, v, p = uvp[0::3], uvp[1::3], uvp[2::3]
p_exact = props.density * -props.gravity[1] * (0.5 - nodes[:, 1])

# A few prints to check the solution on this easy example :-) 
print(f"solved {status.n_dofs} unknowns in {status.solve_time:.3f} s, residual {status.residual_norm:.1e}")
print(f"max |u| = {np.max(np.hypot(u, v)):.2e} m/s (expected 0)")
print(f"max |p - p_hydrostatic| = {np.max(np.abs(p - p_exact)):.2e} Pa (p at the bottom: {p_exact.max():.0f} Pa)")
print(f"\n{'node':>4} {'type':>12} {'x':>5} {'y':>5} {'u':>10} {'v':>10} {'p':>8} {'p exact':>8}")
names = {BULK: "bulk", FREE_SURFACE: "free surface", WALL: "wall"}
for i in range(len(nodes)):
    print(f"{i:>4} {names[node_type[i]]:>12} {nodes[i, 0]:>5.2f} {nodes[i, 1]:>5.2f} "
          f"{u[i]:>10.2e} {v[i]:>10.2e} {p[i]:>8.1f} {p_exact[i]:>8.1f}")

# small plot to see the pressure :-) 
# ----------------------------------
import matplotlib.pyplot as plt
fig, ax = plt.subplots()
pc = ax.tripcolor(nodes[:, 0], nodes[:, 1], elems, p, shading="gouraud")
ax.triplot(nodes[:, 0], nodes[:, 1], elems, lw=0.5, color="k")
for kind, marker in [(WALL, "s"), (FREE_SURFACE, "o"), (BULK, "x")]:
    ax.plot(*nodes[node_type == kind].T, marker, color="k", label=names[kind], ls="none")
fig.colorbar(pc, label="pressure [Pa]")
ax.set_aspect("equal")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=3)
ax.set_title("pressure after one time step")
plt.show()
