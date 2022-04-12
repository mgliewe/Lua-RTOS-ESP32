#include "includes.h"
#include "dbutil.h"
#include "buffer.h"
#include "ecdsa.h"
#include "genrsa.h"
#include "gendss.h"
#include "signkey.h"
#include "dbrandom.h"

#include <stdio.h>

#define RSA_DEFAULT_SIZE 2048
#define DSS_DEFAULT_SIZE 1024

/* Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int buf_writefile(buffer * buf, const char * filename) {
#if 0
	int ret = DROPBEAR_FAILURE;
	int fd = -1;

	fd = open(filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		dropbear_log(LOG_ERR, "Couldn't create new file %s: %s",
			filename, strerror(errno));
		goto out;
	}

	/* write the file now */
	while (buf->pos != buf->len) {
		int len = write(fd, buf_getptr(buf, buf->len - buf->pos),
				buf->len - buf->pos);
		if (len == -1 && errno == EINTR) {
			continue;
		}
		if (len <= 0) {
			dropbear_log(LOG_ERR, "Failed writing file %s: %s",
				filename, strerror(errno));
			goto out;
		}
		buf_incrpos(buf, len);
	}

	ret = DROPBEAR_SUCCESS;

out:
	if (fd >= 0) {
		if (fsync(fd) != 0) {
			dropbear_log(LOG_ERR, "fsync of %s failed: %s", filename, strerror(errno));
		}
		m_close(fd);
	}
	return ret;
#else
	int ret = DROPBEAR_FAILURE;
	FILE *fd = 0;

	fd = fopen(filename, "w");
	if (!fd) {
		dropbear_log(LOG_ERR, "Couldn't create new file %s: %s",
			filename, strerror(errno));
		goto out;
	}

	/* write the file now */
	while (buf->pos != buf->len) {
		int len = fwrite(buf_getptr(buf, buf->len - buf->pos),
				1, buf->len - buf->pos, fd);
		if (len <= 0) {
			dropbear_log(LOG_ERR, "Failed writing file %s: %s",
				filename, strerror(errno));
			goto out;
		}
		buf_incrpos(buf, len);
	}

	ret = DROPBEAR_SUCCESS;
out:
	if (fd) {
		fclose(fd);
	}
	return ret;
#endif
}

/* returns 0 on failure */
static int get_default_bits(enum signkey_type keytype)
{
	switch (keytype) {
#ifdef DROPBEAR_RSA
		case DROPBEAR_SIGNKEY_RSA:
			return RSA_DEFAULT_SIZE;
#endif
#ifdef DROPBEAR_DSS
		case DROPBEAR_SIGNKEY_DSS:
			return DSS_DEFAULT_SIZE;
#endif
#ifdef DROPBEAR_ECDSA
		case DROPBEAR_SIGNKEY_ECDSA_KEYGEN:
			return ECDSA_DEFAULT_SIZE;
		case DROPBEAR_SIGNKEY_ECDSA_NISTP521:
			return 521;
		case DROPBEAR_SIGNKEY_ECDSA_NISTP384:
			return 384;
		case DROPBEAR_SIGNKEY_ECDSA_NISTP256:
			return 256;
#endif
		default:
			return 0;
	}
}

/* if skip_exist is set it will silently return if the key file exists */
int signkey_generate(enum signkey_type keytype, int bits, const char* filename, int skip_exist)
{
	sign_key * key = NULL;
	buffer *buf = NULL;

	int ret = DROPBEAR_FAILURE;
	if (bits == 0)
	{
		bits = get_default_bits(keytype);
	}

	if (skip_exist) {
		// If file exists, exit
		FILE * file;
		file = fopen(filename, "r");
		if (file) {
			dropbear_log(LOG_DEBUG, "%s key file exists", filename);

			fclose(file);
			return DROPBEAR_SUCCESS;
		}

		fclose(file);
	}

	dropbear_log(LOG_INFO, "generating %s key file, this may take a while...", filename);

	/* now we can generate the key */
	key = new_sign_key();

	seedrandom();

	switch(keytype) {
#ifdef DROPBEAR_RSA
		case DROPBEAR_SIGNKEY_RSA:
			key->rsakey = gen_rsa_priv_key(bits);
			break;
#endif
#ifdef DROPBEAR_DSS
		case DROPBEAR_SIGNKEY_DSS:
			key->dsskey = gen_dss_priv_key(bits);
			break;
#endif
#ifdef DROPBEAR_ECDSA
		case DROPBEAR_SIGNKEY_ECDSA_KEYGEN:
		case DROPBEAR_SIGNKEY_ECDSA_NISTP521:
		case DROPBEAR_SIGNKEY_ECDSA_NISTP384:
		case DROPBEAR_SIGNKEY_ECDSA_NISTP256:
			{
				ecc_key *ecckey = gen_ecdsa_priv_key(bits);
				keytype = ecdsa_signkey_type(ecckey);
				*signkey_key_ptr(key, keytype) = ecckey;
			}
			break;
#endif
		default:
			dropbear_exit("Internal error");
	}

	seedrandom();

	buf = buf_new(MAX_PRIVKEY_SIZE); 

	buf_put_priv_key(buf, key, keytype);
	sign_key_free(key);
	key = NULL;
	buf_setpos(buf, 0);

	ret = buf_writefile(buf, filename);

	if (ret == DROPBEAR_FAILURE) {
		dropbear_log(LOG_ERR, "%s key file NOT generated", filename);
		goto out;
	}
	dropbear_log(LOG_INFO, "%s key file generated", filename);

out:
	if (buf) {
		buf_burn(buf);
		buf_free(buf);
	}
	
	return ret;
}
