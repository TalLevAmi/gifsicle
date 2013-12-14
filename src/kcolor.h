/* kcolor.h - Color-oriented function declarations for gifsicle.
   Copyright (C) 2013 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#ifndef GIFSICLE_KCOLOR_H
#define GIFSICLE_KCOLOR_H
#include <lcdfgif/gif.h>
#include <assert.h>

/* kcolor: a 3D vector, each component has 15 bits of precision */
/* 15 bits means KC_MAX * KC_MAX always fits within a signed 32-bit
   integer, and a 3-D squared distance always fits within an unsigned 32-bit
   integer. */
#define KC_MAX   0x7FFF
#define KC_WHOLE 0x8000
#define KC_HALF  0x4000
#define KC_BITS  15
typedef struct kcolor {
    int16_t a[3];
} kcolor;

#define KC_CLAMPV(v) ((v) < 0 ? 0 : ((v) < KC_MAX ? (v) : KC_MAX))


/* gamma_tables[0]: array of 256 gamma-conversion values
   gamma_tables[1]: array of 256 reverse gamma-conversion values */
extern uint16_t* gamma_tables[2];


/* set `*x` to the gamma transformation of `a0/a1/a2` [RGB] */
static inline void kc_set8g(kcolor* x, int a0, int a1, int a2) {
    x->a[0] = gamma_tables[0][a0];
    x->a[1] = gamma_tables[0][a1];
    x->a[2] = gamma_tables[0][a2];
}

/* return a hex color string definition for `x` */
const char* kc_debug_str(kcolor x);

/* set `*x` to the reverse gamma transformation of `*x` */
static inline void kc_revgamma_transform(kcolor* x) {
    int d;
    for (d = 0; d != 3; ++d) {
        int c = gamma_tables[1][x->a[d] >> 7];
        while (c < 0x7F80 && x->a[d] >= gamma_tables[0][(c + 0x80) >> 7])
            c += 0x80;
        x->a[d] = c;
    }
}


/* return the squared Euclidean distance between `*x` and `*y` */
static inline uint32_t kc_distance(const kcolor* x, const kcolor* y) {
    int32_t d0 = x->a[0] - y->a[0], d1 = x->a[1] - y->a[1],
        d2 = x->a[2] - y->a[2];
    return d0 * d0 + d1 * d1 + d2 * d2;
}

/* return the luminance value for `*x`; result is between 0 and KC_MAX */
static inline int kc_luminance(const kcolor* x) {
    return (306 * x->a[0] + 601 * x->a[1] + 117 * x->a[2]) >> 10;
}

/* set `*x` to the grayscale version of `*x`, transformed by luminance */
static inline void kc_luminance_transform(kcolor* x) {
    /* For grayscale colormaps, use distance in luminance space instead of
       distance in RGB space. The weights for the R,G,B components in
       luminance space are 0.299,0.587,0.114. Using the proportional factors
       306, 601, and 117 we get a scaled gray value between 0 and 255 *
       1024. Thanks to Christian Kumpf, <kumpf@igd.fhg.de>, for providing a
       patch. */
    x->a[0] = x->a[1] = x->a[2] = kc_luminance(x);
}


/* wkcolor: like kcolor, but components are 32 bits instead of 16 */

typedef struct wkcolor {
    int32_t a[3];
} wkcolor;

static inline void wkc_clear(wkcolor* x) {
    x->a[0] = x->a[1] = x->a[2] = 0;
}


/* kd3_tree: kd-tree for 3 dimensions, indexing kcolors */

typedef struct kd3_tree kd3_tree;
typedef struct kd3_treepos kd3_treepos;

struct kd3_tree {
    kd3_treepos* tree;
    int ntree;
    int disabled;
    kcolor* ks;
    int nitems;
    int items_cap;
    int maxdepth;
    void (*transform)(kcolor*);
    unsigned* xradius;
};

/* initialize `kd3` with the given color `transform` (may be NULL) */
void kd3_init(kd3_tree* kd3, void (*transform)(kcolor*));

/* free `kd3` */
void kd3_cleanup(kd3_tree* kd3);

/* add the transformed color `k` to `*kd3` (do not apply `kd3->transform`). */
void kd3_add_transformed(kd3_tree* kd3, const kcolor* k);

/* given 8-bit color `a0/a1/a2` (RGB), gamma-transform it, transform it
   by `kd3->transform` if necessary, and add it to `*kd3` */
void kd3_add8g(kd3_tree* kd3, int a0, int a1, int a2);

/* set `kd3->xradius`. given color `i`, `kd3->xradius[i]` is the square of the
   color's uniquely owned neighborhood.
   If `kc_distance(&kd3->ks[i], &k) < kd3->xradius[i]`, then
   `kd3_closest_transformed(kd3, &k) == i`. */
void kd3_build_xradius(kd3_tree* kd3);

/* build the actual kd-tree for `kd3`. must be called before kd3_closest. */
void kd3_build(kd3_tree* kd3);

/* kd3_init + kd3_add8g for all colors in `gfcm` + kd3_build */
void kd3_init_build(kd3_tree* kd3, void (*transform)(kcolor*),
                    const Gif_Colormap* gfcm);

/* return the index of the color in `*kd3` closest to `k`. */
int kd3_closest_transformed(const kd3_tree* kd3, const kcolor* k);

/* given 8-bit color `a0/a1/a2` (RGB), gamma-transform it, transform it by
   `kd3->transform` if necessary, and return the index of the color in
   `*kd3` closest to it. */
int kd3_closest8g(const kd3_tree* kd3, int a0, int a1, int a2);

/* disable color index `i` in `*kd3`: it will never be returned by
   `kd3_closest*` */
static inline void kd3_disable(kd3_tree* kd3, int i) {
    assert((unsigned) i < (unsigned) kd3->nitems);
    assert(kd3->disabled < 0 || kd3->disabled == i);
    kd3->disabled = i;
}

/* enable all color indexes in `*kd3` */
static inline void kd3_enable_all(kd3_tree* kd3) {
    kd3->disabled = -1;
}

#endif
