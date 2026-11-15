/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-boottheme: the words the boot screen's theme and language travel
 * in (src/boottheme.h), as boot.scr and the splash read them. */
#include "../src/boottheme.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(cond, what) do { \
	if (cond) printf("  ok   %s\n", what); \
	else { printf("  FAIL %s\n", what); fails++; } \
} while (0)

int main(void)
{
	char line[BOOTTHEME_MAX], file[BOOTTHEME_MAX], said[BOOTTHEME_MAX];

	printf("check-boottheme:\n");
	boottheme_words("auto", "America/Sao_Paulo", "pt-BR", 74, line, sizeof line, file, sizeof file);
	CHECK(!strcmp(line, "zytheme=auto zyzone=America/Sao_Paulo zylang=pt-BR zybright=74"),
	      "auto, a zone, a language and a brightness, as the kernel line has them");
	CHECK(!strcmp(file, "zytheme=auto\nzyzone=America/Sao_Paulo\nzylang=pt-BR\nzybright=74\n"), "  ...and as theme.txt, a line each");
	boottheme_words("dark", "Etc/GMT+3", "zh-Hans", -1, line, sizeof line, NULL, 0);
	CHECK(!strcmp(line, "zytheme=dark zyzone=Etc/GMT+3 zylang=zh-Hans zybright=-"),
	      "dark, a zone with a +, Chinese, no brightness kept: -");
	boottheme_words("sepia", "UTC", NULL, 0, line, sizeof line, NULL, 0);
	CHECK(!strcmp(line, "zytheme=light zyzone=UTC zylang=en zybright=-"),
	      "a word the launcher does not know is light, as it reads it; no language is en; 0 is no light, so none");
	boottheme_words(NULL, "Bad Zone", "pt BR", 255, line, sizeof line, NULL, 0);
	CHECK(!strcmp(line, "zytheme=light zyzone=- zylang=en zybright=255"), "a zone or a language with a space is - or en: one word on the line");
	boottheme_words("light", "x\nzytheme=dark", "fr\nzytheme=dark", 17, line, sizeof line, file, sizeof file);
	CHECK(!strcmp(file, "zytheme=light\nzyzone=-\nzylang=en\nzybright=17\n"), "a zone or a language cannot add a line to theme.txt");
	boottheme_words("light", "America/Argentina/ComodRivadavia", "zh-Hans", 255, line, sizeof line, file, sizeof file);
	CHECK(strlen(line) < sizeof line - 1 && strstr(line, " zybright=255") && strstr(file, "zybright=255\n"),
	      "the longest zone and language still leave room for the brightness");

	boottheme_from_cmdline("root=/dev/mmcblk0p2 ro zyperos.slot=a zyperos.trial=0 zytheme=auto zyzone=America/Sao_Paulo zylang=fr",
	                       said, sizeof said);
	CHECK(!strcmp(said, "zytheme=auto zyzone=America/Sao_Paulo zylang=fr"), "the words back from a boot.scr from before the brightness");
	boottheme_words("auto", "America/Sao_Paulo", "fr", 74, line, sizeof line, NULL, 0);
	CHECK(boottheme_same(line, said), "  ...the brightness is not a difference it could carry: nothing to write");
	boottheme_words("auto", "America/Sao_Paulo", "es", 74, line, sizeof line, NULL, 0);
	CHECK(!boottheme_same(line, said), "  ...another language is a write");

	boottheme_from_cmdline("root=/dev/mmcblk0p2 zytheme=auto zyzone=UTC zylang=fr zybright=74 quiet", said, sizeof said);
	CHECK(!strcmp(said, "zytheme=auto zyzone=UTC zylang=fr zybright=74"), "the four words back from this boot.scr's line");
	boottheme_words("auto", "UTC", "fr", 74, line, sizeof line, NULL, 0);
	CHECK(boottheme_same(line, said), "  ...the same brightness: nothing to write");
	boottheme_words("auto", "UTC", "fr", 36, line, sizeof line, NULL, 0);
	CHECK(!boottheme_same(line, said), "  ...another brightness is a write");
	boottheme_from_cmdline("root=/dev/mmcblk0p2 zytheme=auto zyzone=UTC zylang=fr zybright=-", said, sizeof said);
	CHECK(!boottheme_same(line, said), "  ...and none kept yet (-) is a write, so the next start has it");
	boottheme_words("auto", "America/Sao_Paulo", "es", 74, line, sizeof line, NULL, 0);
	boottheme_from_cmdline("root=/dev/mmcblk0p2 zytheme=auto zyzone=America/Sao_Paulo", said, sizeof said);
	CHECK(!strcmp(said, "zytheme=auto zyzone=America/Sao_Paulo"), "a boot.scr from before the language: its two words");
	CHECK(boottheme_same(line, said), "  ...and the language and the brightness are no difference it could carry: nothing to write");
	boottheme_words("dark", "America/Sao_Paulo", "es", 74, line, sizeof line, NULL, 0);
	CHECK(!boottheme_same(line, said), "  ...but the theme still is");
	boottheme_from_cmdline("root=/dev/mmcblk0p2 zyperos.slot=a\n", said, sizeof said);
	CHECK(!said[0], "a line without them (an older boot.scr): \"\"");
	boottheme_from_cmdline("root=/dev/vda2 zyperos.slot=a zytheme=- zyzone=- zylang=- zybright=-", said, sizeof said);
	CHECK(!strcmp(said, "zytheme=- zyzone=- zylang=- zybright=-"), "a boot.scr that found no theme.txt says -: not the settings, so it is written");
	CHECK(!boottheme_same(line, said), "  ...a write");
	boottheme_from_cmdline("xzytheme=dark zytheme=light zyzone=- xzylang=fr", said, sizeof said);
	CHECK(!strcmp(said, "zytheme=light zyzone=-"), "a word inside another is not it");

	if (fails) { printf("check-boottheme: %d failed\n", fails); return 1; }
	printf("check-boottheme: all passed\n");
	return 0;
}
