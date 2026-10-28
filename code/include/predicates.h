/* Public declarations for Jonathan Richard Shewchuk's predicates.c.
 * Original source: https://www.cs.cmu.edu/~quake/robust.html (public domain).
 * This copy uses REAL = double. Call exactinit() once before using predicates.
 * Points are arrays {x, y}. The original API takes double* but does not modify
 * these input points. Use only the sign of the result for decisions.
 * These functions accept finite coordinates; infinity needs your own wrapper.
 */
#ifndef STUDENT_PREDICATES_H
#define STUDENT_PREDICATES_H

#ifdef __cplusplus
extern "C" {
#endif

void exactinit(void);

/* Positive: CCW; negative: CW; zero: collinear. */
double orient2d(double *a, double *b, double *c);

/* For CCW (a,b,c): positive inside, negative outside, zero on circumcircle.
 * Reversing (a,b,c) reverses the sign. Collinear (a,b,c) is not a valid circle.
 */
double incircle(double *a, double *b, double *c, double *d);

#ifdef __cplusplus
}
#endif
#endif /* STUDENT_PREDICATES_H */
