#include "hd_movie.h"

#define MOVIE_ASPECT_W 16
#define MOVIE_ASPECT_H 9
#define PICTURE_ASPECT_W 4
#define PICTURE_ASPECT_H 3

/**
 * @brief Fit a box of an aspect ratio inside a rectangle, centered.
 * @param outer The rectangle: x, y, width and height.
 * @param aspect_w The box's aspect, across.
 * @param aspect_h And down.
 * @param rect Set to the box.
 */
static void fit(const int outer[4], int aspect_w, int aspect_h, int rect[4])
{
    int w = outer[2], h = outer[2] * aspect_h / aspect_w;

    if (h > outer[3])
    {
        h = outer[3];
        w = outer[3] * aspect_w / aspect_h;
    }

    rect[0] = outer[0] + (outer[2] - w) / 2;
    rect[1] = outer[1] + (outer[3] - h) / 2;
    rect[2] = w;
    rect[3] = h;
}

void lom_hd_movie_fit(int window_w, int window_h, int wide, int rect[4])
{
    const int window[4] = {0, 0, window_w > 0 ? window_w : 0, window_h > 0 ? window_h : 0};
    int picture[4];
    fit(window, wide ? MOVIE_ASPECT_W : PICTURE_ASPECT_W, wide ? MOVIE_ASPECT_H : PICTURE_ASPECT_H, picture);
    fit(picture, MOVIE_ASPECT_W, MOVIE_ASPECT_H, rect);
}
