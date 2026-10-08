/*
 * This program is copyright Alec Muffett 1993. The author disclaims all 
 * responsibility or liability with respect to it's usage or its effect 
 * upon hardware or computer systems, and maintains copyright as set out 
 * in the "LICENCE" document which accompanies distributions of Crack v4.0 
 * and upwards.
 */

#include "packer.h"

static char vers_id[] = "packlib.c : v2.3p2 Alec Muffett 18 May 1993";

/* On-disk fields are 32-bit little-endian regardless of sizeof(int32). */
#define PI_HEADER_DISK_SIZE 12

static int32 GetU32(const unsigned char *b)
{
    return (int32) b[0] | ((int32) b[1] << 8) | ((int32) b[2] << 16) | ((int32) b[3] << 24);
}

static void PutU32(unsigned char *b, int32 v)
{
    b[0] = (unsigned char) (v & 0xff);
    b[1] = (unsigned char) ((v >> 8) & 0xff);
    b[2] = (unsigned char) ((v >> 16) & 0xff);
    b[3] = (unsigned char) ((v >> 24) & 0xff);
}

static size_t ReadU32(FILE *fp, int32 *out, size_t n)
{
    unsigned char b[4];
    size_t i;
    for (i = 0; i < n && fread(b, 1, 4, fp) == 4; i++)
	out[i] = GetU32(b);
    return i;
}

static size_t WriteU32(FILE *fp, const int32 *in, size_t n)
{
    unsigned char b[4];
    size_t i;
    for (i = 0; i < n; i++)
    {
	PutU32(b, in[i]);
	if (fwrite(b, 1, 4, fp) != 4)
	    break;
    }
    return i;
}

static int ReadHeader(FILE *fp, struct pi_header *h)
{
    unsigned char b[PI_HEADER_DISK_SIZE];
    if (fread(b, 1, sizeof(b), fp) != sizeof(b))
	return 0;
    h->pih_magic = GetU32(b);
    h->pih_numwords = GetU32(b + 4);
    h->pih_blocklen = (int16) (b[8] | (b[9] << 8));
    h->pih_pad = (int16) (b[10] | (b[11] << 8));
    return 1;
}

static int WriteHeader(FILE *fp, const struct pi_header *h)
{
    unsigned char b[PI_HEADER_DISK_SIZE];
    PutU32(b, h->pih_magic);
    PutU32(b + 4, h->pih_numwords);
    b[8] = (unsigned char) (h->pih_blocklen & 0xff);
    b[9] = (unsigned char) ((h->pih_blocklen >> 8) & 0xff);
    b[10] = (unsigned char) (h->pih_pad & 0xff);
    b[11] = (unsigned char) ((h->pih_pad >> 8) & 0xff);
    return fwrite(b, 1, sizeof(b), fp) == sizeof(b);
}

PWDICT *
PWOpen(prefix, mode)
    char *prefix;
    char *mode;
{
    static PWDICT pdesc;
    char iname[STRINGSIZE];
    char dname[STRINGSIZE];
    char wname[STRINGSIZE];
    FILE *dfp;
    FILE *ifp;
    FILE *wfp;

    if (pdesc.header.pih_magic == PIH_MAGIC)
    {
	fprintf(stderr, "%s: another dictionary already open\n", prefix);
	return ((PWDICT *) 0);
    }

    memset(&pdesc, '\0', sizeof(pdesc));

    sprintf(iname, "%s.pwi", prefix);
    sprintf(dname, "%s.pwd", prefix);
    sprintf(wname, "%s.hwm", prefix);

    if (!(pdesc.dfp = fopen(dname, mode)))
    {
	perror(dname);
	return ((PWDICT *) 0);
    }

    if (!(pdesc.ifp = fopen(iname, mode)))
    {
	fclose(pdesc.dfp);
	perror(iname);
	return ((PWDICT *) 0);
    }

    if (pdesc.wfp = fopen(wname, mode))
    {
	pdesc.flags |= PFOR_USEHWMS;
    }

    ifp = pdesc.ifp;
    dfp = pdesc.dfp;
    wfp = pdesc.wfp;

    if (mode[0] == 'w')
    {
	pdesc.flags |= PFOR_WRITE;
	pdesc.header.pih_magic = PIH_MAGIC;
	pdesc.header.pih_blocklen = NUMWORDS;
	pdesc.header.pih_numwords = 0;

	WriteHeader(ifp, &pdesc.header);
    } else
    {
	pdesc.flags &= ~PFOR_WRITE;

	if (!ReadHeader(ifp, &pdesc.header))
	{
	    fprintf(stderr, "%s: error reading header\n", prefix);

	    pdesc.header.pih_magic = 0;
	    fclose(ifp);
	    fclose(dfp);
	    return ((PWDICT *) 0);
	}

	if (pdesc.header.pih_magic != PIH_MAGIC)
	{
	    fprintf(stderr, "%s: magic mismatch\n", prefix);

	    pdesc.header.pih_magic = 0;
	    fclose(ifp);
	    fclose(dfp);
	    return ((PWDICT *) 0);
	}

	if (pdesc.header.pih_blocklen != NUMWORDS)
	{
	    fprintf(stderr, "%s: size mismatch\n", prefix);

	    pdesc.header.pih_magic = 0;
	    fclose(ifp);
	    fclose(dfp);
	    return ((PWDICT *) 0);
	}

	if (pdesc.flags & PFOR_USEHWMS)
	{
	    if (ReadU32(wfp, pdesc.hwms, 256) != 256)
	    {
		pdesc.flags &= ~PFOR_USEHWMS;
	    }
	}
    }

    return (&pdesc);
}

int
PWClose(pwp)
    PWDICT *pwp;
{
    if (pwp->header.pih_magic != PIH_MAGIC)
    {
	fprintf(stderr, "PWClose: close magic mismatch\n");
	return (-1);
    }

    if (pwp->flags & PFOR_WRITE)
    {
	pwp->flags |= PFOR_FLUSH;
	PutPW(pwp, (char *) 0);	/* flush last index if necess */

	if (fseek(pwp->ifp, 0L, 0))
	{
	    fprintf(stderr, "index magic fseek failed\n");
	    return (-1);
	}

	if (!WriteHeader(pwp->ifp, &pwp->header))
	{
	    fprintf(stderr, "index magic fwrite failed\n");
	    return (-1);
	}

	if (pwp->flags & PFOR_USEHWMS)
	{
	    int i;
	    for (i=1; i<=0xff; i++)
	    {
	    	if (!pwp->hwms[i])
	    	{
	    	    pwp->hwms[i] = pwp->hwms[i-1];
	    	}
#ifdef DEBUG
	    	printf("hwm[%02x] = %d\n", i, pwp->hwms[i]);
#endif
	    }
	    WriteU32(pwp->wfp, pwp->hwms, 256);
	}
    }

    fclose(pwp->ifp);
    fclose(pwp->dfp);

	// TEST
	if (pwp->wfp)
		fclose(pwp->wfp);

    pwp->header.pih_magic = 0;

    return (0);
}

int
PutPW(pwp, string)
    PWDICT *pwp;
    char *string;
{
    if (!(pwp->flags & PFOR_WRITE))
    {
	return (-1);
    }

    if (string)
    {
	strncpy(pwp->data[pwp->count], string, MAXWORDLEN);
	pwp->data[pwp->count][MAXWORDLEN - 1] = '\0';

	pwp->hwms[string[0] & 0xff]= pwp->header.pih_numwords;

	++(pwp->count);
	++(pwp->header.pih_numwords);

    } else if (!(pwp->flags & PFOR_FLUSH))
    {
	return (-1);
    }

    if ((pwp->flags & PFOR_FLUSH) || !(pwp->count % NUMWORDS))
    {
	int i;
	int32 datum;
	register char *ostr;

	datum = (int32) ftell(pwp->dfp);

	WriteU32(pwp->ifp, &datum, 1);

	fputs(pwp->data[0], pwp->dfp);
	putc(0, pwp->dfp);

	ostr = pwp->data[0];

	for (i = 1; i < NUMWORDS; i++)
	{
	    register int j;
	    register char *nstr;
	    nstr = pwp->data[i];

	    if (nstr[0])
	    {
		for (j = 0; ostr[j] && nstr[j] && (ostr[j] == nstr[j]); j++);
		putc(j & 0xff, pwp->dfp);
		fputs(nstr + j, pwp->dfp);
	    }
	    putc(0, pwp->dfp);

	    ostr = nstr;
	}

	memset(pwp->data, '\0', sizeof(pwp->data));
	pwp->count = 0;
    }
    return (0);
}

char *
GetPW(pwp, number)
    PWDICT *pwp;
    int32 number;
{
    int32 datum;
    register int i;
    register char *ostr;
    register char *nstr;
    register char *bptr;
    char buffer[NUMWORDS * MAXWORDLEN];
    static char data[NUMWORDS][MAXWORDLEN];
    static int32 prevblock = 0xffffffff;
    int32 thisblock;

    thisblock = number / NUMWORDS;

    if (prevblock == thisblock)
    {
	return (data[number % NUMWORDS]);
    }
    if (fseek(pwp->ifp, (long) (PI_HEADER_DISK_SIZE + thisblock * 4), 0))
    {
	perror("(index fseek failed)");
	return ((char *) 0);
    }

    if (!ReadU32(pwp->ifp, &datum, 1))
    {
	perror("(index fread failed)");
	if(ferror(pwp->ifp))
		perror("File error");
	if(feof(pwp->ifp))
		perror("End of file error");
	return ((char *) 0);
    }

    if (fseek(pwp->dfp, datum, 0))
    {
	perror("(data fseek failed)");
	return ((char *) 0);
    }

	memset(buffer, 0, sizeof(buffer));
    if (!fread(buffer, 1, sizeof(buffer), pwp->dfp))
    {
	perror("(data fread failed)");
	return ((char *) 0);
    }

    prevblock = thisblock;

    bptr = buffer;

    for (ostr = data[0]; *(ostr++) = *(bptr++); /* nothing */ );

    ostr = data[0];

    for (i = 1; i < NUMWORDS; i++)
    {
	nstr = data[i];
	strcpy(nstr, ostr);

	ostr = nstr + *(bptr++);
	while (*(ostr++) = *(bptr++));

	ostr = nstr;
    }

    return (data[number % NUMWORDS]);
}

int32
FindPW(pwp, string)
    PWDICT *pwp;
    char *string;
{
    register int32 lwm;
    register int32 hwm;
    register int32 middle;
    register char *this;
    int idx;

    if (pwp->flags & PFOR_USEHWMS)
    {
	idx = string[0] & 0xff;
    	lwm = idx ? pwp->hwms[idx - 1] : 0;
    	hwm = pwp->hwms[idx];
    } else
    {
    	lwm = 0;
    	hwm = PW_WORDS(pwp) - 1;
    }

#ifdef DEBUG
    printf("---- %lu, %lu ----\n", lwm, hwm);
#endif

    for (;;)
    {
	int cmp;

#ifdef DEBUG
	printf("%lu, %lu\n", lwm, hwm);
#endif

	middle = lwm + ((hwm - lwm + 1) / 2);

	if (middle == hwm)
	{
	    break;
	}

	this = GetPW(pwp, middle);
	if(!this)
		cmp = strcmp(string, "");
	else
		cmp = strcmp(string, this);		/* INLINE ? */

	if (cmp < 0)
	{
	    hwm = middle;
	} else if (cmp > 0)
	{
	    lwm = middle;
	} else
	{
	    return (middle);
	}
    }

    return (PW_WORDS(pwp));
}
