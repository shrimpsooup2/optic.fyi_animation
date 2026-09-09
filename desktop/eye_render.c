#include "eye_render.h"
#include <math.h>

/* The mark, in units of the eyeball's radius. Same constants as tools/optic.py,
   which were fitted to the original artwork. */
#define AXIS   0.785398163397448   /* 45 degrees */
#define CUT_D  0.70760
#define CUT_R  0.75080
#define DOT_D  0.75735
#define DOT_R  0.36960

/* Eyelid aperture: corners out at the sides, an upper lid curve and a shallower
   lower one. Sized so that wide open it clears the pupil at any rotation. */
#define AP_X   1.30
#define AP_Y   0.28
#define AP_UP  1.55
#define AP_DN  0.98

#define FIT    0.44                /* eyeball radius as a fraction of the box */
#define SS     4                   /* supersampling, per axis */

static const double INK[3]  = {0xF5, 0xF4, 0xF2};   /* BGR */
static const double TEAL[3] = {0xCB, 0xDB, 0x3D};

double eye_angle_to(double dx, double dy)
{
    /* Screen y runs down; the mark's own maths runs y-up. */
    return atan2(-dy, dx) - AXIS;
}

void eye_render(unsigned int *px, int S, double phi, double blink)
{
    const double R = S * FIT, c = S * 0.5;
    const double cs = cos(phi), sn = sin(phi);
    const double cutx = CUT_D * R * cos(AXIS), cuty = CUT_D * R * sin(AXIS);
    const double dotx = DOT_D * R * cos(AXIS), doty = DOT_D * R * sin(AXIS);
    const double up = AP_UP * R * (blink < 0.0 ? 0.0 : blink);
    const double dn = AP_DN * R * (blink < 0.0 ? 0.0 : blink);
    const double apx = AP_X * R, apy = AP_Y * R;
    const double step = 1.0 / SS, half = step * 0.5;
    int y, x, sy, sx;

    for (y = 0; y < S; y++) {
        for (x = 0; x < S; x++) {
            int nInk = 0, nPup = 0;
            for (sy = 0; sy < SS; sy++) {
                for (sx = 0; sx < SS; sx++) {
                    /* centred, y-up */
                    double u = (x + half + sx * step) - c;
                    double v = c - (y + half + sy * step);
                    double dv, bow, u2, v2, ex, ey;

                    /* Eyelids do not rotate with the mark. */
                    dv = v + apy;
                    bow = (dv >= 0.0) ? up : dn;
                    if (bow <= 0.0) continue;
                    if ((u * u) / (apx * apx) + (dv * dv) / (bow * bow) > 1.0) continue;

                    /* Rotate the sample back into the mark's own frame. */
                    u2 =  u * cs + v * sn;
                    v2 = -u * sn + v * cs;

                    ex = u2 - dotx; ey = v2 - doty;
                    if (ex * ex + ey * ey <= (DOT_R * R) * (DOT_R * R)) { nPup++; continue; }

                    if (u2 * u2 + v2 * v2 > R * R) continue;
                    ex = u2 - cutx; ey = v2 - cuty;
                    if (ex * ex + ey * ey <= (CUT_R * R) * (CUT_R * R)) continue;
                    nInk++;
                }
            }
            {
                int n = nInk + nPup, total = SS * SS, i;
                unsigned int out = 0;
                if (n) {
                    double a = (double)n / total;
                    for (i = 0; i < 3; i++) {
                        double ch = (INK[i] * nInk + TEAL[i] * nPup) / n;
                        unsigned int b = (unsigned int)(ch * a + 0.5);   /* premultiplied */
                        out |= (b > 255 ? 255 : b) << (i * 8);
                    }
                    out |= (unsigned int)(a * 255.0 + 0.5) << 24;
                }
                px[y * S + x] = out;
            }
        }
    }
}
