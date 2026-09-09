/* Software rasteriser for the Optic eye. No platform dependencies, so the
   geometry can be tested off-Windows. */
#ifndef EYE_RENDER_H
#define EYE_RENDER_H

/* Writes S*S premultiplied BGRA pixels, top-down.
     gx,gy - where the pupil sits, in units of the eyeball's radius, measured
             from its centre with y running UP. (0,0) is dead centre. The iris
             travels with it, so the pupil is free to go anywhere inside.
     blink - 1.0 eye wide open, 0.0 shut.
     light - 0.0 for dark surroundings, 1.0 for light ones. Fades the ink
             between off-white and near-black.
     scale - 1.0 normal size, 0.0 gone. May overshoot past 1 a little; the box
             has room for it. */
void eye_render(unsigned int *px, int S, double gx, double gy,
                double blink, double light, double scale);

/* Where the pupil should sit to look at something (dx, dy) screen pixels from
   the eye's centre, y running down. r_px is the eyeball's radius in pixels.
   Close up the pupil draws inward; far off it rests out on its orbit. */
void eye_look(double dx, double dy, double r_px, double *gx, double *gy);

/* Eyeball radius in pixels for a box of S pixels. */
double eye_radius(int S);

#endif
