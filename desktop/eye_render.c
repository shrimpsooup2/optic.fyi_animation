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

/* Eyeball radius as a fraction of the box. The pupil reaches 1.127R, and the
   pop-in overshoots to about 1.10, so 0.40 keeps the far edge inside. */
#define FIT    0.40
#define SS     4                   /* supersampling, per axis */

/* BGR. The mark inverts on a light backdrop, and the pupil deepens with it so
   it keeps its contrast either way. */
static const double INK_ON_DARK[3]  = {0xF5, 0xF4, 0xF2};
static const double INK_ON_LIGHT[3] = {0x1F, 0x18, 0x14};
static const double PUP_ON_DARK[3]  = {0xCB, 0xDB, 0x3D};
static const double PUP_ON_LIGHT[3] = {0x90, 0x9E, 0x0E};

double eye_radius(int S)
{
    return S * FIT;
}

void eye_look(double dx, double dy, double r_px, double *gx, double *gy)
{
    double d = sqrt(dx * dx + dy * dy), t;
    if (d < 1e-6) { *gx = 0.0; *gy = 0.0; return; }
    t = d / (r_px * 2.0);               /* saturates once it is well clear */
    if (t > 1.0) t = 1.0;
    *gx =  (dx / d) * DOT_D * t;
    *gy = -(dy / d) * DOT_D * t;        /* screen y is down, the mark's is up */
}

void eye_render(unsigned int *px, int S, double gx, double gy,
                double blink, double light, double scale)
{
    const double R = S * FIT * (scale < 0.0 ? 0.0 : scale), c = S * 0.5;
    const double dotx = gx * R, doty = gy * R;
    /* The iris is a circle around the pupil, not a fixed notch, so it rides
       along. Scaling its offset by CUT_D/DOT_D keeps the resting mark exactly
       the logo, and brings both to the centre together as the pupil draws in. */
    const double cutx = dotx * (CUT_D / DOT_D), cuty = doty * (CUT_D / DOT_D);
    double ink[3], pup[3];
    int ci, i;

    if (R <= 0.5) {                       /* mid-teleport: nothing to draw */
        for (i = 0; i < S * S; i++) px[i] = 0;
        return;
    }
    if (light < 0.0) light = 0.0; else if (light > 1.0) light = 1.0;
    for (ci = 0; ci < 3; ci++) {
        ink[ci] = INK_ON_DARK[ci] + (INK_ON_LIGHT[ci] - INK_ON_DARK[ci]) * light;
        pup[ci] = PUP_ON_DARK[ci] + (PUP_ON_LIGHT[ci] - PUP_ON_DARK[ci]) * light;
    }
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
                    double dv, bow, ex, ey;

                    /* Eyelids do not rotate with the mark. */
                    dv = v + apy;
                    bow = (dv >= 0.0) ? up : dn;
                    if (bow <= 0.0) continue;
                    if ((u * u) / (apx * apx) + (dv * dv) / (bow * bow) > 1.0) continue;

                    ex = u - dotx; ey = v - doty;
                    if (ex * ex + ey * ey <= (DOT_R * R) * (DOT_R * R)) { nPup++; continue; }

                    if (u * u + v * v > R * R) continue;
                    ex = u - cutx; ey = v - cuty;
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
                        double ch = (ink[i] * nInk + pup[i] * nPup) / n;
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
