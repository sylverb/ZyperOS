/* Zip content, because that is how launchers ship ROM libraries.
 *
 * ZyperOS's entire library is zipped - one ROM per archive, No-Intro style -
 * and minarch handled that, so Diatom meeting its first real launcher failed
 * on the first game it was handed. The register's line about protodrive
 * testing "what the author expected" earned its keep in under a minute.
 *
 * Scope is deliberately the launcher case and nothing more: ONE interesting
 * entry per archive, chosen as the largest file, stored, deflated or LZMA
 * (7-Zip's zip method 14, through liblzma, which plastron has for the CHD
 * reader; 1.0.7). Bzip2 (12) and Zstandard (93) are not read: their
 * libraries are not in the system. The largest entry is the ROM whatever its
 * method, so a zip whose ROM this cannot read is refused, never another of
 * its files; the launcher says why before it starts one (src/gamefile.h). Multi-
 * file archives (cue+bin) ship unzipped for the same reason they do on every
 * other firmware: the cue names its bin by path.
 *
 * A deflated entry goes through zlib; a stored one is copied as it is.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <lzma.h>
#include <zlib.h>

#include "diatom.h"

/* A zip's LZMA entry (method 14, APPNOTE 5.8.8): two bytes of the encoder's
 * version, the properties' size (5), the LZMA1 properties, then the raw
 * stream, with an end marker or without one. Decoded into `out` until it
 * holds `usize` bytes: the size the directory gave says where it ends, the
 * marker or none. */
static bool lzma_entry(const unsigned char *comp, size_t csize, unsigned char *out, size_t usize)
{
	lzma_stream z = LZMA_STREAM_INIT;
	lzma_filter filters[2] = { { LZMA_FILTER_LZMA1, NULL }, { LZMA_VLI_UNKNOWN, NULL } };
	size_t props;
	lzma_ret rc;

	if (csize < 4) return false;
	props = (size_t)comp[2] | (size_t)comp[3] << 8;
	if (props != 5 || csize < 4 + props) return false;
	if (lzma_properties_decode(&filters[0], NULL, comp + 4, props) != LZMA_OK) return false;
	rc = lzma_raw_decoder(&z, filters);
	free(filters[0].options);
	if (rc != LZMA_OK) return false;
	z.next_in = comp + 4 + props;
	z.avail_in = csize - 4 - props;
	z.next_out = out;
	z.avail_out = usize;
	do rc = lzma_code(&z, LZMA_FINISH);
	while (rc == LZMA_OK && z.avail_out > 0 && z.avail_in > 0);
	lzma_end(&z);
	return z.total_out == usize && (rc == LZMA_OK || rc == LZMA_STREAM_END || rc == LZMA_BUF_ERROR);
}

static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const unsigned char *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool diatom_zip_is(const char *path)
{
	size_t n = path ? strlen(path) : 0;
	return n > 4 && strcasecmp(path + n - 4, ".zip") == 0;
}

/* Largest entry in the archive, extracted whole. */
bool diatom_zip_load(const char *path, void **out, size_t *out_len,
                     char *name, size_t name_n)
{
	FILE *f = fopen(path, "rb");
	unsigned char tail[66000 + 22], hdr[46], lh[30];
	long fsize, tail_off;
	size_t tail_len, i;
	uint32_t cd_off = 0, cd_n = 0;
	int found = 0;
	uint32_t best_usize = 0, best_csize = 0, best_lho = 0;
	uint16_t best_method = 0;
	char best_name[512] = "";

	*out = NULL; *out_len = 0;
	if (name && name_n) name[0] = '\0';
	if (!f) return false;

	/* End-of-central-directory: within the last 64K + comment slack. The
	 * record alone is 22 bytes, and a file shorter than that is no zip: the
	 * scan below starts at tail_len - 22 + 1 in a size_t, which wrapped for
	 * one, and a 0-byte .zip ended Diatom with SIGSEGV. ftell's -1 too. */
	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	if (fsize < 22) { fclose(f); return false; }
	tail_len = fsize < (long)sizeof tail ? (size_t)fsize : sizeof tail;
	tail_off = fsize - (long)tail_len;
	fseek(f, tail_off, SEEK_SET);
	if (fread(tail, 1, tail_len, f) != tail_len) { fclose(f); return false; }
	for (i = tail_len - 22 + 1; i-- > 0; ) {
		if (tail[i] == 'P' && tail[i+1] == 'K' && tail[i+2] == 5 && tail[i+3] == 6) {
			cd_n   = rd16(tail + i + 10);
			cd_off = rd32(tail + i + 16);
			found = 1;
			break;
		}
	}
	if (!found || !cd_n) { fclose(f); return false; }

	fseek(f, (long)cd_off, SEEK_SET);
	for (i = 0; i < cd_n; i++) {
		char en[512];
		uint16_t nlen, elen, clen, method;
		uint32_t csize, usize, lho;

		if (fread(hdr, 1, 46, f) != 46 || rd32(hdr) != 0x02014b50) break;
		method = rd16(hdr + 10);
		csize  = rd32(hdr + 20);
		usize  = rd32(hdr + 24);
		nlen   = rd16(hdr + 28);
		elen   = rd16(hdr + 30);
		clen   = rd16(hdr + 32);
		lho    = rd32(hdr + 42);

		if (nlen >= sizeof en) { fclose(f); return false; }
		if (fread(en, 1, nlen, f) != nlen) break;
		en[nlen] = '\0';
		fseek(f, elen + clen, SEEK_CUR);

		if (nlen == 0) continue;                                /* no name to give it */
		if (usize == 0 || en[nlen - 1] == '/') continue;        /* directory */
		/* The largest entry whatever its method: the ROM, which a zip of
		 * one game holds beside its readme; one this cannot read is
		 * refused below rather than another file given in its place */
		if (usize > best_usize) {
			best_usize = usize; best_csize = csize;
			best_method = method; best_lho = lho;
			snprintf(best_name, sizeof best_name, "%s", en);
		}
	}
	if (!best_usize) { fclose(f); return false; }
	if (best_method != 0 && best_method != 8 && best_method != 14) {
		fprintf(stderr, "diatom: zip: %s is compressed by method %u, which this cannot read\n",
		        best_name, (unsigned)best_method);
		fclose(f);
		return false;
	}

	/* The local header repeats the name/extra lengths; the data follows it. */
	fseek(f, (long)best_lho, SEEK_SET);
	if (fread(lh, 1, 30, f) != 30 || rd32(lh) != 0x04034b50) { fclose(f); return false; }
	fseek(f, rd16(lh + 26) + rd16(lh + 28), SEEK_CUR);

	{
		unsigned char *comp = malloc(best_csize ? best_csize : 1);
		unsigned char *un   = malloc(best_usize);

		if (!comp || !un) { free(comp); free(un); fclose(f); return false; }
		if (fread(comp, 1, best_csize, f) != best_csize) {
			free(comp); free(un); fclose(f); return false;
		}
		fclose(f);

		if (best_method == 0) {
			/* Stored holds what it says it holds, or it is no zip: the
			 * copy would read past comp. */
			if (best_csize < best_usize) {
				fprintf(stderr, "diatom: zip: %s holds %u of its %u bytes\n",
				        best_name, (unsigned)best_csize, (unsigned)best_usize);
				free(comp); free(un); return false;
			}
			memcpy(un, comp, best_usize);
		} else if (best_method == 14) {
			if (!lzma_entry(comp, best_csize, un, best_usize)) {
				fprintf(stderr, "diatom: zip: LZMA failed on %s\n", best_name);
				free(comp); free(un); return false;
			}
		} else {
			z_stream z;
			int rc;

			memset(&z, 0, sizeof z);
			z.next_in   = comp;
			z.avail_in  = best_csize;
			z.next_out  = un;
			z.avail_out = best_usize;
			/* -15: raw deflate, no zlib wrapper - what zip stores. */
			if (inflateInit2(&z, -15) != Z_OK) {
				free(comp); free(un); return false;
			}
			rc = inflate(&z, Z_FINISH);
			inflateEnd(&z);
			if (rc != Z_STREAM_END || z.total_out != best_usize) {
				fprintf(stderr, "diatom: zip: inflate failed on %s\n", best_name);
				free(comp); free(un); return false;
			}
		}
		free(comp);
		*out = un;
		*out_len = best_usize;
		if (name && name_n) snprintf(name, name_n, "%s", best_name);
		return true;
	}
}
