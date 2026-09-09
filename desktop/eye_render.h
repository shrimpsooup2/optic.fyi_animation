/* Software rasteriser for the Optic eye. No platform dependencies, so the
   geometry can be tested off-Windows. */
#ifndef EYE_RENDER_H
#define EYE_RENDER_H

/* Writes S*S premultiplied BGRA pixels, top-down.
     phi   - rotation of the mark, radians, counter-clockwise. The pupil rests
             on the 45 degree axis, so phi aims it wherever you like.
     blink - 1.0 eye wide open, 0.0 shut. */
void eye_render(unsigned int *px, int S, double phi, double blink);

/* Angle that points the pupil at (dx, dy), given in screen pixels from the
   eye's centre with y running down. Feed the result to eye_render as phi. */
double eye_angle_to(double dx, double dy);

#endif
