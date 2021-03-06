/* 
**  mod_rounded_corners.c -- Apache sample rounded_corners module
**
**  In httpd.conf
**    LoadModule rounded_corners_module modules/mod_rounded_corners.so
**    <Location />
**    SetHandler rounded-corners
**    </Location>
**
**  URI sample
**    /ffffff/000000/20.png
*/

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include <wand/MagickWand.h>
#include <wand/drawing-wand.h>

#define RLOG_ERR(r,format, ...) ap_log_rerror(APLOG_MARK, \
                                              APLOG_NOERRNO|APLOG_ERR, 0, r, \
                                              format, __VA_ARGS__)

#define MIN_RADIUS 0
#define MAX_RADIUS 512
#define MAX_URI_LEN 256
#define FG_COLOR_INDEX 0
#define BG_COLOR_INDEX 1
#define DEFAULT_FG_COLOR "#000000"
#define DEFAULT_BG_COLOR "#ffffff"

typedef struct {
    char *color[2];
    int radius;
    char *format;
} rounded_corners_spec_t;


static apr_status_t parse_uri(request_rec *r, rounded_corners_spec_t *spec)
{
    const char *uri;
    char *chunk, *radius;
    unsigned long dec;
    int rgb[3];
    int i = 0;

    uri = (const char *)apr_pstrdup(r->pool, r->uri);
    uri++; // skip root
    while (*uri && (chunk = ap_getword(r->pool, &uri, '/'))) {
        if (ap_strstr(chunk, ".")) {
            radius = ap_getword_nc(r->pool, &chunk, '.');
            spec->radius = apr_atoi64(radius);
            spec->format = (char *)apr_pstrdup(r->pool, chunk);
            if (spec->radius <= MIN_RADIUS || MAX_RADIUS < spec->radius ||
                strlen(spec->format) <= 0) {
                RLOG_ERR(r, "invalid radius size", NULL);
                return APR_EGENERAL;
            }
        }
        else {
            if (strlen(chunk) != 6) {
                RLOG_ERR(r, "invalid uri(%s)", r->uri);
                return APR_EGENERAL;
            }
            spec->color[i] = (char *)apr_psprintf(r->pool, "#%s", chunk);
            i++;
        }
    }
    if (strlen(spec->color[FG_COLOR_INDEX]) == 0)
        spec->color[FG_COLOR_INDEX] = (char *)apr_pstrdup(r->pool, DEFAULT_FG_COLOR);
    if (strlen(spec->color[BG_COLOR_INDEX]) == 0)
        spec->color[BG_COLOR_INDEX] = (char *)apr_pstrdup(r->pool, DEFAULT_BG_COLOR);

    return APR_SUCCESS;
}

static apr_status_t create_rounded_corners(request_rec *r)
{
    rounded_corners_spec_t *spec =
        (rounded_corners_spec_t *)apr_palloc(r->pool, sizeof(rounded_corners_spec_t));
    MagickWand *magick_wand;
    PixelWand *pixel_wand_bg, *pixel_wand_fg;
    DrawingWand *drawing_wand;
    unsigned char *data;
    apr_size_t len = 0;
    apr_status_t rv;

    /* parse uri */
    rv = parse_uri(r, spec);
    if (rv != APR_SUCCESS)
        return HTTP_INTERNAL_SERVER_ERROR;

    /* setup colors */
    pixel_wand_bg = NewPixelWand();
    PixelSetColor(pixel_wand_bg, spec->color[BG_COLOR_INDEX]);
    pixel_wand_fg = NewPixelWand();
    PixelSetColor(pixel_wand_fg, spec->color[FG_COLOR_INDEX]);

    /* create image */
    MagickWandGenesis();
    magick_wand = NewMagickWand();
    MagickNewImage(magick_wand, spec->radius, spec->radius, pixel_wand_bg);

    /* draw circle */
    drawing_wand = NewDrawingWand();
    DrawSetFillColor(drawing_wand, pixel_wand_fg);
    DrawCircle(drawing_wand,
               0.5 * (spec->radius - 1), 0.5 * (spec->radius - 1),
               0.5 * (spec->radius - 1), 0);
    MagickDrawImage(magick_wand, drawing_wand);

    /* get data */
    MagickSetFormat(magick_wand, spec->format);
    data = MagickGetImageBlob(magick_wand, &len);

    /* clean memory */
    pixel_wand_fg = DestroyPixelWand(pixel_wand_fg);
    pixel_wand_bg = DestroyPixelWand(pixel_wand_bg);
    drawing_wand = DestroyDrawingWand(drawing_wand);
    magick_wand = DestroyMagickWand(magick_wand);
    MagickWandTerminus();

    r->content_type = "image/png";
    ap_rwrite(data, len, r);
    MagickRelinquishMemory(data);

    return APR_SUCCESS;
}

static int rounded_corners_handler(request_rec *r)
{
    if (strcmp(r->handler, "rounded-corners"))
        return DECLINED;
    return create_rounded_corners(r);
}

static void rounded_corners_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(rounded_corners_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA rounded_corners_module = {
    STANDARD20_MODULE_STUFF, 
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    NULL,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    NULL,                  /* table of config file commands       */
    rounded_corners_register_hooks  /* register hooks                      */
};

