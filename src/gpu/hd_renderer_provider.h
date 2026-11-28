#ifndef LOM_HD_RENDERER_PROVIDER_H
#define LOM_HD_RENDERER_PROVIDER_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct LomHdRendererProvider
    {
        int (*texture_create)(void* context, int which, int layers, int size, int levels);
        int (*texture_upload)(void* context, int which, int layer, int x, int y, int w, int h, const void* pixels);
        void (*texture_mipmaps)(void* context, int which);
        void (*texture_release)(void* context, int which);
        void (*quad)(void* context, const short xy[8], const unsigned short uv[8], unsigned rgba, int tpage, unsigned flags, unsigned clut);
        void* context;
    } LomHdRendererProvider;

    /**
     * @brief The provider in use for the renderer's HD drawing: the renderer's
     *        own, unless a test put in its own.
     * @return The provider. Each call takes its @c context first, then what
     *         src/gpu/hd_textures.h says: make, fill, mipmap and drop one of
     *         the renderer's two HD textures, and queue one quad in its batch.
     *         They're all NULL when the renderer can't draw HD art.
     * @note This is the HD drawing's provider: the one place the port reaches
     *       the renderer's HD calls, each a straight pass-through to the GPU,
     *       which is as low as the renderer can be reached and still be
     *       replaced. Everything above it (which pictures go where, what fits,
     *       how they're blended, where cells are on a page) is the port's own
     *       code, in src/remaster/remaster_hd_art.c, which tests run for real
     *       with a provider of their own that keeps what's uploaded and queued
     *       in memory (tests/support/gpu/hd_renderer_provider_fake.h).
     *       tools/testing/provider_check.py keeps the rest of the port from
     *       going around it.
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
