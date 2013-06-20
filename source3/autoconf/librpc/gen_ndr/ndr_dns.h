/* header auto-generated by pidl */

#include "librpc/ndr/libndr.h"
#include "autoconf/librpc/gen_ndr/dns.h"

#ifndef _HEADER_NDR_dns
#define _HEADER_NDR_dns

#include "librpc/ndr/ndr_dns.h"
#define NDR_DNS_UUID "a047c001-5f22-40b0-9d52-7042c43f711a"
#define NDR_DNS_VERSION 0.0
#define NDR_DNS_NAME "dns"
#define NDR_DNS_HELPSTRING "DNS records"
extern const struct ndr_interface_table ndr_table_dns;
#define NDR_DECODE_DNS_NAME_PACKET (0x00)

#define NDR_DNS_CALL_COUNT (1)
enum ndr_err_code ndr_push_dns_operation(struct ndr_push *ndr, int ndr_flags, uint16_t r);
enum ndr_err_code ndr_pull_dns_operation(struct ndr_pull *ndr, int ndr_flags, uint16_t *r);
void ndr_print_dns_operation(struct ndr_print *ndr, const char *name, uint16_t r);
enum ndr_err_code ndr_push_dns_opcode(struct ndr_push *ndr, int ndr_flags, enum dns_opcode r);
enum ndr_err_code ndr_pull_dns_opcode(struct ndr_pull *ndr, int ndr_flags, enum dns_opcode *r);
void ndr_print_dns_opcode(struct ndr_print *ndr, const char *name, enum dns_opcode r);
enum ndr_err_code ndr_push_dns_rcode(struct ndr_push *ndr, int ndr_flags, enum dns_rcode r);
enum ndr_err_code ndr_pull_dns_rcode(struct ndr_pull *ndr, int ndr_flags, enum dns_rcode *r);
void ndr_print_dns_rcode(struct ndr_print *ndr, const char *name, enum dns_rcode r);
enum ndr_err_code ndr_push_dns_qclass(struct ndr_push *ndr, int ndr_flags, enum dns_qclass r);
enum ndr_err_code ndr_pull_dns_qclass(struct ndr_pull *ndr, int ndr_flags, enum dns_qclass *r);
void ndr_print_dns_qclass(struct ndr_print *ndr, const char *name, enum dns_qclass r);
enum ndr_err_code ndr_push_dns_qtype(struct ndr_push *ndr, int ndr_flags, enum dns_qtype r);
enum ndr_err_code ndr_pull_dns_qtype(struct ndr_pull *ndr, int ndr_flags, enum dns_qtype *r);
void ndr_print_dns_qtype(struct ndr_print *ndr, const char *name, enum dns_qtype r);
enum ndr_err_code ndr_push_dns_tkey_mode(struct ndr_push *ndr, int ndr_flags, enum dns_tkey_mode r);
enum ndr_err_code ndr_pull_dns_tkey_mode(struct ndr_pull *ndr, int ndr_flags, enum dns_tkey_mode *r);
void ndr_print_dns_tkey_mode(struct ndr_print *ndr, const char *name, enum dns_tkey_mode r);
enum ndr_err_code ndr_push_dns_name_question(struct ndr_push *ndr, int ndr_flags, const struct dns_name_question *r);
enum ndr_err_code ndr_pull_dns_name_question(struct ndr_pull *ndr, int ndr_flags, struct dns_name_question *r);
void ndr_print_dns_name_question(struct ndr_print *ndr, const char *name, const struct dns_name_question *r);
enum ndr_err_code ndr_push_dns_rdata_data(struct ndr_push *ndr, int ndr_flags, const struct dns_rdata_data *r);
enum ndr_err_code ndr_pull_dns_rdata_data(struct ndr_pull *ndr, int ndr_flags, struct dns_rdata_data *r);
void ndr_print_dns_rdata_data(struct ndr_print *ndr, const char *name, const struct dns_rdata_data *r);
void ndr_print_dns_soa_record(struct ndr_print *ndr, const char *name, const struct dns_soa_record *r);
enum ndr_err_code ndr_push_dns_mx_record(struct ndr_push *ndr, int ndr_flags, const struct dns_mx_record *r);
enum ndr_err_code ndr_pull_dns_mx_record(struct ndr_pull *ndr, int ndr_flags, struct dns_mx_record *r);
void ndr_print_dns_mx_record(struct ndr_print *ndr, const char *name, const struct dns_mx_record *r);
enum ndr_err_code ndr_push_dns_txt_record(struct ndr_push *ndr, int ndr_flags, const struct dns_txt_record *r);
enum ndr_err_code ndr_pull_dns_txt_record(struct ndr_pull *ndr, int ndr_flags, struct dns_txt_record *r);
void ndr_print_dns_txt_record(struct ndr_print *ndr, const char *name, const struct dns_txt_record *r);
enum ndr_err_code ndr_push_dns_srv_record(struct ndr_push *ndr, int ndr_flags, const struct dns_srv_record *r);
enum ndr_err_code ndr_pull_dns_srv_record(struct ndr_pull *ndr, int ndr_flags, struct dns_srv_record *r);
void ndr_print_dns_srv_record(struct ndr_print *ndr, const char *name, const struct dns_srv_record *r);
enum ndr_err_code ndr_push_dns_tkey_record(struct ndr_push *ndr, int ndr_flags, const struct dns_tkey_record *r);
enum ndr_err_code ndr_pull_dns_tkey_record(struct ndr_pull *ndr, int ndr_flags, struct dns_tkey_record *r);
void ndr_print_dns_tkey_record(struct ndr_print *ndr, const char *name, const struct dns_tkey_record *r);
enum ndr_err_code ndr_push_dns_tsig_record(struct ndr_push *ndr, int ndr_flags, const struct dns_tsig_record *r);
enum ndr_err_code ndr_pull_dns_tsig_record(struct ndr_pull *ndr, int ndr_flags, struct dns_tsig_record *r);
void ndr_print_dns_tsig_record(struct ndr_print *ndr, const char *name, const struct dns_tsig_record *r);
enum ndr_err_code ndr_push_dns_fake_tsig_rec(struct ndr_push *ndr, int ndr_flags, const struct dns_fake_tsig_rec *r);
enum ndr_err_code ndr_pull_dns_fake_tsig_rec(struct ndr_pull *ndr, int ndr_flags, struct dns_fake_tsig_rec *r);
void ndr_print_dns_fake_tsig_rec(struct ndr_print *ndr, const char *name, const struct dns_fake_tsig_rec *r);
enum ndr_err_code ndr_push_dns_rdata(struct ndr_push *ndr, int ndr_flags, const union dns_rdata *r);
enum ndr_err_code ndr_pull_dns_rdata(struct ndr_pull *ndr, int ndr_flags, union dns_rdata *r);
void ndr_print_dns_rdata(struct ndr_print *ndr, const char *name, const union dns_rdata *r);
enum ndr_err_code ndr_push_dns_res_rec(struct ndr_push *ndr, int ndr_flags, const struct dns_res_rec *r);
enum ndr_err_code ndr_pull_dns_res_rec(struct ndr_pull *ndr, int ndr_flags, struct dns_res_rec *r);
void ndr_print_dns_res_rec(struct ndr_print *ndr, const char *name, const struct dns_res_rec *r);
enum ndr_err_code ndr_push_dns_name_packet(struct ndr_push *ndr, int ndr_flags, const struct dns_name_packet *r);
enum ndr_err_code ndr_pull_dns_name_packet(struct ndr_pull *ndr, int ndr_flags, struct dns_name_packet *r);
void ndr_print_dns_name_packet(struct ndr_print *ndr, const char *name, const struct dns_name_packet *r);
void ndr_print_decode_dns_name_packet(struct ndr_print *ndr, const char *name, int flags, const struct decode_dns_name_packet *r);
#endif /* _HEADER_NDR_dns */
