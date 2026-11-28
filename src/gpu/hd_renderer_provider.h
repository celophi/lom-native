#ifndef LOM_HD_RENDERER_PROVIDER_H
#define LOM_HD_RENDERER_PROVIDER_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct LomHdRendererProvider
    {
        int (*textures_set)(void* context, int layers, int size);
        int (*textures_upload)(void* context, int layer, const void* rgba);
        void (*textures_release)(void* context);
        void (*textures_draw)(void* context, const int xy[8], int layer, int cell, unsigned int rgb, int semi, int tpage);
        int (*images_set)(void* context, int layers, int size);
        int (*images_upload)(void* context, int layer, int x, int y, int w, int h, const void* rgba);
        void (*images_release)(void* context);
        void (*images_draw)(void* context, const int xy[8], const int uv[8], int layer, unsigned int rgb, int semi, int tpage);
        int (*movie_begin)(void* context, unsigned width, unsigned height);
        void (*movie_frame)(void* context, const void* rgba);
        void (*movie_end)(void* context);
        void* context;
    } LomHdRendererProvider;

    /**
     * @brief The provider in use for the renderer's HD drawing: the renderer's
     *        own, unless a test put in its own.
     * @return The provider. Each call takes its @c context first, then what
     *         src/gpu/hd_textures.h and src/gpu/hd_movie.h say:
     *         @c textures_* hold and draw the HD backgrounds' pages,
     *         @c images_* the HD screens' pictures, and @c movie_* show an HD
     *         movie's frames. Pages and pictures are uploaded as the
     *         Remaster's files hold them: blue, green, red, then alpha. A
     *         renderer only takes a draw while it's running a GPU command,
     *         which is how the Remaster's draws are made. A call is NULL when
     *         the renderer can't do it, so the Remaster's code can tell what
     *         it can draw.
     * @note This is the HD drawing's provider: the one place the port reaches
     *       the renderer's HD calls, kept as thin as it can be. Everything
     *       above it (which pictures, where, in what order) is the port's own
     *       code, which tests run for real with a provider of their own that
     *       keeps what's uploaded and drawn in memory
     *       (tests/support/gpu/hd_renderer_provider_fake.h).
     *       tools/testing/provider_check.py keeps the Remaster's and the
     *       movies' code from going around it.
     */
    const LomHdRendererProvider* lom_hd_renderer_provider(void);

    /**
     * @brief Use another provider, as tests do.
     * @param renderer The provider, or NULL to go back to the renderer's. It
     *        has to stay valid while it's in use.
     */
    void lom_hd_use_renderer_provider(const LomHdRendererProvider* renderer);

#ifdef __cplusplus
}
#endif
#endif
