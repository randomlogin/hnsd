#include <openssl/ssl.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

#include "ldns/ldns.h"

#include "dnssec.h"
#include "utils.h"

// modulus
static const uint8_t nd[] = ""
  "\x00\xb0\x5f\x7a\xe5\x97\x44\x9d\xc5\xee\x49\xbc"
  "\x31\x84\x3d\x01\x57\x72\xe7\x85\x49\x11\x3f\x0f"
  "\x39\x5a\xcf\x8b\x4e\x89\x2e\x70\xa7\xc1\x69\x1e"
  "\x86\x18\xe5\x76\xa9\xcf\xc2\xc9\xf0\x48\xdd\x90"
  "\x66\x33\x44\xe9\x26\xd8\x73\x48\xa5\x59\x59\xe7"
  "\x5d\x79\x5c\x5c\x8c\xc0\x19\xfa\xa5\x08\xc3\x85"
  "\x3b\x36\x43\xc6\x5c\x02\xe2\x0d\x55\x10\x22\xdc"
  "\x7f\x66\x07\x01\xb6\x25\x76\x86\x61\x5f\x56\x90"
  "\x2b\x42\x87\x4c\x53\x1b\xde\x2b\x33\x0b\xce\x3f"
  "\x15\x6d\x6a\x46\x4e\xed\xa6\xcb\x3c\x82\x4d\x0d"
  "\x4c\x20\xa7\x5d\xa1\xe7\xbc\xe5\x2e\x10\x6d\xf8"
  "\xcc\x6c\xcd\x67\x47\x4d\x0c\x2f\xa6\xc1\xa9\x48"
  "\x4e\xa4\xe2\xb1\x7e\x2f\x95\xe3\x27\x23\x8d\xe6"
  "\x9f\x20\xd5\x78\x5c\x22\x1b\x69\xc0\xfb\x6e\x9d"
  "\x80\x26\xe3\x6b\x9f\x6a\x80\xce\x2c\x4f\xe7\x18"
  "\x1e\xa1\x34\xb5\x59\x00\x14\x59\xb7\xce\x8c\xb4"
  "\x73\x1b\xef\xc3\xaf\xf7\x0c\xc7\x47\x8b\xb9\x87"
  "\xf5\x56\xce\xce\xb0\x56\x62\xb6\xdc\xdd\x06\x83"
  "\x2f\xdf\x10\x52\xd0\xa4\x94\x93\x03\x34\x63\x54"
  "\x93\x4e\xb1\xdf\xf7\x1c\x8e\xa9\xa5\x02\x23\x00"
  "\xfb\x92\xc8\x85\x1e\x26\x54\x3e\x8a\xbb\xed\x2f"
  "\x61\x0e\xf4\x88\x6f";

// public exponent
static const uint8_t ed[] = "\x01\x00\x01";

// private exponent
static const uint8_t dd[] = ""
  "\x04\xe9\x53\xc1\xca\xf9\x95\x2f\x2a\xd8\x90\xce"
  "\x0c\x31\xaa\xb4\xe5\xb5\x3e\xc7\xef\x1c\x03\x6f"
  "\x84\x70\xdd\x1f\x3d\xc0\xb6\x50\x65\x99\x68\xc9"
  "\x31\x2e\x4a\xa4\xa5\xed\x75\xb4\x24\x43\x4f\x3f"
  "\x19\x54\x64\xed\xb8\xff\x54\xd9\x8d\xe8\x6c\x01"
  "\xf5\x5c\x36\x13\x91\x51\xe7\xe1\xea\xa1\x8f\x37"
  "\x3b\xe6\x9d\x42\x78\xae\x14\xd2\xf6\x95\x11\xf0"
  "\xd6\x45\x81\xad\xb1\xd3\x60\x20\x9c\x08\x0c\x11"
  "\xb8\x53\x8e\x33\x8e\x46\x1c\x8b\xda\x5b\x4e\xf4"
  "\x68\xcc\x99\xd2\x9e\xd9\x93\x35\x47\x27\xa0\x24"
  "\x2b\x90\x9d\x91\xe1\xa3\x27\xfa\x99\x85\xc1\xda"
  "\xbe\xdd\x03\x4f\x4e\x9f\x06\x5d\x04\x46\xbf\x3b"
  "\x50\xe1\x06\xe0\x26\x4d\x5a\xde\xf8\xe6\x64\xd7"
  "\x44\x5e\xc7\x8c\x13\x1a\x1c\x73\x7d\x65\x00\x4b"
  "\x17\xd0\x7e\x10\xb9\x72\xbf\x5c\x47\x54\x7e\x2a"
  "\x6e\xa4\x39\xff\x51\x5f\x70\x44\x0c\xd0\x2b\xa5"
  "\x01\xd5\xd9\xdd\x71\xf4\x5c\x9e\x90\xec\xf0\x04"
  "\x7d\x73\xba\x18\x21\x79\x27\x73\xb8\x01\x91\xee"
  "\xb5\xd0\xbc\xb2\x0c\x09\x26\x7f\xd0\xbb\xdd\x4a"
  "\xb5\x09\x89\x8d\x19\x87\x5b\x64\xbe\x17\x54\x5b"
  "\x88\x59\xaa\xcf\x86\xcf\x85\x25\xed\x01\x75\x24"
  "\x67\x12\x65\x79";

// prime1
static const uint8_t pd[] = ""
  "\x00\xe9\x39\x7c\x96\x5b\xb9\x8e\x23\x74\x53\x6f"
  "\x97\x3c\x5b\xdf\x1a\xb5\xcf\xa9\x2b\xe1\x4b\x74"
  "\x47\xbe\x5a\x3d\xe1\xe4\x73\x35\x16\x74\x71\x0b"
  "\x24\x2c\x72\x2d\xde\x32\xc7\xac\x20\x48\xd9\x5c"
  "\xa4\x18\x5d\xf9\x69\x54\xaa\x4d\xbb\xa6\xbf\x87"
  "\x27\xb0\x59\x66\x41\x95\x65\xfc\xe9\x8c\x2c\x8e"
  "\x38\xba\x09\xd6\xcb\x4d\x39\xa3\x7c\x92\x3a\xa1"
  "\x77\x16\xc0\x34\x97\xc4\x90\x0f\x4b\xc8\x26\x4d"
  "\x54\x4d\x3b\xbd\xa1\xb0\x2a\x57\xbd\x8a\x6e\xdf"
  "\x6c\xc7\xa7\xd2\x7a\xe4\x7a\xc5\xc9\x12\x36\x56"
  "\x20\x71\x60\xe5\x8e\xd1\xde\x51\xab";

// prime2
static const uint8_t qd[] = ""
  "\x00\xc1\x98\xba\x6c\x1c\x9d\x26\xf2\x7a\xd4\xf2"
  "\x8d\xad\x74\x32\x10\xfd\x79\x77\x88\xb5\x0a\x71"
  "\xa1\xd8\xe0\x5f\x6c\x68\x90\x48\xc3\x69\xbf\x31"
  "\x5c\x29\xf4\x7b\xf6\xa9\xf6\x2b\xe5\xe1\x57\xa7"
  "\x88\xae\x61\x36\x6c\xcd\xeb\xce\x38\x04\xeb\xaa"
  "\xef\xe9\xfc\xd0\x9f\x92\xc3\x0c\xde\xd9\xb8\x2a"
  "\x0f\x05\xf0\x15\xf6\x11\xf7\xfc\x3a\xfb\x1a\xa0"
  "\xb5\x1a\xf5\xa8\x78\x6f\xdd\x79\x6a\x1b\x3e\xec"
  "\xc1\x1b\x19\xd6\xd4\x1d\x95\x56\x32\xaf\xa8\x6d"
  "\x7c\x0d\x95\x59\x96\x0b\x23\x14\xdf\x4d\x33\xa7"
  "\xf7\x86\x88\x0f\x34\xd2\x39\xe8\x4d";

// exponent1
static const uint8_t dmp1d[] = ""
  "\x00\x93\x05\xdc\x56\x64\xe8\x6a\x84\x4b\x36\xb5"
  "\xe8\x1e\xf2\xc3\x88\x71\x08\xc1\xda\x99\xa2\x19"
  "\x61\x88\xcc\x16\xaa\xaa\x7b\x3e\x02\x33\xd1\x77"
  "\x76\x8c\x56\x46\x38\x06\xc6\xfc\xe9\x35\x43\x61"
  "\x35\x48\xef\x24\xe5\x93\xab\xf0\x68\xd5\x4b\x74"
  "\x06\x3f\x13\x7c\x74\xe7\x9b\x6d\x7e\x45\x11\x69"
  "\x6e\xb5\x48\xb2\x91\x62\xd3\x6d\x0e\x80\x98\x59"
  "\x65\x5b\x80\x3b\x27\x59\x90\x7f\x34\x04\xae\xb0"
  "\x9c\xee\x3b\x34\xe6\x12\xc9\xfe\x99\xcc\x04\xec"
  "\xf0\x04\x44\xf5\x58\xe7\x63\xc5\xff\x65\x6d\xbf"
  "\x89\xa0\x2f\xb7\x46\xfb\x62\x2d\x15";

// exponent2
static const uint8_t dmq1d[] = ""
  "\x00\xa5\x81\x06\xe6\x83\xf3\xc9\xa7\x04\x61\x66"
  "\x56\xbe\x91\x96\x77\xb5\xea\x90\xc9\x1b\x54\xa9"
  "\x5e\x5e\xc5\x3a\x6e\xb8\x59\x99\x0b\x0e\x2d\x38"
  "\x6a\x7d\x27\x98\x8e\x80\x30\x86\xc8\xc8\xc3\xa1"
  "\xe3\x14\x88\xe0\xf9\x55\x75\xa0\xdf\x7d\x3d\x67"
  "\xee\x20\x90\x54\x5e\x07\x1e\x9e\xb3\x29\x57\xeb"
  "\x04\xda\xe3\xac\x32\xa8\x9b\xe3\x53\x1c\xf6\x5f"
  "\xab\x54\x37\xed\x65\xc0\xe4\x8a\xf6\xae\x02\x36"
  "\x6e\xc3\xd7\x6c\x33\xfc\x72\x7d\xb5\x69\x3c\x49"
  "\x15\x03\x9b\xc9\x71\x9f\x65\xc7\x63\x3b\x61\xcc"
  "\xd7\xac\x78\x46\xdf\xb6\x55\x71\x05";

// coefficient
static const uint8_t iqmpd[] = ""
  "\x00\xaa\x05\x6b\x43\x90\x9f\x22\x78\xfd\x31\x94"
  "\xb1\x4d\x45\x56\x8f\x02\xfd\x8a\x6a\xe0\x45\x01"
  "\x17\xef\xbc\xba\x86\x50\xa4\x6e\x55\x07\xde\xe6"
  "\xe9\x93\xdd\x66\x1d\xd5\xf8\xd3\x42\x3a\x5e\x8e"
  "\x67\x22\x9f\x48\x66\x86\xc6\x3c\xb1\x93\xc9\x41"
  "\xe8\x8e\x61\x40\x78\x50\x07\x35\x29\x6d\x76\xe3"
  "\x30\x45\xbe\x35\x77\x28\xd7\x37\x01\xb6\xb3\x9b"
  "\xf9\xb9\x8f\x53\x13\x67\x43\x25\xcd\xb6\xc6\xa9"
  "\xb3\x45\xbf\x67\x73\x67\xbc\xc5\x98\x45\x99\x17"
  "\x30\x78\x75\x8e\x2b\xe8\xac\x8a\xa2\xbf\xea\xab"
  "\x8a\xef\xb5\x9f\x3b\xd2\x47\xca\x45";

ldns_key *
hsk_dnssec_get_key(void) {
  ldns_key *key = NULL;
  RSA *rsa = NULL;
  BIGNUM *n = NULL;
  BIGNUM *e = NULL;
  BIGNUM *d = NULL;
  BIGNUM *p = NULL;
  BIGNUM *q = NULL;
  BIGNUM *dmp1 = NULL;
  BIGNUM *dmq1 = NULL;
  BIGNUM *iqmp = NULL;

  key = ldns_key_new();

  if (!key)
    goto fail;

  ldns_key_set_algorithm(key, LDNS_RSASHA256);

  rsa = RSA_new();

  if (!rsa)
    goto fail;

  n = BN_bin2bn((uint8_t *)nd, sizeof(nd) - 1, NULL);
  e = BN_bin2bn((uint8_t *)ed, sizeof(ed) - 1, NULL);
  d = BN_bin2bn((uint8_t *)dd, sizeof(dd) - 1, NULL);
  p = BN_bin2bn((uint8_t *)pd, sizeof(pd) - 1, NULL);
  q = BN_bin2bn((uint8_t *)qd, sizeof(qd) - 1, NULL);
  dmp1 = BN_bin2bn((uint8_t *)dmp1d, sizeof(dmp1d) - 1, NULL);
  dmq1 = BN_bin2bn((uint8_t *)dmq1d, sizeof(dmq1d) - 1, NULL);
  iqmp = BN_bin2bn((uint8_t *)iqmpd, sizeof(iqmpd) - 1, NULL);

  if (!n || !e || !d || !p || !q || !dmp1 || !dmq1 || !iqmp)
    goto fail;

  if (!RSA_set0_key(rsa, n, e, d))
    goto fail;

  if (!RSA_set0_factors(rsa, p, q))
    goto fail;

  if (!RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp))
    goto fail;

  ldns_key_assign_rsa_key(key, rsa);

  ldns_key_set_inception(key, hsk_now());
  ldns_key_set_expiration(key, hsk_now() + 172800);
  ldns_key_set_pubkey_owner(key, ldns_dname_new_frm_str("."));
  ldns_key_set_flags(key, 257);
  ldns_key_set_use(key, 1);

  ldns_rr *dnskey = ldns_key2rr(key);

  if (!dnskey)
    goto fail;

  ldns_key_set_keytag(key, ldns_calc_keytag(dnskey));
  ldns_rr_free(dnskey);

  return key;

fail:
  if (key)
    ldns_key_deep_free(key);

  if (n)
    BN_free(n);

  if (e)
    BN_free(e);

  if (d)
    BN_free(d);

  if (p)
    BN_free(p);

  if (q)
    BN_free(q);

  if (dmp1)
    BN_free(dmp1);

  if (dmq1)
    BN_free(dmq1);

  if (iqmp)
    BN_free(iqmp);

  return NULL;
}

ldns_key_list *
hsk_dnssec_get_list(void) {
  ldns_key_list *keys = ldns_key_list_new();

  if (!keys)
    return NULL;

  ldns_key *key = hsk_dnssec_get_key();

  if (!key) {
    ldns_key_list_free(keys);
    return NULL;
  }

  ldns_key_list_push_key(keys, key);

  return keys;
}

ldns_rr *
hsk_dnssec_get_dnskey(void) {
  ldns_key *key = hsk_dnssec_get_key();

  if (!key)
    return NULL;

  ldns_rr *dnskey = ldns_key2rr(key);

  ldns_key_deep_free(key);

  return dnskey;
}

ldns_rr *
hsk_dnssec_get_ds(void) {
  ldns_rr *dnskey = hsk_dnssec_get_dnskey();

  if (!dnskey)
    return NULL;

  ldns_rr *ds = ldns_key_rr2ds(dnskey, LDNS_SHA256);

  ldns_rr_free(dnskey);

  return ds;
}

bool
hsk_dnssec_sign(ldns_rr_list *an, ldns_rr_type type) {
  ldns_key_list *keys = hsk_dnssec_get_list();

  if (!keys)
    return false;

  ldns_rr_list *set = ldns_rr_list_new();

  if (!set) {
    ldns_key_list_free(keys);
    return false;
  }

  int32_t i;
  for (i = 0; i < ldns_rr_list_rr_count(an); i++) {
    ldns_rr *rr = ldns_rr_list_rr(an, i);
    if (ldns_rr_get_type(rr) == type)
      ldns_rr_list_push_rr(set, rr);
  }

  if (ldns_rr_list_rr_count(set) == 0) {
    ldns_key_list_free(keys);
    ldns_rr_list_free(set);
    return false;
  }

  ldns_rr_list *rrs = ldns_sign_public(set, keys);

  ldns_key_list_free(keys);
  ldns_rr_list_free(set);

  if (!rrs)
    return false;

  if (!ldns_rr_list_cat(an, rrs)) {
    ldns_rr_list_deep_free(rrs);
    return false;
  }

  ldns_rr_list_free(rrs);

  return true;
}

ldns_rr_list *
hsk_dnssec_filter(ldns_rr_list *list, ldns_rr_type type) {
  ldns_rr_list *set = ldns_rr_list_new();

  if (!set)
    return NULL;

  ldns_rr_list *dead = ldns_rr_list_new();

  if (!dead) {
    ldns_rr_list_free(set);
    return NULL;
  }

  int32_t count = ldns_rr_list_rr_count(list);
  int32_t i;

  for (i = 0; i < count; i++) {
    ldns_rr *rr = ldns_rr_list_rr(list, i);

    if (type != LDNS_RR_TYPE_RRSIG) {
      if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_RRSIG) {
        if (!ldns_rr_list_push_rr(dead, rr)) {
          ldns_rr_list_free(dead);
          ldns_rr_list_free(set);
          return NULL;
        }
        continue;
      }
    }

    if (type != LDNS_RR_TYPE_DNSKEY) {
      if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_DNSKEY) {
        if (!ldns_rr_list_push_rr(dead, rr)) {
          ldns_rr_list_free(dead);
          ldns_rr_list_free(set);
          return NULL;
        }
        continue;
      }
    }

    if (type != LDNS_RR_TYPE_DS) {
      if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_DS) {
        if (!ldns_rr_list_push_rr(dead, rr)) {
          ldns_rr_list_free(dead);
          ldns_rr_list_free(set);
          return NULL;
        }
        continue;
      }
    }

    if (type != LDNS_RR_TYPE_NSEC3) {
      if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_NSEC3) {
        if (!ldns_rr_list_push_rr(dead, rr)) {
          ldns_rr_list_free(dead);
          ldns_rr_list_free(set);
          return NULL;
        }
        continue;
      }
    }

    if (type != LDNS_RR_TYPE_NSEC3PARAM) {
      if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_NSEC3PARAM) {
        if (!ldns_rr_list_push_rr(dead, rr)) {
          ldns_rr_list_free(dead);
          ldns_rr_list_free(set);
          return NULL;
        }
        continue;
      }
    }

    if (type != LDNS_RR_TYPE_NSEC) {
      if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_NSEC) {
        if (!ldns_rr_list_push_rr(dead, rr)) {
          ldns_rr_list_free(dead);
          ldns_rr_list_free(set);
          return NULL;
        }
        continue;
      }
    }

    if (!ldns_rr_list_push_rr(set, rr)) {
      ldns_rr_list_free(dead);
      ldns_rr_list_free(set);
      return NULL;
    }
  }

  ldns_rr_list_deep_free(dead);
  ldns_rr_list_free(list);

  return set;
}

bool
hsk_dnssec_clean(ldns_pkt *pkt, ldns_rr_type type) {
  ldns_rr_list *an = hsk_dnssec_filter(ldns_pkt_answer(pkt), type);

  if (!an)
    return false;

  ldns_pkt_set_ancount(pkt, ldns_rr_list_rr_count(an));
  ldns_pkt_set_answer(pkt, an);

  ldns_rr_list *ns = hsk_dnssec_filter(ldns_pkt_authority(pkt), type);

  if (!ns)
    return false;

  ldns_pkt_set_nscount(pkt, ldns_rr_list_rr_count(ns));
  ldns_pkt_set_authority(pkt, ns);

  ldns_rr_list *ar = hsk_dnssec_filter(ldns_pkt_additional(pkt), type);

  if (!ar)
    return false;

  ldns_pkt_set_arcount(pkt, ldns_rr_list_rr_count(ar));
  ldns_pkt_set_additional(pkt, ar);

  return true;
}
