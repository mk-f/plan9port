#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>

#define STRLEN(s)	(sizeof(s)-1)

uchar*
decodepem(char *s, char *type, int *len)
{
	uchar *d;
	char *t, *e, *tt;
	int n;

	*len = 0;

	/*
	 * find the correct section of the file, stripping garbage at the beginning and end.
	 * the data is delimited by -----BEGIN <type>-----\n and -----END <type>-----\n
	 */
	n = strlen(type);
	e = strchr(s, '\0');
	for(t = s; t != nil && t < e; ){
		tt = t;
		t = strchr(tt, '\n');
		if(t != nil)
			t++;
		if(strncmp(tt, "-----BEGIN ", STRLEN("-----BEGIN ")) == 0
		&& strncmp(&tt[STRLEN("-----BEGIN ")], type, n) == 0
		&& strncmp(&tt[STRLEN("-----BEGIN ")+n], "-----\n", STRLEN("-----\n")) == 0)
			break;
	}
	for(tt = t; tt != nil && tt < e; tt++){
		if(strncmp(tt, "-----END ", STRLEN("-----END ")) == 0
		&& strncmp(&tt[STRLEN("-----END ")], type, n) == 0
		&& strncmp(&tt[STRLEN("-----END ")+n], "-----\n", STRLEN("-----\n")) == 0)
			break;
		tt = strchr(tt, '\n');
		if(tt == nil)
			break;
	}
	if(tt == nil || tt == e){
		werrstr("incorrect .pem file format: bad header or trailer");
		return nil;
	}

	n = ((tt - t) * 6 + 7) / 8;
	d = malloc(n);
	if(d == nil){
		werrstr("out of memory");
		return nil;
	}
	n = dec64(d, n, t, tt - t);
	if(n < 0){
		free(d);
		werrstr("incorrect .pem file format: bad base64 encoded data");
		return nil;
	}
	*len = n;
	return d;
}
