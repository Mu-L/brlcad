/*                          P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup poly */
/** @{ */
/** @file libbn/poly.c
 *
 * Library for dealing with polynomials.
 *
 */

#include "common.h"

#include <stdlib.h>  /* for abs */
#include <stdio.h>
#include <math.h>

#include "bu/log.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "bn/poly.h"


#define CUBEROOT(a)	(((a) > 0.0) ? pow(a, THIRD) : -pow(-(a), THIRD))


static const fastf_t THIRD = 1.0 / 3.0;
static const fastf_t TWENTYSEVENTH = 1.0 / 27.0;
static const struct bn_poly bn_Zero_poly = { BN_POLY_MAGIC, 0, {0.0} };


struct bn_poly *
bn_poly_mul(register struct bn_poly *product, register const struct bn_poly *m1, register const struct bn_poly *m2)
{
    if (m1->dgr == 1 && m2->dgr == 1) {
	product->dgr = 2;
	product->cf[0] = m1->cf[0] * m2->cf[0];
	product->cf[1] = m1->cf[0] * m2->cf[1] +
	    m1->cf[1] * m2->cf[0];
	product->cf[2] = m1->cf[1] * m2->cf[1];
	return product;
    }
    if (m1->dgr == 2 && m2->dgr == 2) {
	product->dgr = 4;
	product->cf[0] = m1->cf[0] * m2->cf[0];
	product->cf[1] = m1->cf[0] * m2->cf[1] +
	    m1->cf[1] * m2->cf[0];
	product->cf[2] = m1->cf[0] * m2->cf[2] +
	    m1->cf[1] * m2->cf[1] +
	    m1->cf[2] * m2->cf[0];
	product->cf[3] = m1->cf[1] * m2->cf[2] +
	    m1->cf[2] * m2->cf[1];
	product->cf[4] = m1->cf[2] * m2->cf[2];
	return product;
    }

    /* Not one of the common (or easy) cases. */
    {
	register size_t ct1, ct2;

	*product = bn_Zero_poly;

	/* If the degree of the product will be larger than the
	 * maximum size allowed in "polyno.h", then return a null
	 * pointer to indicate failure.
	 */
	if ((product->dgr = m1->dgr + m2->dgr) > BN_MAX_POLY_DEGREE)
	    return BN_POLY_NULL;

	for (ct1=0; ct1 <= m1->dgr; ++ct1) {
	    for (ct2=0; ct2 <= m2->dgr; ++ct2) {
		product->cf[ct1+ct2] +=
		    m1->cf[ct1] * m2->cf[ct2];
	    }
	}
    }
    return product;
}


struct bn_poly *
bn_poly_scale(register struct bn_poly *eqn, double factor)
{
    register size_t cnt;

    for (cnt=0; cnt <= eqn->dgr; ++cnt) {
	eqn->cf[cnt] *= factor;
    }
    return eqn;
}


struct bn_poly *
bn_poly_add(register struct bn_poly *sum, register const struct bn_poly *poly1, register const struct bn_poly *poly2)
{
    struct bn_poly tmp;
    register size_t i, offset;

    offset = labs((long)poly1->dgr - (long)poly2->dgr);

    tmp = bn_Zero_poly;

    if (poly1->dgr >= poly2->dgr) {
	*sum = *poly1;
	for (i=0; i <= poly2->dgr; ++i) {
	    tmp.cf[i+offset] = poly2->cf[i];
	}
    } else {
	*sum = *poly2;
	for (i=0; i <= poly1->dgr; ++i) {
	    tmp.cf[i+offset] = poly1->cf[i];
	}
    }

    for (i=0; i <= sum->dgr; ++i) {
	sum->cf[i] += tmp.cf[i];
    }
    return sum;
}


struct bn_poly *
bn_poly_sub(register struct bn_poly *diff, register const struct bn_poly *poly1, register const struct bn_poly *poly2)
{
    struct bn_poly tmp;
    register size_t i, offset;

    offset = labs((long)poly1->dgr - (long)poly2->dgr);

    *diff = bn_Zero_poly;
    tmp = bn_Zero_poly;

    if (poly1->dgr >= poly2->dgr) {
	*diff = *poly1;
	for (i=0; i <= poly2->dgr; ++i) {
	    tmp.cf[i+offset] = poly2->cf[i];
	}
    } else {
	diff->dgr = poly2->dgr;
	for (i=0; i <= poly1->dgr; ++i) {
	    diff->cf[i+offset] = poly1->cf[i];
	}
	tmp = *poly2;
    }

    for (i=0; i <= diff->dgr; ++i) {
	diff->cf[i] -= tmp.cf[i];
    }
    return diff;
}


void
bn_poly_synthetic_division(struct bn_poly *quo, struct bn_poly *rem, const struct bn_poly *dvdend, const struct bn_poly *dvsor)
{
    register size_t divisor;
    register size_t n;

    *quo = *dvdend; /* struct copy */
    *rem = bn_Zero_poly; /* struct copy */

    if (dvsor->dgr > dvdend->dgr) {
	quo->dgr = -1;
    } else {
	quo->dgr = dvdend->dgr - dvsor->dgr;
    }
    if ((rem->dgr = dvsor->dgr - 1) > dvdend->dgr)
	rem->dgr = dvdend->dgr;

    for (n=0; n <= quo->dgr; ++n) {
	quo->cf[n] /= dvsor->cf[0];
	for (divisor=1; divisor <= dvsor->dgr; ++divisor) {
	    quo->cf[n+divisor] -= quo->cf[n] * dvsor->cf[divisor];
	}
    }
    for (n=1; n<=(rem->dgr+1); ++n) {
	rem->cf[n-1] = quo->cf[quo->dgr+n];
	quo->cf[quo->dgr+n] = 0;
    }
}


int
bn_poly_quadratic_roots(register struct bn_complex *roots, register const struct bn_poly *quadrat)
{
    fastf_t discrim, denom, rad;
    const fastf_t small = SMALL_FASTF;

    if (NEAR_ZERO(quadrat->cf[0], small)) {
	/* root = -cf[2] / cf[1] */
	if (NEAR_ZERO(quadrat->cf[1], small)) {
	    /* No solution.  Now what? */
	    /* bu_log("bn_poly_quadratic_roots(): ERROR, no solution\n"); */
	    return 0;
	}
	/* Fake it as a repeated root. */
	roots[0].re = roots[1].re = -quadrat->cf[2]/quadrat->cf[1];
	roots[0].im = roots[1].im = 0.0;
	return 1;	/* OK - repeated root */
    }

    discrim = quadrat->cf[1]*quadrat->cf[1] - 4.0* quadrat->cf[0]*quadrat->cf[2];
    denom = 0.5 / quadrat->cf[0];

    if (discrim > 0.0) {
	rad = sqrt(discrim);

	if (NEAR_ZERO(quadrat->cf[1], small)) {
	    double r = fabs(rad * denom);
	    roots[0].re = r;
	    roots[1].re = -r;
	} else {
	    double t, r1, r2;

	    if (quadrat->cf[1] > 0.0) {
		t = -0.5 * (quadrat->cf[1] + rad);
	    } else {
		t = -0.5 * (quadrat->cf[1] - rad);
	    }
	    r1 = t / quadrat->cf[0];
	    r2 = quadrat->cf[2] / t;

	    if (r1 < r2) {
		roots[0].re = r1;
		roots[1].re = r2;
	    } else {
		roots[0].re = r2;
		roots[1].re = r1;
	    }
	}
	roots[1].im = roots[0].im = 0.0;
    } else if (NEAR_ZERO(discrim, small)) {
	roots[1].re = roots[0].re = -quadrat->cf[1] * denom;
	roots[1].im = roots[0].im = 0.0;
    } else {
	roots[1].re = roots[0].re = -quadrat->cf[1] * denom;
	roots[1].im = -(roots[0].im = sqrt(-discrim) * denom);
    }
    return 1;		/* OK */
}


int
bn_poly_cubic_roots(register struct bn_complex *roots, register const struct bn_poly *eqn)
{
    fastf_t a, b, c1, c1_3rd, delta;
    register int i;

    c1 = eqn->cf[1];
    if (fabs(c1) > SQRT_MAX_FASTF) return 0;	/* FAIL */

    c1_3rd = c1 * THIRD;
    a = eqn->cf[2] - c1*c1_3rd;
    if (fabs(a) > SQRT_MAX_FASTF) return 0;	/* FAIL */
    b = (2.0*c1*c1*c1 - 9.0*c1*eqn->cf[2] + 27.0*eqn->cf[3])*TWENTYSEVENTH;
    if (fabs(b) > SQRT_MAX_FASTF) return 0;	/* FAIL */

    if ((delta = a*a) > SQRT_MAX_FASTF) return 0;	/* FAIL */
    delta = b*b*0.25 + delta*a*TWENTYSEVENTH;

    if (delta > 0.0) {
	fastf_t r_delta, A, B;

	r_delta = sqrt(delta);
	A = B = -0.5 * b;
	A += r_delta;
	B -= r_delta;

	A = CUBEROOT(A);
	B = CUBEROOT(B);

	roots[2].re = roots[1].re = -0.5 * (roots[0].re = A + B);

	roots[0].im = 0.0;
	roots[2].im = -(roots[1].im = (A - B)*M_SQRT3*0.5);
    } else if (ZERO(delta)) {
	fastf_t b_2;
	b_2 = -0.5 * b;

	roots[0].re = 2.0* CUBEROOT(b_2);
	roots[2].re = roots[1].re = -0.5 * roots[0].re;
	roots[2].im = roots[1].im = roots[0].im = 0.0;
    } else {
	fastf_t phi, fact;
	fastf_t cs_phi, sn_phi_s3;

	if (a >= 0.0) {
	    fact = 0.0;
	    cs_phi = 1.0;		/* cos(phi); */
	    sn_phi_s3 = 0.0;	/* sin(phi) * M_SQRT3; */
	} else {
	    register fastf_t f;
	    a *= -THIRD;
	    fact = sqrt(a);
	    if ((f = b * (-0.5) / (a*fact)) >= 1.0) {
		cs_phi = 1.0;		/* cos(phi); */
		sn_phi_s3 = 0.0;	/* sin(phi) * M_SQRT3; */
	    }  else if (f <= -1.0) {
		phi = M_PI_3;
		cs_phi = cos(phi);
		sn_phi_s3 = sin(phi) * M_SQRT3;
	    } else {
		phi = acos(f) * THIRD;
		cs_phi = cos(phi);
		sn_phi_s3 = sin(phi) * M_SQRT3;
	    }
	}

	roots[0].re = 2.0*fact*cs_phi;
	roots[1].re = fact*(sn_phi_s3 - cs_phi);
	roots[2].re = fact*(-sn_phi_s3 - cs_phi);
	roots[2].im = roots[1].im = roots[0].im = 0.0;
    }
    for (i=0; i < 3; ++i)
	roots[i].re -= c1_3rd;

    return 1;		/* OK */
}


int
bn_poly_quartic_roots(register struct bn_complex *roots, register const struct bn_poly *eqn)
{
    struct bn_poly cube, quad1, quad2;
    bn_complex_t u[3];
    fastf_t U, p, q, q1, q2;

    /* something considerably larger than squared floating point fuss */
    const fastf_t small = 1.0e-8;

#define Max3(a, b, c) ((c)>((a)>(b)?(a):(b)) ? (c) : ((a)>(b)?(a):(b)))

    cube.dgr = 3;
    cube.cf[0] = 1.0;
    cube.cf[1] = -eqn->cf[2];
    cube.cf[2] = eqn->cf[3]*eqn->cf[1]
	- 4*eqn->cf[4];
    cube.cf[3] = -eqn->cf[3]*eqn->cf[3]
	- eqn->cf[4]*eqn->cf[1]*eqn->cf[1]
	+ 4*eqn->cf[4]*eqn->cf[2];

    if (!bn_poly_cubic_roots(u, &cube)) {
	return 0;		/* FAIL */
    }
    if (!ZERO(u[1].im)) {
	U = u[0].re;
    } else {
	U = Max3(u[0].re, u[1].re, u[2].re);
    }

    p = eqn->cf[1]*eqn->cf[1]*0.25 + U - eqn->cf[2];
    U *= 0.5;
    q = U*U - eqn->cf[4];
    if (p < 0) {
	if (p < -small) {
	    return 0;	/* FAIL */
	}
	p = 0;
    } else {
	p = sqrt(p);
    }
    if (q < 0) {
	if (q < -small) {
	    return 0;	/* FAIL */
	}
	q = 0;
    } else {
	q = sqrt(q);
    }

    quad1.dgr = quad2.dgr = 2;
    quad1.cf[0] = quad2.cf[0] = 1.0;
    quad1.cf[1] = eqn->cf[1]*0.5;
    quad2.cf[1] = quad1.cf[1] + p;
    quad1.cf[1] -= p;

    q1 = U - q;
    q2 = U + q;

    p = quad1.cf[1]*q2 + quad2.cf[1]*q1 - eqn->cf[3];
    if (NEAR_ZERO(p, small)) {
	quad1.cf[2] = q1;
	quad2.cf[2] = q2;
    } else {
	q = quad1.cf[1]*q1 + quad2.cf[1]*q2 - eqn->cf[3];
	if (NEAR_ZERO(q, small)) {
	    quad1.cf[2] = q2;
	    quad2.cf[2] = q1;
	} else {
	    return 0;	/* FAIL */
	}
    }

    bn_poly_quadratic_roots(&roots[0], &quad1);
    bn_poly_quadratic_roots(&roots[2], &quad2);
    return 1;		/* SUCCESS */
}


void
bn_pr_poly(const char *title, register const struct bn_poly *eqn)
{
    register size_t n;
    register size_t exponent;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char buf[48];

    bu_vls_extend(&str, 196);
    bu_vls_strcat(&str, title);
    snprintf(buf, 48, " polynomial, degree = %lu\n", (long unsigned int)eqn->dgr);
    bu_vls_strcat(&str, buf);

    exponent = eqn->dgr;
    for (n=0; n<=eqn->dgr; n++, exponent--) {
	register double coeff = eqn->cf[n];
	if (n > 0) {
	    if (coeff < 0) {
		bu_vls_strcat(&str, " - ");
		coeff = -coeff;
	    } else {
		bu_vls_strcat(&str, " + ");
	    }
	}
	bu_vls_printf(&str, "%g", coeff);
	if (exponent > 1) {
	    bu_vls_printf(&str, " *X^%zu", exponent);
	} else if (exponent == 1) {

	    bu_vls_strcat(&str, " *X");
	} else {
	    /* For constant term, add nothing */
	}
    }
    bu_vls_strcat(&str, "\n");
    bu_log("%s", bu_vls_addr(&str));
    bu_vls_free(&str);
}


void
bn_pr_roots(const char *title, const struct bn_complex *roots, int n)
{
    register int i;

    bu_log("%s: %d roots:\n", title, n);
    for (i=0; i<n; i++) {
	bu_log("%4d %e + i * %e\n", i, roots[i].re, roots[i].im);
    }
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
