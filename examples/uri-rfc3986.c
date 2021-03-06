/**
 * @file   uri-rfc3986.c
 * @author Adam Risi <ajrisi@gmail.com>
 * @date   Mon Aug 30 21:54:17 2010
 * 
 * @brief  An example of how to do URI (RFC 3986 style) parsing
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fsm.h>

#define MAX_INPUT 2048

typedef struct uri_s uri;
struct uri_s {
  char *scheme;
  char *userinfo;
  char *host;
  int host_is_ip;
  int port;
  char *path;
  char *query;
  char *fragment;
};

/* Private functions */
static void set_scheme(char **uri_string, int scheme_len, void *global_context, void *local_context);
static void set_userinfo(char **uri_string, int userinfo_len, void *global_context, void *local_context);
static void set_host(char **uri_string, int host_len, void *global_context, void *local_context);
static void check_host_is_ip(char **uri_string, int ip_length, void *global_context, void *local_context);
static void set_host_is_ip(char **uri_string, int not_used, void *global_context, void *local_context);
static void set_port(char **uri_string, int port_len, void *global_context, void *local_context);
static void set_path(char **uri_string, int path_len, void *global_context, void *local_context);
static void set_query(char **uri_string, int query_len, void *global_context, void *local_context);
static void set_fragment(char **uri_string, int fragment_len, void *global_context, void *local_context);

/*
   URI           = scheme ":" hier-part [ "?" query ] [ "#" fragment ]

   hier-part     = "//" authority path-abempty
                 / path-absolute
                 / path-rootless
                 / path-empty

   URI-reference = URI / relative-ref

   absolute-URI  = scheme ":" hier-part [ "?" query ]

   relative-ref  = relative-part [ "?" query ] [ "#" fragment ]

   relative-part = "//" authority path-abempty
                 / path-absolute
                 / path-noscheme
                 / path-empty

   scheme        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )

   authority     = [ userinfo "@" ] host [ ":" port ]
   userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
   host          = IP-literal / IPv4address / reg-name
   port          = *DIGIT

   IP-literal    = "[" ( IPv6address / IPvFuture  ) "]"

   IPvFuture     = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )

   IPv6address   =                            6( h16 ":" ) ls32
                 /                       "::" 5( h16 ":" ) ls32
                 / [               h16 ] "::" 4( h16 ":" ) ls32
                 / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
                 / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
                 / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
                 / [ *4( h16 ":" ) h16 ] "::"              ls32
                 / [ *5( h16 ":" ) h16 ] "::"              h16
                 / [ *6( h16 ":" ) h16 ] "::"

   h16           = 1*4HEXDIG
   ls32          = ( h16 ":" h16 ) / IPv4address
   IPv4address   = dec-octet "." dec-octet "." dec-octet "." dec-octet

   dec-octet     = DIGIT                 ; 0-9
                 / %x31-39 DIGIT         ; 10-99
                 / "1" 2DIGIT            ; 100-199
                 / "2" %x30-34 DIGIT     ; 200-249
                 / "25" %x30-35          ; 250-255

   reg-name      = *( unreserved / pct-encoded / sub-delims )

   path          = path-abempty    ; begins with "/" or is empty
                 / path-absolute   ; begins with "/" but not "//"
                 / path-noscheme   ; begins with a non-colon segment
                 / path-rootless   ; begins with a segment
                 / path-empty      ; zero characters

   path-abempty  = *( "/" segment )
   path-absolute = "/" [ segment-nz *( "/" segment ) ]
   path-noscheme = segment-nz-nc *( "/" segment )
   path-rootless = segment-nz *( "/" segment )
   path-empty    = 0<pchar>

   segment       = *pchar
   segment-nz    = 1*pchar
   segment-nz-nc = 1*( unreserved / pct-encoded / sub-delims / "@" )
                 ; non-zero-length segment without any colon ":"

   pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"

   query         = *( pchar / "/" / "?" )

   fragment      = *( pchar / "/" / "?" )

   pct-encoded   = "%" HEXDIG HEXDIG

   unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"

   reserved      = gen-delims / sub-delims

   gen-delims    = ":" / "/" / "?" / "#" / "[" / "]" / "@"

   sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
                 / "*" / "+" / "," / ";" / "="
*/

/* some RFC 2234 definitions that RFC3986 requires */

transition alpha_fsm[] = 
  {
    { 0, SINGLE_CHARACTER("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"), -1, -1, ACCEPT, NULL, NULL, "single character-alpha" },
    {-1}
  };

transition digit_fsm[] = 
  {
    { 0, SINGLE_CHARACTER("0123456789"), -1, -1, ACCEPT, NULL, NULL, "single character-digit" },
    {-1}
  };

transition hexdig_fsm[] = 
  {
    {0, FSM(digit_fsm),                   -1, -1, ACCEPT },
    {0, SINGLE_CHARACTER("ABCDEFabcdef"), -1, -1, ACCEPT },
    {-1}
  };

/* the RFC3986 specific def's */

transition sub_delims_fsm[] = 
  {
    { 0, SINGLE_CHARACTER("!$&\'()*+,;="), -1, -1, ACCEPT, NULL, NULL, "single character-sub delims"},
    {-1}
  };

transition gen_delims_fsm[] = 
  {
    { 0, SINGLE_CHARACTER(":/?#[]@"), -1, -1, ACCEPT, NULL, NULL, "single character-gen delims" },
    {-1}
  };

transition reserved_fsm[] = 
  {
    {0, FSM(gen_delims_fsm),  -1, -1, ACCEPT },
    {0, FSM(sub_delims_fsm), -1, -1, ACCEPT },
    {-1}
  };

transition unreserved_fsm[] =
  {
    {0, FSM(alpha_fsm),           -1, -1, ACCEPT, NULL, NULL, "alpha" },
    {0, FSM(digit_fsm),           -1, -1, ACCEPT, NULL, NULL, "digit" },
    {0, SINGLE_CHARACTER("-._~"), -1, -1, ACCEPT, NULL, NULL, "single character" },
    {-1}
  };

transition pct_encoded_fsm[] =
  {
    {0, EXACT_STRING("%"), 1, -1 },
    {1, FSM(hexdig_fsm), 2, -1 },
    {2, FSM(hexdig_fsm), -1, -1, ACCEPT },
    {-1}
  };

transition pchar_fsm[] =
  {
    {0, FSM(unreserved_fsm), -1, -1, ACCEPT},
    {0, FSM(pct_encoded_fsm), -1, -1, ACCEPT},
    {0, FSM(sub_delims_fsm), -1, -1, ACCEPT},
    {0, SINGLE_CHARACTER(":@"), -1, -1, ACCEPT },
    {-1}
  };

transition fragment_fsm[] =
  {
    {0, FSM(pchar_fsm), 0, -1, ACCEPT},
    {0, SINGLE_CHARACTER("/?"), 0, -1, ACCEPT },
    {0, NOTHING, -1, -1, ACCEPT },
    {-1}
  };

transition query_fsm[] =
  {
    {0, FSM(pchar_fsm), 0, -1, ACCEPT},
    {0, SINGLE_CHARACTER("/?"), 0, -1, ACCEPT },
    {0, NOTHING, -1, -1, ACCEPT },
    {-1}
  };

transition segment_nz_nc_fsm[] =
  {
    {0, FSM(unreserved_fsm),   1, -1},
    {0, FSM(pct_encoded_fsm),  1, -1},
    {0, FSM(sub_delims_fsm),   1, -1},
    {0, SINGLE_CHARACTER("@"), 1, -1},

    {1, FSM(unreserved_fsm),   1, -1, ACCEPT},
    {1, FSM(pct_encoded_fsm),  1, -1, ACCEPT},
    {1, FSM(sub_delims_fsm),   1, -1, ACCEPT},
    {1, SINGLE_CHARACTER("@"), 1, -1, ACCEPT},
    {-1}
  };

transition segment_nz_fsm[] =
  {
    {0, FSM(pchar_fsm), 1, -1},
    {1, FSM(pchar_fsm), 1, -1, ACCEPT},
    {-1}
  };

transition segment_fsm[] =
  {
    {0, FSM(pchar_fsm), 0, -1, ACCEPT, NULL, NULL, "pchar"},
    {0, NOTHING,       -1, -1, ACCEPT, NULL, NULL, "matching nothing-pchar"},
    {-1}
  };

transition path_empty_fsm[] =
  {
    {0, NOTHING, -1, -1, ACCEPT},
    {-1}
  };

transition path_rootless_fsm[] =
  {
    {0, FSM(segment_nz_fsm), 1, -1, ACCEPT},
    {1, EXACT_STRING("/"), 2, -1},
    {2, FSM(segment_fsm), 1, -1, ACCEPT},
    {-1}
  };

transition path_noscheme_fsm[] =
  {
    {0, FSM(segment_nz_nc_fsm), 1, -1, ACCEPT},
    {1, EXACT_STRING("/"), 2, -1},
    {2, FSM(segment_fsm), 1, -1, ACCEPT},
    {-1}
  };

transition path_absolute_fsm[] =
  {
    {0, EXACT_STRING("/"), 1, -1, ACCEPT},
    {1, FSM(segment_nz_fsm), 2, -1, ACCEPT},
    {2, EXACT_STRING("/"), 3, -1},
    {3, FSM(segment_fsm), 2, -1, ACCEPT},
    {-1}
  };

transition path_abempty_fsm[] =
  {
    {0, EXACT_STRING("/"), 1, -1, NORMAL, NULL, NULL, "matching a /"},
    {0, NOTHING,          -1, -1, ACCEPT, NULL, NULL, "null transition"},
    {1, FSM(segment_fsm),  0, -1, ACCEPT, NULL, NULL, "segment"},
    {-1}
  };

transition path_fsm[] =
  {
    {0, FSM(path_abempty_fsm),  -1, -1, ACCEPT, set_path, NULL},
    {0, FSM(path_absolute_fsm), -1, -1, ACCEPT, set_path, NULL},
    {0, FSM(path_noscheme_fsm), -1, -1, ACCEPT, set_path, NULL},
    {0, FSM(path_rootless_fsm), -1, -1, ACCEPT, set_path, NULL},
    {0, FSM(path_empty_fsm),    -1, -1, ACCEPT, set_path, NULL},
    {-1}
  };

transition reg_name_fsm[] =
  {
    {0, FSM(unreserved_fsm),  0, -1, ACCEPT, NULL, NULL, "unreserved"},
    {0, FSM(pct_encoded_fsm), 0, -1, ACCEPT, NULL, NULL, "pct encoded"},
    {0, FSM(sub_delims_fsm),  0, -1, ACCEPT, NULL, NULL, "sub delims"},
    {0, NOTHING,             -1, -1, ACCEPT, NULL, NULL, "null transition"},
    {-1}
  };

transition dec_octet_fsm_1[] =
  {
    {0, SINGLE_CHARACTER("123456789"), 1, -1},
    {1, FSM(digit_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition dec_octet_fsm_2[] =
  {
    {0, EXACT_STRING("1"), 1, -1},
    {1, FSM(digit_fsm), 2, -1},
    {2, FSM(digit_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition dec_octet_fsm_3[] =
  {
    {0, EXACT_STRING("2"), 1, -1},
    {1, SINGLE_CHARACTER("01234"), 2, -1},
    {2, FSM(digit_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition dec_octet_fsm_4[] =
  {
    {0, EXACT_STRING("25"), 1, -1},
    {1, SINGLE_CHARACTER("012345"), -1, -1, ACCEPT},
    {-1}
  };

transition dec_octet_fsm[] =
  {
    /* these are ordered from most to least character greedy */

    {0, FSM(dec_octet_fsm_3), -1, -1, ACCEPT}, /* 250 - 255 */
    {0, FSM(dec_octet_fsm_3), -1, -1, ACCEPT}, /* 200 - 249 */
    {0, FSM(dec_octet_fsm_2), -1, -1, ACCEPT}, /* 100 - 199 */
    {0, FSM(dec_octet_fsm_1), -1, -1, ACCEPT}, /* 10 - 99 */
    {0, FSM(digit_fsm),       -1, -1, ACCEPT}, /* 0 - 9 */
    {-1}
  };

transition ipv4address_fsm[] =
  {
    {0, FSM(dec_octet_fsm), 1, -1 },
    {1, EXACT_STRING("."), 2, -1 },

    {2, FSM(dec_octet_fsm), 3, -1 },
    {3, EXACT_STRING("."), 4, -1 },

    {4, FSM(dec_octet_fsm), 5, -1 },
    {5, EXACT_STRING("."), 6, -1 },

    {6, FSM(dec_octet_fsm), -1, -1, ACCEPT },

    {-1}
  };

transition h16_fsm[] =
  {
    {0, FSM(hexdig_fsm), 1, -1, ACCEPT},
    {1, FSM(hexdig_fsm), 2, -1, ACCEPT},
    {2, FSM(hexdig_fsm), 3, -1, ACCEPT},
    {3, FSM(hexdig_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition ls32_fsm[] =
  {
    {0, FSM(ipv4address_fsm), -1, -1, ACCEPT},
    {0, FSM(h16_fsm),          1, -1},
    {1, EXACT_STRING(":"),     2, -1},
    {2, FSM(h16_fsm),         -1, -1, ACCEPT},
    {-1}
  };

/* h16 ":" */
transition ipv6address_fsm_a[] =
  {
    {0, FSM(h16_fsm),       1, -1},
    {1, EXACT_STRING(":"), -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_1[] =
  {
    {0, FSM(ipv6address_fsm_a), 1, -1},
    {1, FSM(ipv6address_fsm_a), 2, -1},
    {2, FSM(ipv6address_fsm_a), 3, -1},
    {3, FSM(ipv6address_fsm_a), 4, -1},
    {4, FSM(ipv6address_fsm_a), 5, -1},
    {5, FSM(ipv6address_fsm_a), 6, -1},
    {6, FSM(ls32_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_2[] =
  {
    {0, EXACT_STRING("::"), 1, -1},
    {1, FSM(ipv6address_fsm_a), 2, -1},
    {2, FSM(ipv6address_fsm_a), 3, -1},
    {3, FSM(ipv6address_fsm_a), 4, -1},
    {4, FSM(ipv6address_fsm_a), 5, -1},
    {5, FSM(ipv6address_fsm_a), 6, -1},
    {6, FSM(ls32_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_3[] =
  {
    {0, FSM(h16_fsm), 1, -1},
    {0, NOTHING, 1, -1},

    {1, EXACT_STRING("::"), 2, -1},

    {2, FSM(ipv6address_fsm_a), 3, -1},
    {3, FSM(ipv6address_fsm_a), 4, -1},
    {4, FSM(ipv6address_fsm_a), 5, -1},
    {5, FSM(ipv6address_fsm_a), 6, -1},
    {6, FSM(ls32_fsm), -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_4_1[] = 
    {
      {0, FSM(ipv6address_fsm_a), 1,  1},
      {1, FSM(h16_fsm), -1, -1, ACCEPT},
      {-1}
    };

transition ipv6address_fsm_4[] =
  {
    
    {0, FSM(ipv6address_fsm_4_1), 1,  1},

    {1, EXACT_STRING("::"),       2, -1},

    {2, FSM(ipv6address_fsm_a),   3, -1},
    {3, FSM(ipv6address_fsm_a),   4, -1},
    {4, FSM(ipv6address_fsm_a),   5, -1},
    {5, FSM(ls32_fsm),           -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_5_1[] =
  {
    {0, FSM(ipv6address_fsm_a), 1,  2},
    {1, FSM(ipv6address_fsm_a), 2,  2},
    {2, FSM(h16_fsm),          -1, -1, ACCEPT },
    {-1}
  };

transition ipv6address_fsm_5[] =
  {
    {0, FSM(ipv6address_fsm_5_1), 1,  1},

    {1, EXACT_STRING("::"),       2, -1},

    {2, FSM(ipv6address_fsm_a),   3, -1},
    {3, FSM(ipv6address_fsm_a),   4, -1},
    {4, FSM(ls32_fsm),           -1, -1, ACCEPT},
    {-1}
  };


transition ipv6address_fsm_6_1[] =
  {
    {0, FSM(ipv6address_fsm_a), 1,  3},
    {1, FSM(ipv6address_fsm_a), 2,  3},
    {2, FSM(ipv6address_fsm_a), 3,  3},
    {3, FSM(h16_fsm),          -1, -1, ACCEPT },
    {-1}
  };

transition ipv6address_fsm_6[] =
  {
    {0, FSM(ipv6address_fsm_6_1), 1,  1},

    {1, EXACT_STRING("::"),       2, -1},

    {2, FSM(ipv6address_fsm_a),   3, -1},
    {3, FSM(ls32_fsm),           -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_7_1[] =
  {
    {0, FSM(ipv6address_fsm_a), 1,  4},
    {1, FSM(ipv6address_fsm_a), 2,  4},
    {2, FSM(ipv6address_fsm_a), 3,  4},
    {2, FSM(ipv6address_fsm_a), 4,  4},
    {4, FSM(h16_fsm),          -1, -1, ACCEPT },
    {-1}
  };

transition ipv6address_fsm_7[] =
  {
    {0, FSM(ipv6address_fsm_7_1), 1,  1},
    {1, EXACT_STRING("::"),       2, -1},
    {2, FSM(ls32_fsm),           -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_8_1[] =
  {
    {0, FSM(ipv6address_fsm_a), 1,  5},
    {1, FSM(ipv6address_fsm_a), 2,  5},
    {2, FSM(ipv6address_fsm_a), 3,  5},
    {2, FSM(ipv6address_fsm_a), 4,  5},
    {2, FSM(ipv6address_fsm_a), 5,  5},
    {5, FSM(h16_fsm),          -1, -1, ACCEPT },
    {-1}
  };

transition ipv6address_fsm_8[] =
  {
    {0, FSM(ipv6address_fsm_8_1), 1,  1},
    {1, EXACT_STRING("::"),       2, -1},
    {2, FSM(h16_fsm),           -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm_9_1[] =
  {
    {0, FSM(ipv6address_fsm_a), 1,  6},
    {1, FSM(ipv6address_fsm_a), 2,  6},
    {2, FSM(ipv6address_fsm_a), 3,  6},
    {2, FSM(ipv6address_fsm_a), 4,  6},
    {2, FSM(ipv6address_fsm_a), 5,  6},
    {2, FSM(ipv6address_fsm_a), 6,  6},
    {6, FSM(h16_fsm),          -1, -1, ACCEPT },
    {-1}
  };

transition ipv6address_fsm_9[] =
  {
    {0, FSM(ipv6address_fsm_9_1),  1,  1},
    {1, EXACT_STRING("::"),       -1, -1, ACCEPT},
    {-1}
  };

transition ipv6address_fsm[] = 
  {
    {0, FSM(ipv6address_fsm_1), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_2), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_3), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_4), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_5), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_6), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_7), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_8), -1, -1, ACCEPT},
    {0, FSM(ipv6address_fsm_9), -1, -1, ACCEPT},
    {-1}
  };

transition ipvfuture_fsm[] =
    {
      {0, EXACT_STRING("v"),   1, -1},
      {1, FSM(hexdig_fsm),     2, -1},
      {2, FSM(hexdig_fsm),     2, -1},
      {2, EXACT_STRING("."),   3, -1},

      {3, FSM(unreserved_fsm), 4, -1, ACCEPT},
      {3, FSM(sub_delims_fsm), 4, -1, ACCEPT},
      {3, EXACT_STRING(":"),   4, -1, ACCEPT},

      {4, FSM(unreserved_fsm), 4, -1, ACCEPT},
      {4, FSM(sub_delims_fsm), 4, -1, ACCEPT},
      {4, EXACT_STRING(":"),   4, -1, ACCEPT},
      {-1}
    };

transition ip_literal_fsm[] =
    {
      {0, EXACT_STRING("["),    1, -1},
      {1, FSM(ipv6address_fsm), 2, -1},
      {1, FSM(ipvfuture_fsm),   2, -1},
      {2, EXACT_STRING("]"),   -1, -1, ACCEPT},
      {-1}
    };

transition port_fsm[] =
  {
    {0, FSM(digit_fsm), 0, -1, ACCEPT},
    {0, NOTHING,       -1, -1, ACCEPT},
    {-1}
  };

transition host_fsm[] =
    {
      {0, FSM(ip_literal_fsm),  -1, -1, ACCEPT, set_host_is_ip, NULL, "ip literal"},
      
      /* this poses a parsing problem - all IPv4 addresses are valid
	 reg_name as well. Fix this by doing the reg_name_fsm now,
	 then on match, do a function to check if its an IPv4
	 address */
      {0, FSM(reg_name_fsm),    -1, -1, ACCEPT, check_host_is_ip, NULL, "reg name"},      
      {-1}
    };

transition userinfo_fsm[] =
    {
      {0, FSM(unreserved_fsm),  0, -1, ACCEPT},
      {0, FSM(pct_encoded_fsm), 0, -1, ACCEPT},
      {0, FSM(sub_delims_fsm),  0, -1, ACCEPT},
      {0, EXACT_STRING(":"),    0, -1, ACCEPT, NULL, NULL, "matching : in userinfo"},
      {0, NOTHING,             -1, -1, ACCEPT},
      {-1}
    };

transition authority_fsm_1[] =
    {
      {0, FSM(userinfo_fsm),  1, -1, NORMAL, set_userinfo, NULL, "userinfo"},
      {1, EXACT_STRING("@"), -1, -1, ACCEPT, NULL, NULL, "matching a @"},
      {-1}
    };
    
transition authority_fsm_2[] =
    {
      {0, EXACT_STRING(":"), 1, -1, NORMAL, NULL, NULL, "matching a :"},
      {1, FSM(port_fsm),    -1, -1, ACCEPT, set_port, NULL, "port"},
      {-1}
    };

transition authority_fsm[] =
    {
      {0, FSM(authority_fsm_1),  1, 1},
      {1, FSM(host_fsm),         2, -1, ACCEPT, set_host, NULL, "host"},
      {2, FSM(authority_fsm_2), -1, -1, ACCEPT},
      {-1}
    };

transition scheme_fsm[] =
    {
      {0, FSM(alpha_fsm),    1, -1, ACCEPT},
      {1, FSM(alpha_fsm),    1, -1, ACCEPT},
      {1, FSM(digit_fsm),    1, -1, ACCEPT},
      {1, EXACT_STRING("+"), 1, -1, ACCEPT},
      {1, EXACT_STRING("-"), 1, -1, ACCEPT},
      {1, EXACT_STRING("."), 1, -1, ACCEPT},
      {-1}
    };

transition relative_part_fsm[] =
    {
      {0, EXACT_STRING("//"),      1, -1},
      {0, FSM(path_absolute_fsm), -1, -1, ACCEPT, set_path, NULL},
      {0, FSM(path_noscheme_fsm), -1, -1, ACCEPT, set_path, NULL},
      {0, FSM(path_empty_fsm),    -1, -1, ACCEPT, set_path, NULL},
   
      {1, FSM(authority_fsm),      2, -1},
      {2, FSM(path_abempty_fsm),  -1, -1, ACCEPT, set_path, NULL},
      {-1}
    };

transition relative_ref_fsm_1[] =
    {
      {0, EXACT_STRING("?"), 1, -1, NORMAL, NULL, NULL, "matching ?-query"},
      {1, FSM(query_fsm),   -1, -1, ACCEPT, set_query, NULL, "query"},
      {-1}
    };

transition relative_ref_fsm_2[] =
    {
      {0, EXACT_STRING("#"),    1, -1, NORMAL, NULL, NULL, "matching #-fragment"},
      {1, FSM(fragment_fsm),   -1, -1, ACCEPT, set_fragment, NULL, "fragment"},
      {-1}
    };

transition relative_ref_fsm[] =
    {
      {0, FSM(relative_part_fsm),   1, -1, ACCEPT},
      {1, FSM(relative_ref_fsm_1),  2, 2, ACCEPT},
      {2, FSM(relative_ref_fsm_2), -1, -1, ACCEPT},
      {-1}
    };

transition hier_part_fsm[] =
    {
      {0, EXACT_STRING("//"),      1, -1, NORMAL, NULL, NULL, "matching //"},
      {0, FSM(path_absolute_fsm), -1, -1, ACCEPT, set_path, NULL, "path_absolute"},
      {0, FSM(path_rootless_fsm), -1, -1, ACCEPT, set_path, NULL, "path rootless"},
      {0, FSM(path_empty_fsm),    -1, -1, ACCEPT, set_path, NULL, "path empty"},
   
      {1, FSM(authority_fsm),      2, -1, NORMAL, NULL, NULL, "authority"},
      {2, FSM(path_abempty_fsm),  -1, -1, ACCEPT, set_path, NULL, "path abempty"},
      {-1}
    };

transition absolute_uri_fsm_1[] =
    {
      {0, EXACT_STRING("?"), 1, -1},
      {1, FSM(query_fsm),   -1, -1, ACCEPT, set_query, NULL},
      {-1}
    };

transition absolute_uri_fsm[] =
    {
      {0, FSM(scheme_fsm),          1, -1, NORMAL, set_scheme, NULL},
      {1, EXACT_STRING(":"),        2, -1, NORMAL, NULL, NULL, "matching :"},
      {2, FSM(hier_part_fsm),       3, -1, ACCEPT},
      {3, FSM(absolute_uri_fsm_1), -1, -1, ACCEPT},
      {-1}
    };

transition uri_fsm_1[] =
    {
      {0, EXACT_STRING("?"), 1, -1, NORMAL, NULL, NULL, "matching ?-query"},
      {1, FSM(query_fsm),   -1, -1, ACCEPT, set_query, NULL, "query"},
      {-1}
    };

transition uri_fsm_2[] =
    {
      {0, EXACT_STRING("#"),    1, -1, NORMAL, NULL, NULL, "matching #-fagment"},
      {1, FSM(fragment_fsm),   -1, -1, ACCEPT, set_fragment, NULL, "fragment"},
      {-1}
    };

transition uri_fsm[] =
    {
      {0, FSM(scheme_fsm),     1, -1, NORMAL, set_scheme, NULL, "scheme"},
      {1, EXACT_STRING(":"),   2, -1, NORMAL, NULL, NULL, "matching : in uri"},
      {2, FSM(hier_part_fsm),  3, -1, ACCEPT, NULL, NULL, "hier-part"},
      {3, FSM(uri_fsm_1),      4, -1, ACCEPT},
      {4, FSM(uri_fsm_2),     -1, -1, ACCEPT},
      {-1}
    };

transition uri_reference_fsm[] =
    {
      {0, FSM(uri_fsm),          -1, -1, ACCEPT, NULL, NULL, "uri"},
      {0, FSM(relative_ref_fsm), -1, -1, ACCEPT, NULL, NULL, "relative ref"},
      {-1}
    };

/** 
 * Set the scheme in the global content, a uri type variable
 * 
 * @param uri_string the raw URI string
 * @param scheme_len the length of the scheme string
 * @param global_context a pointer to a URI type
 * @param local_context should be NULL, it gets ignored
 */
static void set_scheme(char **uri_string, int scheme_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->scheme = malloc(scheme_len+1);
  if(u->scheme == NULL) {
    /* could not allocate space to store the scheme - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the scheme string\n");
    exit(1);
  }
  memcpy(u->scheme, *uri_string, scheme_len);
  u->scheme[scheme_len] = '\0';
}

static void set_userinfo(char **uri_string, int userinfo_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->userinfo = malloc(userinfo_len+1);
  if(u->userinfo == NULL) {
    /* could not allocate space to store the userinfo - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the userinfo string\n");
    exit(1);
  }
  memcpy(u->userinfo, *uri_string, userinfo_len);
  u->userinfo[userinfo_len] = '\0';
}

static void set_host(char **uri_string, int host_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->host = malloc(host_len+1);
  if(u->host == NULL) {
    /* could not allocate space to store the host - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the host string\n");
    exit(1);
  }
  memcpy(u->host, *uri_string, host_len);
  u->host[host_len] = '\0';
}

static void set_host_is_ip(char **uri_string, int not_used, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->host_is_ip = 1;
}

static void check_host_is_ip(char **uri_string, int ip_length, void *global_context, void *local_context)
{
  int ret;
 
  ret = run_fsm(ipv4address_fsm, uri_string, NULL, NULL, NULL);
  if(ret == ip_length) {
    set_host_is_ip(uri_string, ip_length, global_context, local_context);
  }
}

static void set_port(char **uri_string, int port_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  char *port_tmp;
  port_tmp = malloc(port_len+1);
  if(port_tmp == NULL) {
    /* could not allocate space to store the port - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the port string\n");
    exit(1);
  }
  memcpy(port_tmp, *uri_string, port_len);
  port_tmp[port_len] = '\0';
  u->port = atoi(port_tmp);
  free(port_tmp);
}

static void set_path(char **uri_string, int path_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->path = malloc(path_len+1);
  if(u->path == NULL) {
    /* could not allocate space to store the path - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the path string\n");
    exit(1);
  }
  memcpy(u->path, *uri_string, path_len);
  u->path[path_len] = '\0';
}

static void set_query(char **uri_string, int query_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->query = malloc(query_len+1);
  if(u->query == NULL) {
    /* could not allocate space to store the query - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the query string\n");
    exit(1);
  }
  memcpy(u->query, *uri_string, query_len);
  u->query[query_len] = '\0';
}

static void set_fragment(char **uri_string, int fragment_len, void *global_context, void *local_context)
{
  uri *u = (uri*)global_context;
  u->fragment = malloc(fragment_len+1);
  if(u->fragment == NULL) {
    /* could not allocate space to store the fragment - print an error and abort */
    fprintf(stderr, "Could not malloc the space to store the fragment string\n");
    exit(1);
  }
  memcpy(u->fragment, *uri_string, fragment_len);
  u->fragment[fragment_len] = '\0';
}

static char *xstrdup(char *in) {
  char *out;
  size_t len;
  if(in == NULL) {
    return NULL;
  }
  len = strlen(in);
  
  out = malloc(len+1);
  if(out == NULL) {
    return NULL;
  }
  memcpy(out, in, len);
  out[len] = '\0';

  return out;
}

void* duplicate_uri(void *uri_in)
{
  uri *old_uri = (uri*)uri_in;
  uri *new_uri;

  if(old_uri == NULL) {
    return NULL;
  }

  new_uri = malloc(sizeof(uri));
  if(new_uri == NULL) {
    return NULL;
  }

  new_uri->host_is_ip = old_uri->host_is_ip;
  new_uri->port = old_uri->port;
  new_uri->scheme = xstrdup(old_uri->scheme);
  new_uri->userinfo = xstrdup(old_uri->userinfo);
  new_uri->host = xstrdup(old_uri->host);
  new_uri->path = xstrdup(old_uri->path);
  new_uri->query = xstrdup(old_uri->query);
  new_uri->fragment = xstrdup(old_uri->fragment);

  return new_uri;
}

void free_uri(void *uri_in)
{
  uri *to_free = (uri*)uri_in;
  if(to_free == NULL) {
    return;
  }

  free(to_free->scheme);
  free(to_free->userinfo);
  free(to_free->host);
  free(to_free->path);
  free(to_free->query);
  free(to_free->fragment);
  free(to_free);
}

int main(int argc, char **argv)
{
  char *str, *ostr;
  int ret;
  uri *parsed_uri;
 
  /* initialize the URI structure - this needs pointers set to NULL to
     be correct! */
  parsed_uri = malloc(sizeof(uri));
  if(parsed_uri == NULL) {
    fprintf(stderr, "Unable to allocate space on the heap for a URI\n");
  }

  parsed_uri->host_is_ip = 0;
  parsed_uri->port = -1;
  parsed_uri->scheme = NULL;
  parsed_uri->userinfo = NULL;
  parsed_uri->host = NULL;
  parsed_uri->path = NULL;
  parsed_uri->query = NULL;
  parsed_uri->fragment = NULL;

  /* read a string from the user */
  str = ostr = calloc(MAX_INPUT+1, 1);
  if(str == NULL) {
    printf("Unable to allocate string storage space.\n");
    return 1;
  }
  printf("Please enter a URI:\n");
  fgets(str, MAX_INPUT, stdin);

  printf("Processing %d byte string...\n", (int)strlen(str));

  ret = run_fsm(uri_reference_fsm, &str, (void**)&parsed_uri, duplicate_uri, free_uri);
  if(ret < 0) {
    printf("Unable to execute FSM on string: %s\n", str);
  } else {  
    printf("URI scheme: %s\n", parsed_uri->scheme);
    printf("URI userinfo: %s\n", parsed_uri->userinfo);
    printf("URI host: %s\n", parsed_uri->host);
    printf("  Is host an IP address?: %s\n", (parsed_uri->host_is_ip) ? "yes" : "no");
    printf("URI port: %d\n", parsed_uri->port);
    printf("URI path: %s\n", parsed_uri->path);
    printf("URI query: %s\n", parsed_uri->query);
    printf("URI fragment: %s\n", parsed_uri->fragment); 
    printf("\nFSM Done - processed %d characters: \"%.*s\".\n", ret, ret, ostr);
  }
 
  free_uri(parsed_uri);
  free(ostr);
  return 0;
}



