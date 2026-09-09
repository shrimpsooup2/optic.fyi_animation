/* Software rasteriser for the Optic eye. No platform dependencies, so the
   geometry can be tested off-Windows. */
#ifndef EYE_RENDER_H
#define EYE_RENDER_H

/* Writes S*S premultiplied BGRA pixels, top-down.
     phi   - rotation of the mark, radians, counter-clockwise. The pupil rests
             on the 45 degree axis, so phi aims it wherever you like.
     blink - 1.0 eye wide open, 0.0 shut.
     gaze  - 1.0 pupil out at its resting orbit, 0.0 drawn in towards the
             centre. It stays inside the notch either way, so the mark still
             reads as the logo.
     light - 0.0 for dark surroundings, 1.0 for light ones. Fades the ink
             between off-white and near-black.
     scale - 1.0 normal size, 0.0 gone. May overshoot past 1 a little; the box
             has room for it. */
void eye_render(unsigned int *px, int S, double phi, double blink,
                double gaze, double light, double scale);

/* Angle that points the pupil at (dx, dy), given in screen pixels from the
   eye's centre with y running down. Feed the result to eye_render as phi. */
double eye_angle_to(double dx, double dy);

/* Eyeball radius in pixels for a box of S pixels. */
double eye_radius(int S);

#endif
