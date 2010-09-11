#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

#include "log.h"
#include "pwdb.h"

#include "radius.h"


static uint8_t* encrypt_password(const char *passwd, const char *secret, const uint8_t *RA, int *epasswd_len)
{
	uint8_t *epasswd;
	int i, j, chunk_cnt;
	uint8_t b[16], c[16];
	MD5_CTX ctx;
	
	chunk_cnt = (strlen(passwd) - 1) / 16 + 1;
	
	epasswd = malloc(chunk_cnt * 16);
	if (!epasswd) {
		log_emerg("radius: out of memory\n");
		return NULL;
	}

	memset(epasswd, 0, chunk_cnt * 16);
	memcpy(epasswd, passwd, strlen(passwd));
	memcpy(c, RA, 16);

	for (i = 0; i < chunk_cnt; i++) {
		MD5_Init(&ctx);
		MD5_Update(&ctx, secret, strlen(secret));
		MD5_Update(&ctx, c, 16);
		MD5_Final(b, &ctx);
	
		for(j = 0; j < 16; j++)
			epasswd[i * 16 + j] ^= b[j];

		memcpy(c, epasswd + i * 16, 16);
	}

	*epasswd_len = chunk_cnt * 16;
	return epasswd;
}

int rad_auth_pap(struct radius_pd_t *rpd, const char *username, va_list args)
{
	struct rad_req_t *req;
	int i, r = PWDB_DENIED;
	//int id = va_arg(args, int);
	const char *passwd = va_arg(args, const char *);
	uint8_t *epasswd;
	int epasswd_len;

	req = rad_req_alloc(rpd, CODE_ACCESS_REQUEST, username);
	if (!req)
		return PWDB_DENIED;
	
	epasswd = encrypt_password(passwd, conf_auth_secret, req->RA, &epasswd_len);
	if (!epasswd)
		goto out;

	if (rad_packet_add_octets(req->pack, "Password", epasswd, epasswd_len)) {
		free(epasswd);
		goto out;
	}

	free(epasswd);

	for(i = 0; i < conf_max_try; i++) {
		if (rad_req_send(req))
			goto out;

		rad_req_wait(req, conf_timeout);

		if (req->reply) {
			if (req->reply->id != req->pack->id) {
				rad_packet_free(req->reply);
				req->reply = NULL;
			} else
				break;
		}
	}

	if (req->reply && req->reply->code == CODE_ACCESS_ACCEPT) {
		rad_proc_attrs(req);
		r = PWDB_SUCCESS;
	}

out:
	rad_req_free(req);

	return r;
}

int rad_auth_chap_md5(struct radius_pd_t *rpd, const char *username, va_list args)
{
	struct rad_req_t *req;
	int i, r = PWDB_DENIED;
	uint8_t chap_password[17];
	
	int id = va_arg(args, int);
	uint8_t *challenge = va_arg(args, uint8_t *);
	int challenge_len = va_arg(args, int);
	uint8_t *response = va_arg(args, uint8_t *);

	chap_password[0] = id;
	memcpy(chap_password + 1, response, 16);

	req = rad_req_alloc(rpd, CODE_ACCESS_REQUEST, username);
	if (!req)
		return PWDB_DENIED;
	
	if (challenge_len == 16)
		memcpy(req->RA, challenge, 16);
	else {
		if (rad_packet_add_octets(req->pack, "CHAP-Challenge", challenge, challenge_len))
			goto out;
	}

	if (rad_packet_add_octets(req->pack, "CHAP-Password", chap_password, 17))
		goto out;

	for(i = 0; i < conf_max_try; i++) {
		if (rad_req_send(req))
			goto out;

		rad_req_wait(req, conf_timeout);

		if (req->reply) {
			if (req->reply->id != req->pack->id) {
				rad_packet_free(req->reply);
				req->reply = NULL;
			} else
				break;
		}
	}

	if (!req->reply)
		log_ppp_warn("radius:auth: no response\n");
	else if (req->reply->code == CODE_ACCESS_ACCEPT) {
		rad_proc_attrs(req);
		r = PWDB_SUCCESS;
	}

out:
	rad_req_free(req);

	return r;
}

int rad_auth_mschap_v1(struct radius_pd_t *rpd, const char *username, va_list args)
{
	/*int id = va_arg(args, int);
	const uint8_t *challenge = va_arg(args, const uint8_t *);
	const uint8_t *lm_response = va_arg(args, const uint8_t *);
	const uint8_t *nt_response = va_arg(args, const uint8_t *);
	int flags = va_arg(args, int);*/
	return PWDB_NO_IMPL;
}

int rad_auth_mschap_v2(struct radius_pd_t *rpd, const char *username, va_list args)
{
	/*int id = va_arg(args, int);
	const uint8_t *challenge = va_arg(args, const uint8_t *);
	const uint8_t *peer_challenge = va_arg(args, const uint8_t *);
	const uint8_t *response = va_arg(args, const uint8_t *);
	int flags = va_arg(args, int);
	uint8_t *authenticator = va_arg(args, uint8_t *);*/
	return PWDB_NO_IMPL;
}


