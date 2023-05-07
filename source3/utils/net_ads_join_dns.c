/*
   Samba Unix/Linux SMB client library
   net ads dns internal functions
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Remus Koos (remuskoos@yahoo.com)
   Copyright (C) 2002 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2006 Gerald (Jerry) Carter (jerry@samba.org)

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

#include "includes.h"
#include "utils/net.h"
#include "../lib/addns/dnsquery.h"
#include "secrets.h"
#include "krb5_env.h"
#include "utils/net_dns.h"
#include "lib/util/string_wrappers.h"

#ifdef HAVE_ADS

/*******************************************************************
 Send a DNS update request
*******************************************************************/

#if defined(HAVE_KRB5)
#include "../lib/addns/dns.h"

void use_in_memory_ccache(void) {
	/* Use in-memory credentials cache so we do not interfere with
	 * existing credentials */
	setenv(KRB5_ENV_CCNAME, "MEMORY:net_ads", 1);
}

static NTSTATUS net_update_dns_internal(struct net_context *c,
					TALLOC_CTX *ctx, ADS_STRUCT *ads,
					const char *machine_name,
					const struct sockaddr_storage *addrs,
					int num_addrs, bool remove_host)
{
	struct dns_rr_ns *nameservers = NULL;
	size_t ns_count = 0, i;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	DNS_ERROR dns_err;
	fstring dns_server;
	const char *dnsdomain = NULL;
	char *root_domain = NULL;

	if ( (dnsdomain = strchr_m( machine_name, '.')) == NULL ) {
		d_printf(_("No DNS domain configured for %s. "
			   "Unable to perform DNS Update.\n"), machine_name);
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}
	dnsdomain++;

	status = ads_dns_lookup_ns(ctx,
				   dnsdomain,
				   &nameservers,
				   &ns_count);
	if ( !NT_STATUS_IS_OK(status) || (ns_count == 0)) {
		/* Child domains often do not have NS records.  Look
		   for the NS record for the forest root domain
		   (rootDomainNamingContext in therootDSE) */

		const char *rootname_attrs[] = 	{ "rootDomainNamingContext", NULL };
		LDAPMessage *msg = NULL;
		char *root_dn;
		ADS_STATUS ads_status;

		if ( !ads->ldap.ld ) {
			ads_status = ads_connect( ads );
			if ( !ADS_ERR_OK(ads_status) ) {
				DEBUG(0,("net_update_dns_internal: Failed to connect to our DC!\n"));
				goto done;
			}
		}

		ads_status = ads_do_search(ads, "", LDAP_SCOPE_BASE,
				       "(objectclass=*)", rootname_attrs, &msg);
		if (!ADS_ERR_OK(ads_status)) {
			goto done;
		}

		root_dn = ads_pull_string(ads, ctx, msg,  "rootDomainNamingContext");
		if ( !root_dn ) {
			ads_msgfree( ads, msg );
			goto done;
		}

		root_domain = ads_build_domain( root_dn );

		/* cleanup */
		ads_msgfree( ads, msg );

		/* try again for NS servers */

		status = ads_dns_lookup_ns(ctx,
					   root_domain,
					   &nameservers,
					   &ns_count);

		if ( !NT_STATUS_IS_OK(status) || (ns_count == 0)) {
			DEBUG(3,("net_update_dns_internal: Failed to find name server for the %s "
			 "realm\n", ads->config.realm));
			if (ns_count == 0) {
				status = NT_STATUS_UNSUCCESSFUL;
			}
			goto done;
		}

		dnsdomain = root_domain;

	}

	for (i=0; i < ns_count; i++) {

		uint32_t flags = DNS_UPDATE_SIGNED |
				 DNS_UPDATE_UNSIGNED |
				 DNS_UPDATE_UNSIGNED_SUFFICIENT |
				 DNS_UPDATE_PROBE |
				 DNS_UPDATE_PROBE_SUFFICIENT;

		if (c->opt_force) {
			flags &= ~DNS_UPDATE_PROBE_SUFFICIENT;
			flags &= ~DNS_UPDATE_UNSIGNED_SUFFICIENT;
		}

		/*
		 *  Do not return after PROBE completion if this function
		 *  is called for DNS removal.
		 */
		if (remove_host) {
			flags &= ~DNS_UPDATE_PROBE_SUFFICIENT;
		}

		status = NT_STATUS_UNSUCCESSFUL;

		/* Now perform the dns update - we'll try non-secure and if we fail,
		   we'll follow it up with a secure update */

		fstrcpy( dns_server, nameservers[i].hostname );

		dns_err = DoDNSUpdate(dns_server,
				      dnsdomain,
				      machine_name,
		                      addrs,
				      num_addrs,
				      flags,
				      remove_host);
		if (ERR_DNS_IS_OK(dns_err)) {
			status = NT_STATUS_OK;
			goto done;
		}

		if (ERR_DNS_EQUAL(dns_err, ERROR_DNS_INVALID_NAME_SERVER) ||
		    ERR_DNS_EQUAL(dns_err, ERROR_DNS_CONNECTION_FAILED) ||
		    ERR_DNS_EQUAL(dns_err, ERROR_DNS_SOCKET_ERROR)) {
			DEBUG(1,("retrying DNS update with next nameserver after receiving %s\n",
				dns_errstr(dns_err)));
			continue;
		}

		d_printf(_("DNS Update for %s failed: %s\n"),
			machine_name, dns_errstr(dns_err));
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

done:

	SAFE_FREE( root_domain );

	return status;
}

NTSTATUS net_update_dns_ext(struct net_context *c,
			    TALLOC_CTX *mem_ctx, ADS_STRUCT *ads,
			    const char *hostname,
			    struct sockaddr_storage *iplist,
			    int num_addrs, bool remove_host)
{
	struct sockaddr_storage *iplist_alloc = NULL;
	fstring machine_name;
	NTSTATUS status;

	if (hostname) {
		fstrcpy(machine_name, hostname);
	} else {
		name_to_fqdn( machine_name, lp_netbios_name() );
	}
	if (!strlower_m( machine_name )) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * If remove_host is true, then remove all IP addresses associated with
	 * this hostname from the AD server.
	 */
	if (!remove_host && (num_addrs == 0 || iplist == NULL)) {
		/*
		 * Get our ip address
		 * (not the 127.0.0.x address but a real ip address)
		 */
		num_addrs = get_my_ip_address(&iplist_alloc);
		if ( num_addrs <= 0 ) {
			DEBUG(4, ("net_update_dns_ext: Failed to find my "
				  "non-loopback IP addresses!\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
		iplist = iplist_alloc;
	}

	status = net_update_dns_internal(c, mem_ctx, ads, machine_name,
					 iplist, num_addrs, remove_host);

	SAFE_FREE(iplist_alloc);
	return status;
}

static NTSTATUS net_update_dns(struct net_context *c, TALLOC_CTX *mem_ctx, ADS_STRUCT *ads, const char *hostname)
{
	NTSTATUS status;

	status = net_update_dns_ext(c, mem_ctx, ads, hostname, NULL, 0, false);
	return status;
}
#endif

void net_ads_join_dns_updates(struct net_context *c, TALLOC_CTX *ctx, struct libnet_JoinCtx *r)
{
#if defined(HAVE_KRB5)
	ADS_STRUCT *ads_dns = NULL;
	int ret;
	NTSTATUS status;
	char *machine_password = NULL;

	/*
	 * In a clustered environment, don't do dynamic dns updates:
	 * Registering the set of ip addresses that are assigned to
	 * the interfaces of the node that performs the join does usually
	 * not have the desired effect, since the local interfaces do not
	 * carry the complete set of the cluster's public IP addresses.
	 * And it can also contain internal addresses that should not
	 * be visible to the outside at all.
	 * In order to do dns updates in a clustererd setup, use
	 * net ads dns register.
	 */
	if (lp_clustering()) {
		d_fprintf(stderr, _("Not doing automatic DNS update in a "
				    "clustered setup.\n"));
		return;
	}

	if (!r->out.domain_is_ad) {
		return;
	}

	/*
	 * We enter this block with user creds.
	 * kinit with the machine password to do dns update.
	 */

	ads_dns = ads_init(ctx,
			   lp_realm(),
			   NULL,
			   r->in.dc_name,
			   ADS_SASL_PLAIN);
	if (ads_dns == NULL) {
		d_fprintf(stderr, _("DNS update failed: out of memory!\n"));
		goto done;
	}

	use_in_memory_ccache();

	ads_dns->auth.user_name = talloc_asprintf(ads_dns,
						  "%s$",
						  lp_netbios_name());
	if (ads_dns->auth.user_name == NULL) {
		d_fprintf(stderr, _("DNS update failed: out of memory\n"));
		goto done;
	}

	machine_password = secrets_fetch_machine_password(
		r->out.netbios_domain_name, NULL, NULL);
	if (machine_password != NULL) {
		ads_dns->auth.password = talloc_strdup(ads_dns,
						       machine_password);
		SAFE_FREE(machine_password);
		if (ads_dns->auth.password == NULL) {
			d_fprintf(stderr,
				  _("DNS update failed: out of memory\n"));
			goto done;
		}
	}

	ads_dns->auth.realm = talloc_asprintf_strupper_m(ads_dns, "%s", r->out.dns_domain_name);
	if (ads_dns->auth.realm == NULL) {
		d_fprintf(stderr, _("talloc_asprintf_strupper_m %s failed\n"),
				  ads_dns->auth.realm);
		goto done;
	}

	ret = ads_kinit_password(ads_dns);
	if (ret != 0) {
		d_fprintf(stderr,
			  _("DNS update failed: kinit failed: %s\n"),
			  error_message(ret));
		goto done;
	}

	status = net_update_dns(c, ctx, ads_dns, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf( stderr, _("DNS update failed: %s\n"),
			  nt_errstr(status));
	}

done:
	TALLOC_FREE(ads_dns);
#endif

	return;
}

#endif  /* HAVE_ADS */
