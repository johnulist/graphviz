/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/

/* Comments on the SVG coordinate system (SN 8 Dec 2006):
   The initial <svg> element defines the SVG coordinate system so
   that the graphviz canvas (in units of points) fits the intended
   absolute size in inches.  After this, the situation should be
   that "px" = "pt" in SVG, so we can dispense with stating units.
   Also, the input units (such as fontsize) should be preserved
   without scaling in the output SVG (as long as the graph size
   was not constrained.)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "macros.h"
#include "const.h"

#include "gvplugin_render.h"
#include "gvplugin_device.h"
#include "gvio.h"
#include "gvcint.h"
#include "types.h"		/* need the SVG font name schemes */

extern char *strdup_and_subst_obj(char *str, void *obj);

typedef enum { FORMAT_SVG, FORMAT_SVGZ, } format_type;

extern char *xml_string(char *str);
extern char *xml_url_string(char *str);

/* SVG dash array */
static char *sdasharray = "5,2";
/* SVG dot array */
static char *sdotarray = "1,5";

static void svg_bzptarray(GVJ_t * job, pointf * A, int n)
{
    int i;
    char c;

    c = 'M';			/* first point */
    for (i = 0; i < n; i++) {
	gvprintf(job, "%c%g,%g", c, A[i].x, -A[i].y);
	if (i == 0)
	    c = 'C';		/* second point */
	else
	    c = ' ';		/* remaining points */
    }
}

static void svg_print_color(GVJ_t * job, gvcolor_t color)
{
    switch (color.type) {
    case COLOR_STRING:
	gvputs(job, color.u.string);
	break;
    case RGBA_BYTE:
	if (color.u.rgba[3] == 0) /* transparent */
	    gvputs(job, "none");
	else
	    gvprintf(job, "#%02x%02x%02x",
		color.u.rgba[0], color.u.rgba[1], color.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
}

static void svg_grstyle(GVJ_t * job, int filled)
{
    obj_state_t *obj = job->obj;

    gvputs(job, " fill=\"");
    if (filled)
	svg_print_color(job, obj->fillcolor);
    else
	gvputs(job, "none");
    gvputs(job, "\" stroke=\"");
    svg_print_color(job, obj->pencolor);
    if (obj->penwidth != PENWIDTH_NORMAL)
	gvprintf(job, "\" stroke-width=\"%g", obj->penwidth);
    if (obj->pen == PEN_DASHED) {
	gvprintf(job, "\" stroke-dasharray=\"%s", sdasharray);
    } else if (obj->pen == PEN_DOTTED) {
	gvprintf(job, "\" stroke-dasharray=\"%s", sdotarray);
    }
    gvputs(job, "\"");
}

static void svg_comment(GVJ_t * job, char *str)
{
    gvputs(job, "<!-- ");
    gvputs(job, xml_string(str));
    gvputs(job, " -->\n");
}

/* isAscii:
 * Return true if all characters in the string are ascii.
 */
static int isAscii (char* s)
{
    int c;
    while ((c = *s++) != '\0') {
	if (!isascii (c)) return 0;
    }
    return 1;
}

static void svg_begin_job(GVJ_t * job)
{
    char *s;
    gvputs(job, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
    if ((s = agget(job->gvc->g, "stylesheet")) && s[0]) {
        gvputs(job, "<?xml-stylesheet href=\"");
        gvputs(job, s);
        gvputs(job, "\" type=\"text/css\"?>\n");
    }
#if 0
    gvputs(job, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n");
    gvputs(job, " \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\"");
    /* This is to work around a bug in the SVG 1.0 DTD */
    gvputs(job, " [\n <!ATTLIST svg xmlns:xlink CDATA #FIXED \"http://www.w3.org/1999/xlink\">\n]");
#else
    gvputs(job, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"); 
    gvputs(job, " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
#endif

    gvputs(job, "<!-- Generated by ");
    gvputs(job, xml_string(job->common->info[0]));
    gvputs(job, " version ");
    gvputs(job, xml_string(job->common->info[1]));
    gvputs(job, " (");
    gvputs(job, xml_string(job->common->info[2]));
    /* We have absolutely no idea what character set the username
     * may be in. To be conservative, we only output the username
     * if it is all ascii. Since SVG output is UTF-8, we could check
     * if the string appears to be in this format and allow it.
     */
    if (isAscii (job->common->user)) {
	gvputs(job, ")\n     For user: ");
	gvputs(job, xml_string(job->common->user));
    }
    else
	gvputs(job, ")\n");
    gvputs(job, " -->\n");
}

static void svg_begin_graph(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "<!--");
    if (agnameof(obj->u.g)[0]) {
        gvputs(job, " Title: ");
	gvputs(job, xml_string(agnameof(obj->u.g)));
    }
    gvprintf(job, " Pages: %d -->\n", job->pagesArraySize.x * job->pagesArraySize.y);

    gvprintf(job, "<svg width=\"%dpt\" height=\"%dpt\"\n",
	job->width, job->height);
    gvprintf(job, " viewBox=\"%.2f %.2f %.2f %.2f\"",
        job->canvasBox.LL.x, job->canvasBox.LL.y,
        job->canvasBox.UR.x, job->canvasBox.UR.y);
    /* namespace of svg */
    gvputs(job, " xmlns=\"http://www.w3.org/2000/svg\"");
    /* namespace of xlink */
    gvputs(job, " xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
    gvputs(job, ">\n");
}

static void svg_end_graph(GVJ_t * job)
{
    gvputs(job, "</svg>\n");
}

static void svg_begin_layer(GVJ_t * job, char *layername, int layerNum, int numLayers)
{
    gvputs(job, "<g id=\"");
    gvputs(job, xml_string(layername));
    gvputs(job, "\" class=\"layer\">\n");
}

static void svg_end_layer(GVJ_t * job)
{
    gvputs(job, "</g>\n");
}

static void svg_begin_page(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    /* its really just a page of the graph, but its still a graph,
     * and it is the entire graph if we're not currently paging */
    gvputs(job, "<g id=\"");
    gvputs(job, xml_string(obj->id));
    gvputs(job, "\" class=\"graph\"");
    gvprintf(job, " transform=\"scale(%g %g) rotate(%d) translate(%g %g)\">\n",
	    job->scale.x, job->scale.y, -job->rotation,
	    job->translation.x, -job->translation.y);
    /* default style */
    if (agnameof(obj->u.g)[0]) {
        gvputs(job, "<title>");
        gvputs(job, xml_string(agnameof(obj->u.g)));
        gvputs(job, "</title>\n");
    }
}

static void svg_end_page(GVJ_t * job)
{
    gvputs(job, "</g>\n");
}

static void svg_begin_cluster(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "<g id=\"");
    gvputs(job, xml_string(obj->id));
    gvputs(job, "\" class=\"cluster\">");
    gvputs(job, "<title>");
    gvputs(job, xml_string(agnameof(obj->u.g)));
    gvputs(job, "</title>\n");
}

static void svg_end_cluster(GVJ_t * job)
{
    gvputs(job, "</g>\n");
}

static void svg_begin_node(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "<g id=\"");
    gvputs(job, xml_string(obj->id));
    gvputs(job, "\" class=\"node\">");
    gvputs(job, "<title>");
    gvputs(job, xml_string(agnameof(obj->u.n)));
    gvputs(job, "</title>\n");
}

static void svg_end_node(GVJ_t * job)
{
    gvputs(job, "</g>\n");
}

static void
svg_begin_edge(GVJ_t * job)
{
    obj_state_t *obj = job->obj;
    char *ename;

    gvputs(job, "<g id=\"");
    gvputs(job, xml_string(obj->id));
    gvputs(job, "\" class=\"edge\">");
    
    gvputs(job, "<title>");
    ename = strdup_and_subst_obj("\\E", (void*)(obj->u.e));
    gvputs(job, xml_string(ename));
    free(ename);
    gvputs(job, "</title>\n");
}

static void svg_end_edge(GVJ_t * job)
{
    gvputs(job, "</g>\n");
}

static void
svg_begin_anchor(GVJ_t * job, char *href, char *tooltip, char *target, char *id)
{
    gvputs(job, "<a");
#if 0
    /* the svg spec implies this can be omitted: http://www.w3.org/TR/SVG/linking.html#Links */
    gvputs(job, " xlink:type=\"simple\"");
#endif
    if (href && href[0])
	gvprintf(job, " xlink:href=\"%s\"", xml_url_string(href));
#if 0
    /* linking to itself, just so that it can have a xlink:link in the anchor, seems wrong.
     * it changes the behavior in browsers, the link apears in the bottom information bar
     */
    else {
        assert (id && id[0]); /* there should always be an id available */
	gvprintf(job, " xlink:href=\"#%s\"", xml_url_string(id));
    }
#endif
    if (tooltip && tooltip[0])
	gvprintf(job, " xlink:title=\"%s\"", xml_string(tooltip));
    if (target && target[0])
	gvprintf(job, " target=\"%s\"", xml_string(target));
    gvputs(job, ">\n");
}

static void svg_end_anchor(GVJ_t * job)
{
    gvputs(job, "</a>\n");
}

static void svg_textpara(GVJ_t * job, pointf p, textpara_t * para)
{
    obj_state_t *obj = job->obj;
    PostscriptAlias *pA;

    gvputs(job, "<text");
    switch (para->just) {
    case 'l':
	gvputs(job, " text-anchor=\"start\"");
	break;
    case 'r':
	gvputs(job, " text-anchor=\"end\"");
	break;
    default:
    case 'n':
	gvputs(job, " text-anchor=\"middle\"");
	break;
    }
    p.y += para->yoffset_centerline;
    gvprintf(job, " x=\"%g\" y=\"%g\"", p.x, -p.y);
    pA = para->postscript_alias;
    if (pA) {
	char *family=NULL, *weight=NULL, *stretch=NULL, *style=NULL;
	switch(GD_fontnames(job->gvc->g)) {
		case PSFONTS:
		    family = pA->name;
		    weight = pA->weight;
		    style = pA->style;
		    break;
		case SVGFONTS:
                    family = pA->svg_font_family;
                    weight = pA->svg_font_weight;
                    style = pA->svg_font_style;
		    break;
		default:
		case NATIVEFONTS:
		    family = pA->family;
		    weight = pA->weight;
		    style = pA->style;
		    break;
	}
	stretch = pA->stretch;

        gvprintf(job, " font-family=\"%s", family);
	if (pA->svg_font_family) gvprintf(job, ",%s", pA->svg_font_family);
        gvputs(job, "\"");
        if (weight) gvprintf(job, " font-weight=\"%s\"", weight);
        if (stretch) gvprintf(job, " font-stretch=\"%s\"", stretch);
        if (style) gvprintf(job, " font-style=\"%s\"", style);
    }
    else
	gvprintf(job, " font-family=\"%s\"", para->fontname);
    gvprintf(job, " font-size=\"%.2f\"", para->fontsize);
    switch (obj->pencolor.type) {
    case COLOR_STRING:
	if (strcasecmp(obj->pencolor.u.string, "black"))
	    gvprintf(job, " fill=\"%s\"", obj->pencolor.u.string);
	break;
    case RGBA_BYTE:
	gvprintf(job, " fill=\"#%02x%02x%02x\"",
		obj->pencolor.u.rgba[0], obj->pencolor.u.rgba[1], obj->pencolor.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
    gvputs(job, ">");
    gvputs(job, xml_string(para->str));
    gvputs(job, "</text>\n");
}

static void svg_ellipse(GVJ_t * job, pointf * A, int filled)
{
    /* A[] contains 2 points: the center and corner. */
    gvputs(job, "<ellipse");
    svg_grstyle(job, filled);
    gvprintf(job, " cx=\"%g\" cy=\"%g\"", A[0].x, -A[0].y);
    gvprintf(job, " rx=\"%g\" ry=\"%g\"",
	    A[1].x - A[0].x, A[1].y - A[0].y);
    gvputs(job, "/>\n");
}

static void
svg_bezier(GVJ_t * job, pointf * A, int n, int arrow_at_start,
	      int arrow_at_end, int filled)
{
    gvputs(job, "<path");
    svg_grstyle(job, filled);
    gvputs(job, " d=\"");
    svg_bzptarray(job, A, n);
    gvputs(job, "\"/>\n");
}

static void svg_polygon(GVJ_t * job, pointf * A, int n, int filled)
{
    int i;

    gvputs(job, "<polygon");
    svg_grstyle(job, filled);
    gvputs(job, " points=\"");
    for (i = 0; i < n; i++)
	gvprintf(job, "%g,%g ", A[i].x, -A[i].y);
    gvprintf(job, "%g,%g", A[0].x, -A[0].y);	/* because Adobe SVG is broken */
    gvputs(job, "\"/>\n");
}

static void svg_polyline(GVJ_t * job, pointf * A, int n)
{
    int i;

    gvputs(job, "<polyline");
    svg_grstyle(job, 0);
    gvputs(job, " points=\"");
    for (i = 0; i < n; i++)
	gvprintf(job, "%g,%g ", A[i].x, -A[i].y);
    gvputs(job, "\"/>\n");
}

/* color names from http://www.w3.org/TR/SVG/types.html */
/* NB.  List must be LANG_C sorted */
static char *svg_knowncolors[] = {
    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure",
    "beige", "bisque", "black", "blanchedalmond", "blue",
    "blueviolet", "brown", "burlywood",
    "cadetblue", "chartreuse", "chocolate", "coral",
    "cornflowerblue", "cornsilk", "crimson", "cyan",
    "darkblue", "darkcyan", "darkgoldenrod", "darkgray",
    "darkgreen", "darkgrey", "darkkhaki", "darkmagenta",
    "darkolivegreen", "darkorange", "darkorchid", "darkred",
    "darksalmon", "darkseagreen", "darkslateblue", "darkslategray",
    "darkslategrey", "darkturquoise", "darkviolet", "deeppink",
    "deepskyblue", "dimgray", "dimgrey", "dodgerblue",
    "firebrick", "floralwhite", "forestgreen", "fuchsia",
    "gainsboro", "ghostwhite", "gold", "goldenrod", "gray",
    "green", "greenyellow", "grey",
    "honeydew", "hotpink", "indianred",
    "indigo", "ivory", "khaki",
    "lavender", "lavenderblush", "lawngreen", "lemonchiffon",
    "lightblue", "lightcoral", "lightcyan", "lightgoldenrodyellow",
    "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue",
    "lightslategray", "lightslategrey", "lightsteelblue",
    "lightyellow", "lime", "limegreen", "linen",
    "magenta", "maroon", "mediumaquamarine", "mediumblue",
    "mediumorchid", "mediumpurple", "mediumseagreen",
    "mediumslateblue", "mediumspringgreen", "mediumturquoise",
    "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin",
    "navajowhite", "navy", "oldlace",
    "olive", "olivedrab", "orange", "orangered", "orchid",
    "palegoldenrod", "palegreen", "paleturquoise",
    "palevioletred", "papayawhip", "peachpuff", "peru", "pink",
    "plum", "powderblue", "purple",
    "red", "rosybrown", "royalblue",
    "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell",
    "sienna", "silver", "skyblue", "slateblue", "slategray",
    "slategrey", "snow", "springgreen", "steelblue",
    "tan", "teal", "thistle", "tomato", "turquoise",
    "violet",
    "wheat", "white", "whitesmoke",
    "yellow", "yellowgreen"
};

gvrender_engine_t svg_engine = {
    svg_begin_job,
    0,				/* svg_end_job */
    svg_begin_graph,
    svg_end_graph,
    svg_begin_layer,
    svg_end_layer,
    svg_begin_page,
    svg_end_page,
    svg_begin_cluster,
    svg_end_cluster,
    0,				/* svg_begin_nodes */
    0,				/* svg_end_nodes */
    0,				/* svg_begin_edges */
    0,				/* svg_end_edges */
    svg_begin_node,
    svg_end_node,
    svg_begin_edge,
    svg_end_edge,
    svg_begin_anchor,
    svg_end_anchor,
    svg_textpara,
    0,				/* svg_resolve_color */
    svg_ellipse,
    svg_polygon,
    svg_bezier,
    svg_polyline,
    svg_comment,
    0,				/* svg_library_shape */
};

gvrender_features_t render_features_svg = {
    GVRENDER_Y_GOES_DOWN
        | GVRENDER_DOES_TRANSFORM
	| GVRENDER_DOES_LABELS
	| GVRENDER_DOES_MAPS
	| GVRENDER_DOES_TARGETS
	| GVRENDER_DOES_TOOLTIPS, /* flags */
    4.,                         /* default pad - graph units */
    svg_knowncolors,		/* knowncolors */
    sizeof(svg_knowncolors) / sizeof(char *),	/* sizeof knowncolors */
    RGBA_BYTE,			/* color_type */
};

gvdevice_features_t device_features_svg = {
    GVDEVICE_DOES_TRUECOLOR,	/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {72.,72.},			/* default dpi */
};

gvdevice_features_t device_features_svgz = {
    GVDEVICE_BINARY_FORMAT
      | GVDEVICE_COMPRESSED_FORMAT
      | GVDEVICE_DOES_TRUECOLOR,/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {72.,72.},			/* default dpi */
};

gvplugin_installed_t gvrender_svg_types[] = {
    {FORMAT_SVG, "svg", 1, &svg_engine, &render_features_svg},
    {0, NULL, 0, NULL, NULL}
};

gvplugin_installed_t gvdevice_svg_types[] = {
    {FORMAT_SVG, "svg:svg", 1, NULL, &device_features_svg},
#if HAVE_LIBZ
    {FORMAT_SVGZ, "svgz:svg", 1, NULL, &device_features_svgz},
#endif
    {0, NULL, 0, NULL, NULL}
};
