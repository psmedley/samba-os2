/*
   Unix SMB/CIFS implementation.

   Samba KDB plugin for MIT Kerberos

   Copyright (c) 2010      Simo Sorce <idra@samba.org>.
   Copyright (c) 2014-2021 Andreas Schneider <asn@samba.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "lib/replace/replace.h"
#include "lib/replace/system/kerberos.h"
#include "lib/util/data_blob.h"
#include "lib/util/debug.h"
#include "lib/util/fault.h"
#include "lib/util/memory.h"
#include "libcli/util/ntstatus.h"
#include "lib/krb5_wrap/krb5_samba.h"

#include <profile.h>
#include <kdb.h>

#include "kdc/mit_samba.h"
#include "kdb_samba.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_KERBEROS

/* FIXME: This is a krb5 function which is exported, but in no header */
extern krb5_error_code decode_krb5_padata_sequence(const krb5_data *output,
						   krb5_pa_data ***rep);

static krb5_error_code ks_get_netbios_name(krb5_address **addrs, char **name)
{
	char *nb_name = NULL;
	int len, i;

	for (i = 0; addrs[i]; i++) {
		if (addrs[i]->addrtype != ADDRTYPE_NETBIOS) {
			continue;
		}
		len = MIN(addrs[i]->length, 15);
		nb_name = strndup((const char *)addrs[i]->contents, len);
		if (!nb_name) {
			return ENOMEM;
		}
		break;
	}

	if (nb_name) {
		/* Strip space padding */
		i = strlen(nb_name) - 1;
		for (i = strlen(nb_name) - 1;
		     i > 0 && nb_name[i] == ' ';
		     i--) {
			nb_name[i] = '\0';
		}
	}

	*name = nb_name;

	return 0;
}

krb5_error_code kdb_samba_db_check_policy_as(krb5_context context,
					     krb5_kdc_req *kdcreq,
					     krb5_db_entry *client,
					     krb5_db_entry *server,
					     krb5_timestamp kdc_time,
					     const char **status,
					     krb5_pa_data ***e_data_out)
{
	struct mit_samba_context *mit_ctx;
	krb5_error_code code;
	char *client_name = NULL;
	char *server_name = NULL;
	char *netbios_name = NULL;
	char *realm = NULL;
	bool password_change = false;
	krb5_const_principal client_princ;
	DATA_BLOB int_data = { NULL, 0 };
	krb5_data d;
	krb5_pa_data **e_data;

	mit_ctx = ks_get_context(context);
	if (mit_ctx == NULL) {
		return KRB5_KDB_DBNOTINITED;
	}

	/* Prefer canonicalised name from client entry */
	client_princ = client ? client->princ : kdcreq->client;

	if (client_princ == NULL || ks_is_kadmin(context, client_princ)) {
		return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	}

	if (krb5_princ_size(context, kdcreq->server) == 2 &&
	    ks_is_kadmin_changepw(context, kdcreq->server)) {
		code = krb5_get_default_realm(context, &realm);
		if (code) {
			goto done;
		}

		if (ks_data_eq_string(kdcreq->server->realm, realm)) {
			password_change = true;
		}
	}

	code = krb5_unparse_name(context, kdcreq->server, &server_name);
	if (code) {
		goto done;
	}

	code = krb5_unparse_name(context, client_princ, &client_name);
	if (code) {
		goto done;
	}

	if (kdcreq->addresses) {
		code = ks_get_netbios_name(kdcreq->addresses, &netbios_name);
		if (code) {
			goto done;
		}
	}

	code = mit_samba_check_client_access(mit_ctx,
					     client,
					     client_name,
					     server,
					     server_name,
					     netbios_name,
					     password_change,
					     &int_data);

	if (int_data.length && int_data.data) {

		/* make sure the mapped return code is returned - gd */
		int code_tmp;

		d = smb_krb5_data_from_blob(int_data);

		code_tmp = decode_krb5_padata_sequence(&d, &e_data);
		if (code_tmp == 0) {
			*e_data_out = e_data;
		}
	}
done:
	free(realm);
	free(server_name);
	free(client_name);
	free(netbios_name);

	return code;
}

static krb5_error_code ks_get_pac(krb5_context context,
				  uint32_t flags,
				  krb5_db_entry *client,
				  krb5_db_entry *server,
				  krb5_keyblock *client_key,
				  krb5_pac *pac)
{
	struct mit_samba_context *mit_ctx;
	krb5_error_code code;

	mit_ctx = ks_get_context(context);
	if (mit_ctx == NULL) {
		return KRB5_KDB_DBNOTINITED;
	}

	code = mit_samba_get_pac(mit_ctx,
				 context,
				 flags,
				 client,
				 server,
				 client_key,
				 pac);
	if (code != 0) {
		return code;
	}

	return code;
}

#if KRB5_KDB_DAL_MAJOR_VERSION < 9
static krb5_error_code ks_verify_pac(krb5_context context,
				     unsigned int flags,
				     krb5_const_principal client_princ,
				     krb5_db_entry *client,
				     krb5_db_entry *server,
				     krb5_db_entry *krbtgt,
				     krb5_keyblock *server_key,
				     krb5_keyblock *krbtgt_key,
				     krb5_timestamp authtime,
				     krb5_authdata **tgt_auth_data,
				     krb5_pac *pac)
{
	struct mit_samba_context *mit_ctx;
	krb5_authdata **authdata = NULL;
	krb5_pac ipac = NULL;
	DATA_BLOB logon_data = { NULL, 0 };
	krb5_error_code code;

	mit_ctx = ks_get_context(context);
	if (mit_ctx == NULL) {
		return KRB5_KDB_DBNOTINITED;
	}

	/* find the existing PAC, if present */
	code = krb5_find_authdata(context,
				  tgt_auth_data,
				  NULL,
				  KRB5_AUTHDATA_WIN2K_PAC,
				  &authdata);
	if (code != 0) {
		return code;
	}

	/* no pac data */
	if (authdata == NULL) {
		return 0;
	}

	SMB_ASSERT(authdata[0] != NULL);

	if (authdata[1] != NULL) {
		code = KRB5KDC_ERR_BADOPTION; /* XXX */
		goto done;
	}

	code = krb5_pac_parse(context,
			      authdata[0]->contents,
			      authdata[0]->length,
			      &ipac);
	if (code != 0) {
		goto done;
	}

	/* TODO: verify this is correct
	 *
	 * In the constrained delegation case, the PAC is from a service
	 * ticket rather than a TGT; we must verify the server and KDC
	 * signatures to assert that the server did not forge the PAC.
	 */
	if (flags & KRB5_KDB_FLAG_CONSTRAINED_DELEGATION) {
		code = krb5_pac_verify(context,
				       ipac,
				       authtime,
				       client_princ,
				       server_key,
				       krbtgt_key);
	} else {
		code = krb5_pac_verify(context,
				       ipac,
				       authtime,
				       client_princ,
				       krbtgt_key,
				       NULL);
	}
	if (code != 0) {
		goto done;
	}

	/* check and update PAC */
	code = krb5_pac_parse(context,
			      authdata[0]->contents,
			      authdata[0]->length,
			      pac);
	if (code != 0) {
		goto done;
	}

	code = mit_samba_reget_pac(mit_ctx,
				   context,
				   flags,
				   client_princ,
				   client,
				   server,
				   krbtgt,
				   krbtgt_key,
				   pac);

done:
	krb5_free_authdata(context, authdata);
	krb5_pac_free(context, ipac);
	free(logon_data.data);

	return code;
}

krb5_error_code kdb_samba_db_sign_auth_data(krb5_context context,
					    unsigned int flags,
					    krb5_const_principal client_princ,
					    krb5_const_principal server_princ,
					    krb5_db_entry *client,
					    krb5_db_entry *server,
					    krb5_db_entry *krbtgt,
					    krb5_db_entry *local_krbtgt,
					    krb5_keyblock *client_key,
					    krb5_keyblock *server_key,
					    krb5_keyblock *krbtgt_key,
					    krb5_keyblock *local_krbtgt_key,
					    krb5_keyblock *session_key,
					    krb5_timestamp authtime,
					    krb5_authdata **tgt_auth_data,
					    void *authdata_info,
					    krb5_data ***auth_indicators,
					    krb5_authdata ***signed_auth_data)
{
	krb5_const_principal ks_client_princ = NULL;
	krb5_db_entry *client_entry = NULL;
	krb5_authdata **pac_auth_data = NULL;
	krb5_authdata **authdata = NULL;
	krb5_boolean is_as_req;
	krb5_error_code code;
	krb5_pac pac = NULL;
	krb5_data pac_data;
	bool with_pac = false;
	bool generate_pac = false;
	char *client_name = NULL;


	krbtgt = krbtgt == NULL ? local_krbtgt : krbtgt;
	krbtgt_key = krbtgt_key == NULL ? local_krbtgt_key : krbtgt_key;

	/* FIXME: We don't support S4U yet */
	if (flags & KRB5_KDB_FLAGS_S4U) {
		return KRB5_KDB_DBTYPE_NOSUP;
	}

	is_as_req = ((flags & KRB5_KDB_FLAG_CLIENT_REFERRALS_ONLY) != 0);

	/*
	 * When using s4u2proxy client_princ actually refers to the proxied user
	 * while client->princ to the proxy service asking for the TGS on behalf
	 * of the proxied user. So always use client_princ in preference.
	 *
	 * Note that when client principal is not NULL, client entry might be
	 * NULL for cross-realm case, so we need to make sure to not
	 * dereference NULL pointer here.
	 */
	if (client_princ != NULL) {
		ks_client_princ = client_princ;
		if (!is_as_req) {
			krb5_boolean is_equal = false;

			if (client != NULL && client->princ != NULL) {
				is_equal =
					krb5_principal_compare(context,
							       client_princ,
							       client->princ);
			}

			/*
			 * When client principal is the same as supplied client
			 * entry, don't fetch it.
			 */
			if (!is_equal) {
				code = ks_get_principal(context,
							ks_client_princ,
							0,
							&client_entry);
				if (code != 0) {
					(void)krb5_unparse_name(context,
								ks_client_princ,
								&client_name);

					DBG_DEBUG("We didn't find the client "
						  "principal [%s] in our "
						  "database.\n",
						  client_name);
					SAFE_FREE(client_name);

					/*
					 * If we didn't find client_princ in our
					 * database it might be from another
					 * realm.
					 */
					client_entry = NULL;
				}
			}
		}
	} else {
		if (client == NULL) {
			*signed_auth_data = NULL;
			return 0;
		}
		ks_client_princ = client->princ;
	}

	if (client_entry == NULL) {
		client_entry = client;
	}

	if (is_as_req) {
		with_pac = mit_samba_princ_needs_pac(client_entry);
	} else {
		with_pac = mit_samba_princ_needs_pac(server);
	}

	code = krb5_unparse_name(context,
				 client_princ,
				 &client_name);
	if (code != 0) {
		goto done;
	}

	if (is_as_req && (flags & KRB5_KDB_FLAG_INCLUDE_PAC) != 0) {
		generate_pac = true;
	}

	DBG_DEBUG("*** Sign data for client principal: %s [%s %s%s]\n",
		  client_name,
		  is_as_req ? "AS-REQ" : "TGS_REQ",
		  with_pac ? is_as_req ? "WITH_PAC" : "FIND_PAC" : "NO_PAC",
		  generate_pac ? " GENERATE_PAC" : "");

	/*
	 * Generate PAC for the AS-REQ or check or generate one for the TGS if
	 * needed.
	 */
	if (with_pac && generate_pac) {
		DBG_DEBUG("Generate PAC for AS-REQ [%s]\n", client_name);

		code = krb5_pac_init(context, &pac);
		if (code != 0) {
			goto done;
		}

		code = ks_get_pac(context,
				  flags,
				  client_entry,
				  server,
				  NULL,
				  &pac);
		if (code != 0) {
			goto done;
		}
	} else if (with_pac && !is_as_req) {
		/*
		 * Find the PAC in the TGS, if one exists.
		 */
		code = krb5_find_authdata(context,
					  tgt_auth_data,
					  NULL,
					  KRB5_AUTHDATA_WIN2K_PAC,
					  &pac_auth_data);
		if (code != 0) {
			DBG_ERR("krb5_find_authdata failed: %d\n", code);
			goto done;
		}
		DBG_DEBUG("Found PAC data for TGS-REQ [%s]\n", client_name);

		if (pac_auth_data != NULL && pac_auth_data[0] != NULL) {
			if (pac_auth_data[1] != NULL) {
				DBG_ERR("Invalid PAC data!\n");
				code = KRB5KDC_ERR_BADOPTION;
				goto done;
			}

			DBG_DEBUG("Verify PAC for TGS [%s]\n",
				client_name);

			code = ks_verify_pac(context,
					     flags,
					     ks_client_princ,
					     client_entry,
					     server,
					     krbtgt,
					     server_key,
					     krbtgt_key,
					     authtime,
					     tgt_auth_data,
					     &pac);
			if (code != 0) {
				goto done;
			}
		} else {
			if (flags & KRB5_KDB_FLAG_CONSTRAINED_DELEGATION) {
				DBG_DEBUG("Generate PAC for constrained"
					  "delegation TGS [%s]\n",
					  client_name);

				code = krb5_pac_init(context, &pac);
				if (code != 0) {
					goto done;
				}

				code = ks_get_pac(context,
						  flags,
						  client_entry,
						  server,
						  NULL,
						  &pac);
				if (code != 0 && code != ENOENT) {
					goto done;
				}
			}
		}
	}

	if (pac == NULL) {
		DBG_DEBUG("No PAC data - we're done [%s]\n", client_name);
		*signed_auth_data = NULL;
		code = 0;
		goto done;
	}

	DBG_DEBUG("Signing PAC for %s [%s]\n",
		  is_as_req ? "AS-REQ" : "TGS-REQ",
		  client_name);
	code = krb5_pac_sign(context, pac, authtime, ks_client_princ,
			server_key, krbtgt_key, &pac_data);
	if (code != 0) {
		DBG_ERR("krb5_pac_sign failed: %d\n", code);
		goto done;
	}

	authdata = calloc(2, sizeof(krb5_authdata *));
	if (authdata == NULL) {
		goto done;
	}

	authdata[0] = malloc(sizeof(krb5_authdata));
	if (authdata[0] == NULL) {
		goto done;
	}

	/* put in signed data */
	authdata[0]->magic = KV5M_AUTHDATA;
	authdata[0]->ad_type = KRB5_AUTHDATA_WIN2K_PAC;
	authdata[0]->contents = (krb5_octet *)pac_data.data;
	authdata[0]->length = pac_data.length;

	code = krb5_encode_authdata_container(context,
					      KRB5_AUTHDATA_IF_RELEVANT,
					      authdata,
					      signed_auth_data);
	if (code != 0) {
		goto done;
	}

	code = 0;

done:
	if (client_entry != NULL && client_entry != client) {
		ks_free_principal(context, client_entry);
	}
	SAFE_FREE(client_name);
	krb5_free_authdata(context, authdata);
	krb5_pac_free(context, pac);

	return code;
}
#else /* KRB5_KDB_DAL_MAJOR_VERSION >= 9 */
static krb5_error_code ks_update_pac(krb5_context context,
				     int flags,
				     krb5_db_entry *client,
				     krb5_db_entry *server,
				     krb5_db_entry *signing_krbtgt,
				     krb5_pac old_pac,
				     krb5_pac new_pac)
{
	struct mit_samba_context *mit_ctx = NULL;
	krb5_error_code code;

	mit_ctx = ks_get_context(context);
	if (mit_ctx == NULL) {
		return KRB5_KDB_DBNOTINITED;
	}

	code = mit_samba_update_pac(mit_ctx,
				    context,
				    flags,
				    client,
				    server,
				    signing_krbtgt,
				    old_pac,
				    new_pac);
	if (code != 0) {
		return code;
	}

	return code;
}

krb5_error_code kdb_samba_db_issue_pac(krb5_context context,
				       unsigned int flags,
				       krb5_db_entry *client,
				       krb5_keyblock *replaced_reply_key,
				       krb5_db_entry *server,
				       krb5_db_entry *signing_krbtgt,
				       krb5_timestamp authtime,
				       krb5_pac old_pac,
				       krb5_pac new_pac,
				       krb5_data ***auth_indicators)
{
	char *client_name = NULL;
	char *server_name = NULL;
	krb5_error_code code = EINVAL;

	/* The KDC handles both signing and verification for us. */

	if (client != NULL) {
		code = krb5_unparse_name(context,
					 client->princ,
					 &client_name);
		if (code != 0) {
			return code;
		}
	}

	if (server != NULL) {
		code = krb5_unparse_name(context,
					 server->princ,
					 &server_name);
		if (code != 0) {
			SAFE_FREE(client_name);
			return code;
		}
	}

	/*
	 * Get a new PAC for AS-REQ or S4U2Self for our realm.
	 *
	 * For a simple cross-realm S4U2Proxy there will be the following TGS
	 * requests after the client realm is identified:
	 *
	 * 1. server@SREALM to SREALM for krbtgt/CREALM@SREALM -- a regular TGS
	 *    request with server's normal TGT and no S4U2Self padata.
	 * 2. server@SREALM to CREALM for server@SREALM (expressed as an
	 *    enterprise principal), with the TGT from #1 as header ticket and
	 *    S4U2Self padata identifying the client.
	 * 3. server@SREALM to SREALM for server@SREALM with S4U2Self padata,
	 *    with the referral TGT from #2 as header ticket
	 *
	 * In request 2 the PROTOCOL_TRANSITION and CROSS_REALM flags are set,
	 * and the request is for a local client (so client != NULL) and we
	 * want to make a new PAC.
	 *
	 * In request 3 the PROTOCOL_TRANSITION and CROSS_REALM flags are also
	 * set, but the request is for a non-local client (so client == NULL)
	 * and we want to copy the subject PAC contained in the referral TGT.
	 */
	if (old_pac == NULL ||
	    (client != NULL && (flags & KRB5_KDB_FLAG_PROTOCOL_TRANSITION))) {
		DBG_NOTICE("Generate PAC for AS-REQ [client=%s, flags=%#08x]\n",
			   client_name != NULL ? client_name : "<unknown>",
			   flags);

		code = ks_get_pac(context,
				  flags,
				  client,
				  server,
				  replaced_reply_key,
				  &new_pac);
	} else {
		DBG_NOTICE("Update PAC for TGS-REQ [client=%s, server=%s, "
			   "flags=%#08x]\n",
			   client_name != NULL ? client_name : "<unknown>",
			   server_name != NULL ? server_name : "<unknown>",
			   flags);

		code = ks_update_pac(context,
				flags,
				client,
				server,
				signing_krbtgt,
				old_pac,
				new_pac);
	}
	SAFE_FREE(client_name);
	SAFE_FREE(server_name);

	return code;
}
#endif /* KRB5_KDB_DAL_MAJOR_VERSION */

krb5_error_code kdb_samba_db_check_allowed_to_delegate(krb5_context context,
						       krb5_const_principal client,
						       const krb5_db_entry *server,
						       krb5_const_principal proxy)
{
	struct mit_samba_context *mit_ctx = NULL;

	mit_ctx = ks_get_context(context);
	if (mit_ctx == NULL) {
		return KRB5_KDB_DBNOTINITED;
	}

	return mit_samba_check_s4u2proxy(mit_ctx,
					 server,
					 proxy);

}


#if KRB5_KDB_DAL_MAJOR_VERSION >= 9
krb5_error_code kdb_samba_db_allowed_to_delegate_from(
		krb5_context context,
		krb5_const_principal client_principal,
		krb5_const_principal server_principal,
		krb5_pac header_pac,
		const krb5_db_entry *proxy)
{
	struct mit_samba_context *mit_ctx = NULL;
	krb5_error_code code;

	mit_ctx = ks_get_context(context);
	if (mit_ctx == NULL) {
		return KRB5_KDB_DBNOTINITED;
	}

	code = mit_samba_check_allowed_to_delegate_from(mit_ctx,
							client_principal,
							server_principal,
							header_pac,
							proxy);

	return code;
}
#endif


static void samba_bad_password_count(krb5_db_entry *client,
				     krb5_error_code error_code)
{
	switch (error_code) {
	case 0:
		mit_samba_zero_bad_password_count(client);
		break;
	case KRB5KDC_ERR_PREAUTH_FAILED:
	case KRB5KRB_AP_ERR_BAD_INTEGRITY:
		mit_samba_update_bad_password_count(client);
		break;
	}
}

void kdb_samba_db_audit_as_req(krb5_context context,
			       krb5_kdc_req *request,
			       const krb5_address *local_addr,
			       const krb5_address *remote_addr,
			       krb5_db_entry *client,
			       krb5_db_entry *server,
			       krb5_timestamp authtime,
			       krb5_error_code error_code)
{
	/*
	 * FIXME: This segfaulted with a FAST test
	 * FIND_FAST: <unknown client> for <unknown server>, Unknown FAST armor type 0
	 */
	if (client == NULL) {
		return;
	}

	samba_bad_password_count(client, error_code);

	/* TODO: perform proper audit logging for addresses */
}
