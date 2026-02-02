/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com> [2012]
 */
#include "c.h"
#include "color-names.h"

#include <ctype.h>

struct ul_color_name {
	const char *name;
	const char *seq;
};

/*
 * qsort/bsearch buddy
 */
static int cmp_color_name(const void *a0, const void *b0)
{
	const struct ul_color_name
		*a = (const struct ul_color_name *) a0,
		*b = (const struct ul_color_name *) b0;
	return strcmp(a->name, b->name);
}

/*
 * Maintains human readable color names
 */
const char *color_sequence_from_colorname(const char *str)
{
	static const struct ul_color_name basic_schemes[] = {
		{ "black",	UL_COLOR_BLACK           },
		{ "blink",      UL_COLOR_BLINK           },
		{ "blue",	UL_COLOR_BLUE            },
		{ "bold",       UL_COLOR_BOLD		 },
		{ "brown",	UL_COLOR_BROWN           },
		{ "cyan",	UL_COLOR_CYAN            },
		{ "darkgray",	UL_COLOR_DARK_GRAY       },
		{ "gray",	UL_COLOR_GRAY            },
		{ "green",	UL_COLOR_GREEN           },
		{ "halfbright", UL_COLOR_HALFBRIGHT	 },
		{ "lightblue",	UL_COLOR_BOLD_BLUE       },
		{ "lightcyan",	UL_COLOR_BOLD_CYAN       },
		{ "lightgray",	UL_COLOR_GRAY            },
		{ "lightgreen", UL_COLOR_BOLD_GREEN      },
		{ "lightmagenta", UL_COLOR_BOLD_MAGENTA  },
		{ "lightred",	UL_COLOR_BOLD_RED        },
		{ "magenta",	UL_COLOR_MAGENTA         },
		{ "red",	UL_COLOR_RED             },
		{ "reset",      UL_COLOR_RESET,          },
		{ "reverse",    UL_COLOR_REVERSE         },
		{ "white",      UL_COLOR_WHITE           },
		{ "yellow",	UL_COLOR_BOLD_YELLOW     }
	};
	struct ul_color_name key = { .name = str };
	const struct ul_color_name *res;

	if (!str)
		return NULL;

	res = bsearch(&key, basic_schemes, ARRAY_SIZE(basic_schemes),
				sizeof(struct ul_color_name),
				cmp_color_name);
	return res ? res->seq : NULL;
}


int color_is_sequence(const char *color)
{
	if (color && *color == 0x1B) {
		size_t len = strlen(color);

		if (len >= 4 &&
		    *(color + 1) == '[' &&
		    isdigit(*(color + 2)) &&
		    *(color + len - 1) == 'm')
		return 1;
	}

	return 0;
}

/* canonicalize sequence for 24bit colors (#RRGGBB or #RGB) */
static char *__color_canonicalize_24bit(const char *str)
{
	unsigned int r, g, b;
	size_t len;
	char *out;
	char rr[3], gg[3], bb[3];

	if (!str || str[0] != '#')
		return NULL;

	len = strlen(str);
	if (len == 7) {
		/* #RRGGBB: two hex digits per channel */
		if (!isxdigit((unsigned char)str[1]) || !isxdigit((unsigned char)str[2]) ||
		    !isxdigit((unsigned char)str[3]) || !isxdigit((unsigned char)str[4]) ||
		    !isxdigit((unsigned char)str[5]) || !isxdigit((unsigned char)str[6]))
			return NULL;
		rr[0] = str[1]; rr[1] = str[2]; rr[2] = '\0';
		gg[0] = str[3]; gg[1] = str[4]; gg[2] = '\0';
		bb[0] = str[5]; bb[1] = str[6]; bb[2] = '\0';
		r = strtoul(rr, NULL, 16);
		g = strtoul(gg, NULL, 16);
		b = strtoul(bb, NULL, 16);
	} else if (len == 4) {
		/* #RGB: one hex digit per channel, expand to 0-255 */
		if (!isxdigit((unsigned char)str[1]) || !isxdigit((unsigned char)str[2]) ||
		    !isxdigit((unsigned char)str[3]))
			return NULL;
		rr[0] = rr[1] = str[1]; rr[2] = '\0';
		gg[0] = gg[1] = str[2]; gg[2] = '\0';
		bb[0] = bb[1] = str[3]; bb[2] = '\0';
		r = strtoul(rr, NULL, 16);
		g = strtoul(gg, NULL, 16);
		b = strtoul(bb, NULL, 16);
	} else {
		return NULL;
	}

	if (asprintf(&out, "\033[38;2;%u;%u;%um", r, g, b) < 0)
		return NULL;
	return out;
}

/* canonicalize sequence */
static int __color_canonicalize(const char *str, char **seq)
{
	char *in, *out;
	int len;

	if (!str)
		return -EINVAL;

	*seq = NULL;

	/* convert #RRGGBB or #RGB to 24-bit foreground sequence */
	if (str[0] == '#') {
		size_t n = strlen(str);
		if (n == 4 || n == 7) {
			char *hex_seq = __color_canonicalize_24bit(str);
			if (hex_seq) {
				*seq = hex_seq;
				return 0;
			}
			return -EINVAL;
		}
	}

	/* convert color names like "red" to the real sequence */
	if (*str != '\\' && !strchr(str, ';') && isalpha(*str)) {
		const char *s = color_sequence_from_colorname(str);

		if (s) {
			*seq = strdup(s);
			return *seq ? 0 : -ENOMEM;
		}
		return 1;
	}

	/* convert xx;yy sequences to "\033[xx;yy" */
	if ((len = asprintf(seq, "\033[%sm", str)) < 1)
		return -ENOMEM;

	for (in = *seq, out = *seq; in && *in; in++) {
		if (*in != '\\') {
			*out++ = *in;
			continue;
		}
		switch(*(in + 1)) {
		case 'a':
			*out++ = '\a';	/* Bell */
			break;
		case 'b':
			*out++ = '\b';	/* Backspace */
			break;
		case 'e':
			*out++ = '\033';	/* Escape */
			break;
		case 'f':
			*out++ = '\f';	/* Form Feed */
			break;
		case 'n':
			*out++ = '\n';	/* Newline */
			break;
		case 'r':
			*out++ = '\r';	/* Carriage Return */
			break;
		case 't':
			*out++ = '\t';	/* Tab */
			break;
		case 'v':
			*out++ = '\v';	/* Vertical Tab */
			break;
		case '\\':
			*out++ = '\\';	/* Backslash */
			break;
		case '_':
			*out++ = ' ';	/* Space */
			break;
		case '#':
			*out++ = '#';	/* Hash mark */
			break;
		case '?':
			*out++ = '?';	/* Question mark */
			break;
		default:
			*out++ = *in;
			*out++ = *(in + 1);
			break;
		}
		in++;
	}

	if (out) {
		assert ((out - *seq) <= len);
		*out = '\0';
	}

	return 0;
}

char *color_get_sequence(const char *color)
{
	char *seq = NULL;
	int rc = __color_canonicalize(color, &seq);

	return rc ? NULL : seq;
}
