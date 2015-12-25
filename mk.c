#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mail.h"

static int uc_len(char *s)
{
	int c = (unsigned char) s[0];
	if (~c & 0x80)
		return c > 0;
	if (~c & 0x20)
		return 2;
	if (~c & 0x10)
		return 3;
	if (~c & 0x80)
		return 4;
	if (~c & 0x40)
		return 5;
	if (~c & 0x20)
		return 6;
	return 1;
}

static int uc_wid(char *s)
{
	return 1;
}

static char *msg_dec(char *msg, long msz, char *hdr)
{
	char *val = msg_get(msg, msz, hdr);
	char *buf, *ret;
	int val_len;
	if (!val)
		return NULL;
	val_len = hdrlen(val, msg + msz - val) - 1;
	buf = malloc(val_len + 1);
	memcpy(buf, val, val_len);
	buf[val_len] = '\0';
	ret = msg_hdrdec(buf);
	free(buf);
	return ret;
}

static int msg_stat(char *msg, long msz)
{
	char *val = msg_get(msg, msz, "status:");
	if (!val)
		return 'N';
	val += strlen("status:");
	while (isspace((unsigned char) val[0]))
		val++;
	return val[0];
}

static char *fieldformat(char *msg, long msz, char *hdr, int wid)
{
	struct sbuf *dst;
	int dst_wid;
	char *val = msg_dec(msg, msz, hdr);
	char *val0, *end;
	if (!val) {
		val = malloc(1);
		val[0] = '\0';
	}
	val0 = val;
	end = strchr(val, '\0');
	dst = sbuf_make();
	val += strlen(hdr);
	while (val < end && isspace((unsigned char) *val))
		val++;
	dst_wid = 0;
	while (val < end && (wid <= 0 || dst_wid < wid)) {
		int l = uc_len(val);
		if (l == 1) {
			int c = (unsigned char) *val;
			sbuf_chr(dst, isblank(c) || !isprint(c) ? ' ' : c);
		} else {
			sbuf_mem(dst, val, l);
		}
		dst_wid += uc_wid(val);
		val += l;
	}
	if (wid > 0)
		while (dst_wid++ < wid)
			sbuf_chr(dst, ' ');
	free(val0);
	return sbuf_done(dst);
}

static char *segment(char *d, char *s, int m)
{
	char *r = strchr(s, m);
	char *e = r ? r + 1 : strchr(s, '\0');
	memcpy(d, s, e - s);
	d[e - s] = '\0';
	return e;
}

static char *usage =
	"usage: neatmail mk [options] mbox\n\n"
	"options:\n"
	"   -0 fmt \tmessage first line format (e.g., 20from:40subject:)\n"
	"   -1 fmt \tmessage second line format\n"
	"   -f n   \tthe first message to list\n";

int mk(char *argv[])
{
	struct mbox *mbox;
	char *ln[4] = {NULL};
	int i, j;
	int first = 0;
	for (i = 0; argv[i] && argv[i][0] == '-'; i++) {
		if (argv[i][1] == 'f') {
			first = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]);
			continue;
		}
		if (argv[i][1] == '0' || argv[i][1] == '1') {
			int idx = argv[i][1] - '0';
			ln[idx] = argv[i][2] ? argv[i] + 2 : argv[++i];
			continue;
		}
	}
	if (!argv[i]) {
		printf("%s", usage);
		return 1;
	}
	mbox = mbox_open(argv[i]);
	if (!mbox) {
		fprintf(stderr, "neatmail: cannot open <%s>\n", argv[i]);
		return 1;
	}
	for (i = first; i < mbox_len(mbox); i++) {
		char *msg;
		long msz;
		mbox_get(mbox, i, &msg, &msz);
		printf("%c%04d", msg_stat(msg, msz), i);
		for (j = 0; ln[j]; j++) {
			char *cln = ln[j];
			char *tok = malloc(strlen(ln[j]) + 1);
			if (j)
				printf("\n");
			while ((cln = segment(tok, cln, ':')) && tok[0]) {
				char *fmt = tok;
				char *hdr = tok;
				char *val;
				while (isdigit((unsigned char) *hdr))
					hdr++;
				val = fieldformat(msg, msz, hdr, atoi(fmt));
				printf("\t%s", val);
				free(val);
			}
			free(tok);
		}
		printf("\n");
	}
	mbox_free(mbox);
	return 0;
}
