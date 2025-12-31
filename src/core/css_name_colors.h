#include <strings.h>
#include <stdlib.h>

#ifndef CSS_NAME_COLORS_H
#define CSS_NAME_COLORS_H
static uint32_t parse_color(const char *val)
{
    if (!val)
        return 0;
    if (val[0] == '#')
    {
        return (uint32_t)strtol(val + 1, NULL, 16);
    }

    if (strcasecmp(val, "aliceblue") == 0) return 0xf0f8ff;
    if (strcasecmp(val, "antiquewhite") == 0) return 0xfaebd7;
    if (strcasecmp(val, "aqua") == 0) return 0x00ffff;
    if (strcasecmp(val, "aquamarine") == 0) return 0x7fffd4;
    if (strcasecmp(val, "azure") == 0) return 0xf0ffff;
    if (strcasecmp(val, "beige") == 0) return 0xf5f5dc;
    if (strcasecmp(val, "bisque") == 0) return 0xffe4c4;
    if (strcasecmp(val, "black") == 0) return 0x000000;
    if (strcasecmp(val, "blanchedalmond") == 0) return 0xffebcd;
    if (strcasecmp(val, "blue") == 0) return 0x0000ff;
    if (strcasecmp(val, "blueviolet") == 0) return 0x8a2be2;
    if (strcasecmp(val, "brown") == 0) return 0xa52a2a;
    if (strcasecmp(val, "burlywood") == 0) return 0xdeb887;
    if (strcasecmp(val, "cadetblue") == 0) return 0x5f9ea0;
    if (strcasecmp(val, "chartreuse") == 0) return 0x7fff00;
    if (strcasecmp(val, "chocolate") == 0) return 0xd2691e;
    if (strcasecmp(val, "coral") == 0) return 0xff7f50;
    if (strcasecmp(val, "cornflowerblue") == 0) return 0x6495ed;
    if (strcasecmp(val, "cornsilk") == 0) return 0xfff8dc;
    if (strcasecmp(val, "crimson") == 0) return 0xdc143c;
    if (strcasecmp(val, "cyan") == 0) return 0x00ffff;
    if (strcasecmp(val, "darkblue") == 0) return 0x00008b;
    if (strcasecmp(val, "darkcyan") == 0) return 0x008b8b;
    if (strcasecmp(val, "darkgoldenrod") == 0) return 0xb8860b;
    if (strcasecmp(val, "darkgray") == 0) return 0xa9a9a9;
    if (strcasecmp(val, "darkgreen") == 0) return 0x006400;
    if (strcasecmp(val, "darkgrey") == 0) return 0xa9a9a9;
    if (strcasecmp(val, "darkkhaki") == 0) return 0xbdb76b;
    if (strcasecmp(val, "darkmagenta") == 0) return 0x8b008b;
    if (strcasecmp(val, "darkolivegreen") == 0) return 0x556b2f;
    if (strcasecmp(val, "darkorange") == 0) return 0xff8c00;
    if (strcasecmp(val, "darkorchid") == 0) return 0x9932cc;
    if (strcasecmp(val, "darkred") == 0) return 0x8b0000;
    if (strcasecmp(val, "darksalmon") == 0) return 0xe9967a;
    if (strcasecmp(val, "darkseagreen") == 0) return 0x8fbc8f;
    if (strcasecmp(val, "darkslateblue") == 0) return 0x483d8b;
    if (strcasecmp(val, "darkslategray") == 0) return 0x2f4f4f;
    if (strcasecmp(val, "darkslategrey") == 0) return 0x2f4f4f;
    if (strcasecmp(val, "darkturquoise") == 0) return 0x00ced1;
    if (strcasecmp(val, "darkviolet") == 0) return 0x9400d3;
    if (strcasecmp(val, "deeppink") == 0) return 0xff1493;
    if (strcasecmp(val, "deepskyblue") == 0) return 0x00bfff;
    if (strcasecmp(val, "dimgray") == 0) return 0x696969;
    if (strcasecmp(val, "dimgrey") == 0) return 0x696969;
    if (strcasecmp(val, "dodgerblue") == 0) return 0x1e90ff;
    if (strcasecmp(val, "firebrick") == 0) return 0xb22222;
    if (strcasecmp(val, "floralwhite") == 0) return 0xfffaf0;
    if (strcasecmp(val, "forestgreen") == 0) return 0x228b22;
    if (strcasecmp(val, "fuchsia") == 0) return 0xff00ff;
    if (strcasecmp(val, "gainsboro") == 0) return 0xdcdcdc;
    if (strcasecmp(val, "ghostwhite") == 0) return 0xf8f8ff;
    if (strcasecmp(val, "gold") == 0) return 0xffd700;
    if (strcasecmp(val, "goldenrod") == 0) return 0xdaa520;
    if (strcasecmp(val, "gray") == 0) return 0x808080;
    if (strcasecmp(val, "green") == 0) return 0x008000;
    if (strcasecmp(val, "greenyellow") == 0) return 0xadff2f;
    if (strcasecmp(val, "grey") == 0) return 0x808080;
    if (strcasecmp(val, "honeydew") == 0) return 0xf0fff0;
    if (strcasecmp(val, "hotpink") == 0) return 0xff69b4;
    if (strcasecmp(val, "indianred") == 0) return 0xcd5c5c;
    if (strcasecmp(val, "indigo") == 0) return 0x4b0082;
    if (strcasecmp(val, "ivory") == 0) return 0xfffff0;
    if (strcasecmp(val, "khaki") == 0) return 0xf0e68c;
    if (strcasecmp(val, "lavender") == 0) return 0xe6e6fa;
    if (strcasecmp(val, "lavenderblush") == 0) return 0xfff0f5;
    if (strcasecmp(val, "lawngreen") == 0) return 0x7cfc00;
    if (strcasecmp(val, "lemonchiffon") == 0) return 0xfffacd;
    if (strcasecmp(val, "lightblue") == 0) return 0xadd8e6;
    if (strcasecmp(val, "lightcoral") == 0) return 0xf08080;
    if (strcasecmp(val, "lightcyan") == 0) return 0xe0ffff;
    if (strcasecmp(val, "lightgoldenrodyellow") == 0) return 0xfafad2;
    if (strcasecmp(val, "lightgray") == 0) return 0xd3d3d3;
    if (strcasecmp(val, "lightgreen") == 0) return 0x90ee90;
    if (strcasecmp(val, "lightgrey") == 0) return 0xd3d3d3;
    if (strcasecmp(val, "lightpink") == 0) return 0xffb6c1;
    if (strcasecmp(val, "lightsalmon") == 0) return 0xffa07a;
    if (strcasecmp(val, "lightseagreen") == 0) return 0x20b2aa;
    if (strcasecmp(val, "lightskyblue") == 0) return 0x87cefa;
    if (strcasecmp(val, "lightslategray") == 0) return 0x778899;
    if (strcasecmp(val, "lightslategrey") == 0) return 0x778899;
    if (strcasecmp(val, "lightsteelblue") == 0) return 0xb0c4de;
    if (strcasecmp(val, "lightyellow") == 0) return 0xffffe0;
    if (strcasecmp(val, "lime") == 0) return 0x00ff00;
    if (strcasecmp(val, "limegreen") == 0) return 0x32cd32;
    if (strcasecmp(val, "linen") == 0) return 0xfaf0e6;
    if (strcasecmp(val, "magenta") == 0) return 0xff00ff;
    if (strcasecmp(val, "maroon") == 0) return 0x800000;
    if (strcasecmp(val, "mediumaquamarine") == 0) return 0x66cdaa;
    if (strcasecmp(val, "mediumblue") == 0) return 0x0000cd;
    if (strcasecmp(val, "mediumorchid") == 0) return 0xba55d3;
    if (strcasecmp(val, "mediumpurple") == 0) return 0x9370db;
    if (strcasecmp(val, "mediumseagreen") == 0) return 0x3cb371;
    if (strcasecmp(val, "mediumslateblue") == 0) return 0x7b68ee;
    if (strcasecmp(val, "mediumspringgreen") == 0) return 0x00fa9a;
    if (strcasecmp(val, "mediumturquoise") == 0) return 0x48d1cc;
    if (strcasecmp(val, "mediumvioletred") == 0) return 0xc71585;
    if (strcasecmp(val, "midnightblue") == 0) return 0x191970;
    if (strcasecmp(val, "mintcream") == 0) return 0xf5fffa;
    if (strcasecmp(val, "mistyrose") == 0) return 0xffe4e1;
    if (strcasecmp(val, "moccasin") == 0) return 0xffe4b5;
    if (strcasecmp(val, "navajowhite") == 0) return 0xffdead;
    if (strcasecmp(val, "navy") == 0) return 0x000080;
    if (strcasecmp(val, "oldlace") == 0) return 0xfdf5e6;
    if (strcasecmp(val, "olive") == 0) return 0x808000;
    if (strcasecmp(val, "olivedrab") == 0) return 0x6b8e23;
    if (strcasecmp(val, "orange") == 0) return 0xffa500;
    if (strcasecmp(val, "orangered") == 0) return 0xff4500;
    if (strcasecmp(val, "orchid") == 0) return 0xda70d6;
    if (strcasecmp(val, "palegoldenrod") == 0) return 0xeee8aa;
    if (strcasecmp(val, "palegreen") == 0) return 0x98fb98;
    if (strcasecmp(val, "paleturquoise") == 0) return 0xafeeee;
    if (strcasecmp(val, "palevioletred") == 0) return 0xdb7093;
    if (strcasecmp(val, "papayawhip") == 0) return 0xffefd5;
    if (strcasecmp(val, "peachpuff") == 0) return 0xffdab9;
    if (strcasecmp(val, "peru") == 0) return 0xcd853f;
    if (strcasecmp(val, "pink") == 0) return 0xffc0cb;
    if (strcasecmp(val, "plum") == 0) return 0xdda0dd;
    if (strcasecmp(val, "powderblue") == 0) return 0xb0e0e6;
    if (strcasecmp(val, "purple") == 0) return 0x800080;
    if (strcasecmp(val, "rebeccapurple") == 0) return 0x663399;
    if (strcasecmp(val, "red") == 0) return 0xff0000;
    if (strcasecmp(val, "rosybrown") == 0) return 0xbc8f8f;
    if (strcasecmp(val, "royalblue") == 0) return 0x4169e1;
    if (strcasecmp(val, "saddlebrown") == 0) return 0x8b4513;
    if (strcasecmp(val, "salmon") == 0) return 0xfa8072;
    if (strcasecmp(val, "sandybrown") == 0) return 0xf4a460;
    if (strcasecmp(val, "seagreen") == 0) return 0x2e8b57;
    if (strcasecmp(val, "seashell") == 0) return 0xfff5ee;
    if (strcasecmp(val, "sienna") == 0) return 0xa0522d;
    if (strcasecmp(val, "silver") == 0) return 0xc0c0c0;
    if (strcasecmp(val, "skyblue") == 0) return 0x87ceeb;
    if (strcasecmp(val, "slateblue") == 0) return 0x6a5acd;
    if (strcasecmp(val, "slategray") == 0) return 0x708090;
    if (strcasecmp(val, "slategrey") == 0) return 0x708090;
    if (strcasecmp(val, "snow") == 0) return 0xfffafa;
    if (strcasecmp(val, "springgreen") == 0) return 0x00ff7f;
    if (strcasecmp(val, "steelblue") == 0) return 0x4682b4;
    if (strcasecmp(val, "tan") == 0) return 0xd2b48c;
    if (strcasecmp(val, "teal") == 0) return 0x008080;
    if (strcasecmp(val, "thistle") == 0) return 0xd8bfd8;
    if (strcasecmp(val, "tomato") == 0) return 0xff6347;
    if (strcasecmp(val, "turquoise") == 0) return 0x40e0d0;
    if (strcasecmp(val, "violet") == 0) return 0xee82ee;
    if (strcasecmp(val, "wheat") == 0) return 0xf5deb3;
    if (strcasecmp(val, "white") == 0) return 0xffffff;
    if (strcasecmp(val, "whitesmoke") == 0) return 0xf5f5f5;
    if (strcasecmp(val, "yellow") == 0) return 0xffff00;
    if (strcasecmp(val, "yellowgreen") == 0) return 0x9acd32;

    // Unknown named color
    return 0;
}
#endif
