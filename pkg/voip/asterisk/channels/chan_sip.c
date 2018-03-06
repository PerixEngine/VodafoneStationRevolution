/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*!
 * \file
 * \brief Implementation of Session Initiation Protocol
 * 
 * Implementation of RFC 3261 - without S/MIME, TCP and TLS support
 * Configuration file \link Config_sip sip.conf \endlink
 *
 * \todo SIP over TCP
 * \todo SIP over TLS
 * \todo Better support of forking
 */


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <regex.h>
#include <linux/version.h>
#include <sys/sysinfo.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.132.2.10.2.329.4.10 $")

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/config.h"
#include "asterisk/logger.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/options.h"
#include "asterisk/lock.h"
#include "asterisk/sched.h"
#include "asterisk/io.h"
#include "asterisk/rtp.h"
#include "rtp/rtp.h"
#if defined(T38_SUPPORT)
#include "asterisk/udptl.h"
#endif
#include "asterisk/acl.h"
#include "asterisk/manager.h"
#include "asterisk/callerid.h"
#include "asterisk/cli.h"
#include "asterisk/app.h"
#include "asterisk/musiconhold.h"
#include "asterisk/dsp.h"
#include "asterisk/features.h"
#include "asterisk/acl.h"
#include "asterisk/srv.h"
#include "asterisk/astdb.h"
#include "asterisk/causes.h"
#include "asterisk/utils.h"
#include "asterisk/file.h"
#include "asterisk/astobj.h"
#include "asterisk/dnsmgr.h"
#include "asterisk/devicestate.h"
#include "asterisk/linkedlists.h"
#include "asterisk/jdsp_common.h"
#include "asterisk/incall_announcement.h"

#ifdef OSP_SUPPORT
#include "asterisk/astosp.h"
#endif

#ifndef FALSE
#define FALSE    0
#endif

#ifndef TRUE
#define TRUE     1
#endif

#ifndef DEFAULT_USERAGENT
#define DEFAULT_USERAGENT "Asterisk PBX"
#endif
 
#define VIDEO_CODEC_MASK	0x1fc0000 /* Video codecs from H.261 thru AST_FORMAT_MAX_VIDEO */
#ifndef IPTOS_MINCOST
#define IPTOS_MINCOST		0x02
#endif

typedef struct {
	unsigned long cfg_upstream; /* maximum bandwidth we may use */
	unsigned long used_upstream; /* bandwidth we actually use */
} sip_bw_mgt_t;
 
/* for now, only one bandwidth-mgt. each peer is either in or out. */
sip_bw_mgt_t global_bw_mgt = {};

/* #define VOCAL_DATA_HACK */

#define SIP_RETVAL_IGNORE 42
#define SIPDUMPER
#define DEFAULT_DEFAULT_EXPIRY  120
#define DEFAULT_MAX_EXPIRY	30
#define DEFAULT_REGISTRATION_TIMEOUT	20
#define DEFAULT_REGISTRATION_SPACE  100
#define DEFAULT_SUBSCRIPTION_TIMEOUT 60
#define DEFAULT_SUBATTEMPTS_MAX 20
#define DEFAULT_MAX_FORWARDS	"70"
#define DEFAULT_SRV_RECOVER_TIME 30
#define DEFAULT_REG_RECOVER_TIME 300

/* guard limit must be larger than guard secs */
/* guard min must be < 1000, and should be >= 250 */
#define EXPIRY_GUARD_SECS	300	/* How long before expiry do we reregister */
#define EXPIRY_GUARD_LIMIT	600	/* Below here, we use EXPIRY_GUARD_PCT instead of 
					   EXPIRY_GUARD_SECS */
#define EXPIRY_GUARD_MIN	500	/* This is the minimum guard time applied. If 
					   GUARD_PCT turns out to be lower than this, it 
					   will use this time instead.
					   This is in milliseconds. */

static int max_expiry = DEFAULT_MAX_EXPIRY;
static int default_expiry = DEFAULT_DEFAULT_EXPIRY;
static int default_regspacing = DEFAULT_REGISTRATION_SPACE;
static int default_reg_period = 0;
static int default_subscription_expiry = DEFAULT_MAX_EXPIRY;
static int default_expiry_server = DEFAULT_MAX_EXPIRY;
static int instant_dial_enabled = 0;
static int use_asserted_identity = 0;
static int unregister_existing_bindings = 0;
static int ip_changed = 0;
static int check_for_unique_register = 0;

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define CALLERID_UNKNOWN	"Unknown"



#define DEFAULT_MAXMS		2000		/* Must be faster than 2 seconds by default */
#define DEFAULT_FREQ_OK		60 * 1000	/* How often to check for the host to be up */
#define DEFAULT_FREQ_NOTOK	10 * 1000	/* How often to check, if the host is down... */

#define DEFAULT_RETRANS		500		/* How frequently to retransmit */
						/* 500 ms in RFC 3261 */
#define MAX_RETRANS		10			/* Try only 10 times for retransmissions, a total of 7 transmissions */
#define MAX_INVITE_RETRANS	6		/* Try only 6 retransmissions of INVITE packets, according to RFC3261 */
#define MAX_AUTHTRIES		1		/* Try authentication only once, if it fails now, it will fail always */


#define DEBUG_READ	0			/* Recieved data	*/
#define DEBUG_SEND	1			/* Transmit data	*/

#define PTIME_MIN 5                             /* ptime in SDP minimum value */
#define PTIME_MAX 100                           /* ptime in SDP maximum value */

#define DEFAULT_MAX_SE               1800             /*!< Session-Timer Default Session-Expires period (RFC 4028) */
#define DEFAULT_MIN_SE               90               /*!< Session-Timer Default Min-SE period (RFC 4028) */

static const char desc[] = "Session Initiation Protocol (SIP)";
static const char channeltype[] = "SIP";
static const char config[] = "sip.conf";
static const char notify_config[] = "sip_notify.conf";
static const char register_backup_prefix[] = "BKP-";

static unsigned char conf_file_md5[MD5_DIGEST_LEN];

#define RTP 	1
#define NO_RTP	0

/* Do _NOT_ make any changes to this enum, or the array following it;
   if you think you are doing the right thing, you are probably
   not doing the right thing. If you think there are changes
   needed, get someone else to review them first _before_
   submitting a patch. If these two lists do not match properly
   bad things will happen.
*/

enum subscriptiontype { 
	NONE = 0,
	TIMEOUT,
	XPIDF_XML,
	DIALOG_INFO_XML,
	CPIM_PIDF_XML,
	PIDF_XML
};

static const struct cfsubscription_types {
	enum subscriptiontype type;
	const char * const event;
	const char * const mediatype;
	const char * const text;
} subscription_types[] = {
	{ NONE,            "-",        "unknown",	                  "unknown" },
 	/* IETF draft: draft-ietf-sipping-dialog-package-05.txt */
	{ DIALOG_INFO_XML, "dialog",   "application/dialog-info+xml", "dialog-info+xml" },
	{ CPIM_PIDF_XML,   "presence", "application/cpim-pidf+xml",   "cpim-pidf+xml" },  /* RFC 3863 */
	{ PIDF_XML,        "presence", "application/pidf+xml",        "pidf+xml" },       /* RFC 3863 */
	{ XPIDF_XML,       "presence", "application/xpidf+xml",       "xpidf+xml" }       /* Pre-RFC 3863 with MS additions */
};

static const char *binary_content_types[] = {
	"application/vnd.3gpp.sms",
	NULL
};

enum sipmethod {
	SIP_UNKNOWN,
	SIP_RESPONSE,
	SIP_REGISTER,
	SIP_OPTIONS,
	SIP_NOTIFY,
	SIP_INVITE,
	SIP_ACK,
	SIP_PRACK,
	SIP_BYE,
	SIP_REFER,
	SIP_SUBSCRIBE,
	SIP_MESSAGE,
	SIP_UPDATE,
	SIP_INFO,
	SIP_CANCEL,
	SIP_PUBLISH,
} sip_method_list;

enum sip_auth_type {
	PROXY_AUTH,
	WWW_AUTH,
};

/*! \brief Modes in which Asterisk can be configured to run SIP Session-Timers */
enum st_mode {
        SESSION_TIMER_MODE_INVALID = 0, /*!< Invalid value */ 
        SESSION_TIMER_MODE_ACCEPT,      /*!< Honor inbound Session-Timer requests */
        SESSION_TIMER_MODE_ORIGINATE,   /*!< Originate outbound and honor inbound requests */
        SESSION_TIMER_MODE_REFUSE       /*!< Ignore inbound Session-Timers requests */
};

/*! \brief The entity playing the refresher role for Session-Timers */
enum st_refresher {
        SESSION_TIMER_REFRESHER_AUTO,    /*!< Negotiated                      */
        SESSION_TIMER_REFRESHER_LOCAL,     /*!< Session is refreshed by the local UA */
        SESSION_TIMER_REFRESHER_REMOTE      /*!< Session is refreshed by the remote UA */
};

/*! XXX Note that sip_methods[i].id == i must hold or the code breaks */
static const struct  cfsip_methods { 
	enum sipmethod id;
	int need_rtp;		/*!< when this is the 'primary' use for a pvt structure, does it need RTP? */
	char * const text;
} sip_methods[] = {
	{ SIP_UNKNOWN,	 RTP,    "-UNKNOWN-" },
	{ SIP_RESPONSE,	 NO_RTP, "SIP/2.0" },
	{ SIP_REGISTER,	 NO_RTP, "REGISTER" },
 	{ SIP_OPTIONS,	 NO_RTP, "OPTIONS" },
	{ SIP_NOTIFY,	 NO_RTP, "NOTIFY" },
	{ SIP_INVITE,	 RTP,    "INVITE" },
	{ SIP_ACK,	 NO_RTP, "ACK" },
	{ SIP_PRACK,	 NO_RTP, "PRACK" },
	{ SIP_BYE,	 NO_RTP, "BYE" },
	{ SIP_REFER,	 NO_RTP, "REFER" },
	{ SIP_SUBSCRIBE, NO_RTP, "SUBSCRIBE" },
	{ SIP_MESSAGE,	 NO_RTP, "MESSAGE" },
	{ SIP_UPDATE,	 NO_RTP, "UPDATE" },
	{ SIP_INFO,	 NO_RTP, "INFO" },
	{ SIP_CANCEL,	 NO_RTP, "CANCEL" },
	{ SIP_PUBLISH,	 NO_RTP, "PUBLISH" }
};

/*! \brief Structure for conversion between compressed SIP and "normal" SIP */
static const struct cfalias {
	char * const fullname;
	char * const shortname;
} aliases[] = {
	{ "Content-Type", "c" },
	{ "Content-Encoding", "e" },
	{ "From", "f" },
	{ "Call-ID", "i" },
	{ "Contact", "m" },
	{ "Content-Length", "l" },
	{ "Subject", "s" },
	{ "To", "t" },
	{ "Supported", "k" },
	{ "Refer-To", "r" },
	{ "Referred-By", "b" },
	{ "Allow-Events", "u" },
	{ "Event", "o" },
	{ "Via", "v" },
	{ "Accept-Contact",      "a" },
	{ "Reject-Contact",      "j" },
	{ "Request-Disposition", "d" },
	{ "Session-Expires",     "x" },
};

/*!  Define SIP option tags, used in Require: and Supported: headers 
 	We need to be aware of these properties in the phones to use 
	the replace: header. We should not do that without knowing
	that the other end supports it... 
	This is nothing we can configure, we learn by the dialog
	Supported: header on the REGISTER (peer) or the INVITE
	(other devices)
	We are not using many of these today, but will in the future.
	This is documented in RFC 3261
*/
#define SUPPORTED		1
#define NOT_SUPPORTED		0

#define SIP_OPT_REPLACES	(1 << 0)
#define SIP_OPT_100REL		(1 << 1)
#define SIP_OPT_TIMER		(1 << 2)
#define SIP_OPT_EARLY_SESSION	(1 << 3)
#define SIP_OPT_JOIN		(1 << 4)
#define SIP_OPT_PATH		(1 << 5)
#define SIP_OPT_PREF		(1 << 6)
#define SIP_OPT_PRECONDITION	(1 << 7)
#define SIP_OPT_PRIVACY		(1 << 8)
#define SIP_OPT_SDP_ANAT	(1 << 9)
#define SIP_OPT_SEC_AGREE	(1 << 10)
#define SIP_OPT_EVENTLIST	(1 << 11)
#define SIP_OPT_GRUU		(1 << 12)
#define SIP_OPT_TARGET_DIALOG	(1 << 13)
#define SIP_OPT_UNKNOWN         (1 << 14)

/*! \brief List of well-known SIP options. If we get this in a require,
   we should check the list and answer accordingly. */
static const struct cfsip_options {
	int id;			/*!< Bitmap ID */
	int supported;		/*!< Supported by Asterisk ? */
	char * const text;	/*!< Text id, as in standard */
} sip_options[] = {
	/* Replaces: header for transfer */
	{ SIP_OPT_REPLACES,	SUPPORTED,	"replaces" },	
	/* RFC3262: PRACK 100% reliability */
	{ SIP_OPT_100REL,	SUPPORTED,	"100rel" },	
	/* SIP Session Timers */
	{ SIP_OPT_TIMER,	SUPPORTED,	"timer" },
	/* RFC3959: SIP Early session support */
	{ SIP_OPT_EARLY_SESSION, NOT_SUPPORTED,	"early-session" },
	/* SIP Join header support */
	{ SIP_OPT_JOIN,		NOT_SUPPORTED,	"join" },
	/* RFC3327: Path support */
	{ SIP_OPT_PATH,		NOT_SUPPORTED,	"path" },
	/* RFC3840: Callee preferences */
	{ SIP_OPT_PREF,		NOT_SUPPORTED,	"pref" },
	/* RFC3312: Precondition support */
	{ SIP_OPT_PRECONDITION,	NOT_SUPPORTED,	"precondition" },
	/* RFC3323: Privacy with proxies*/
	{ SIP_OPT_PRIVACY,	SUPPORTED,	"privacy" },
	/* RFC4092: Usage of the SDP ANAT Semantics in the SIP */
	{ SIP_OPT_SDP_ANAT,	NOT_SUPPORTED,	"sdp-anat" },
	/* RFC3329: Security agreement mechanism */
	{ SIP_OPT_SEC_AGREE,	NOT_SUPPORTED,	"sec_agree" },
	/* SIMPLE events:  draft-ietf-simple-event-list-07.txt */
	{ SIP_OPT_EVENTLIST,	NOT_SUPPORTED,	"eventlist" },
	/* GRUU: Globally Routable User Agent URI's */
	{ SIP_OPT_GRUU,		NOT_SUPPORTED,	"gruu" },
	/* Target-dialog: draft-ietf-sip-target-dialog-00.txt */
	{ SIP_OPT_TARGET_DIALOG,NOT_SUPPORTED,	"target-dialog" },
};


/*! \brief SIP Methods we support */
#define ALLOWED_METHODS "INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, SUBSCRIBE, NOTIFY, INFO, UPDATE"
#define ALLOWED_METHODS_PRACK ALLOWED_METHODS ", PRACK"

#define DEFAULT_SIP_PORT	5060	/*!< From RFC 3261 (former 2543) */
#define SIP_MAX_PACKET		4096	/*!< Also from RFC 3261 (2543), should sub headers tho */

#define ARRAY_LEN(a) (sizeof(a) / sizeof(0[a]))

static char default_useragent[AST_MAX_EXTENSION] = DEFAULT_USERAGENT;

static int modemfax_tx_reinvite_delay = 0;
static int modemfax_rx_reinvite_delay = 0;

#define DEFAULT_CONTEXT "default"
static char default_context[AST_MAX_CONTEXT] = DEFAULT_CONTEXT;
static char default_subscribecontext[AST_MAX_CONTEXT];

#define DEFAULT_VMEXTEN "asterisk"
static char global_vmexten[AST_MAX_EXTENSION] = DEFAULT_VMEXTEN;

static char default_language[MAX_LANGUAGE] = "";

#define NETWORK_ANONYMOUS_CALLERID "Anonymous"
#define NETWORK_ANONYMOUS_USERNAME "anonymous"
#define NETWORK_ANONYMOUS_DOMAIN "anonymous.invalid"
#define AST_ANONYMOUS_CALLER ""

#define DEFAULT_CALLERID NETWORK_ANONYMOUS_CALLERID
static char default_callerid[AST_MAX_EXTENSION] = DEFAULT_CALLERID;

static char default_fromdomain[AST_MAX_EXTENSION] = "";
static char default_domain[AST_MAX_EXTENSION] = "";

#define DEFAULT_NOTIFYMIME "application/simple-message-summary"
static char default_notifymime[AST_MAX_EXTENSION] = DEFAULT_NOTIFYMIME;

static int global_notifyringing = 1;	/*!< Send notifications on ringing */

static int default_qualify = 0;		/*!< Default Qualify= setting */

static struct ast_flags global_flags = {0};		/*!< global SIP_ flags */
static struct ast_flags global_flags_page2 = {0};	/*!< more global SIP_ flags */

static int srvlookup = 0;		/*!< SRV Lookup on or off. Default is off, RFC behavior is on */

/* srv failover parameters*/
static int srv_failover_enabled = 0; /*!< Srv failover feature on or off. */
static int min_srv_ttl = 0 /*!< Minimum ttl for srv entries */;
static int srv_recover_time = DEFAULT_SRV_RECOVER_TIME;
static int reg_recover_time = DEFAULT_REG_RECOVER_TIME;

static int pedanticsipchecking = 0;	/*!< Extra checking ?  Default off */

static int autocreatepeer = 0;		/*!< Auto creation of peers at registration? Default off. */

static int relaxdtmf = 0;

static int inbandtonedetect = 0;

static int global_rtptimeout = 0;

static int global_rtpholdtimeout = 0;

static int global_rtpkeepalive = 0;

static int global_reg_timeout = DEFAULT_REGISTRATION_TIMEOUT;	
static int global_regattempts_max = 0;

static int global_vf_s1_delay_backup_proxy = 5000;
static int global_vf_s2_delay_primary_proxy = 15000;
static int global_vf_t2_delay_recover_primary = 20000;
static int global_vf_1s_delay_dereg_backup = 1000;

static int global_sub_timeout = DEFAULT_SUBSCRIPTION_TIMEOUT;
static int global_subattempts_max = DEFAULT_SUBATTEMPTS_MAX;

static int global_max_sessions = 0;

static int global_max_from_templates = 0;
static int global_reg_from_templates = 0;

static int global_template_registration_forbid = 0;

static int is_mwi_external = 0;
static int expires_renew_percentage = 80;
/* XXX: These are counters for the external voicemail messages (i.e. messages
 * which are handled externally by the SIP proxy). These counters are global,
 * so they will not work well when we are registered with more than one
 * voicemail-providing proxy (our status will always be updated with the last
 * proxy that sent us a NOTIFY message). */
static int new_msg_count = 0;
static int old_msg_count = 0;

/* Object counters */
static int suserobjs = 0;
static int ruserobjs = 0;
static int speerobjs = 0;
static int rpeerobjs = 0;
static int apeerobjs = 0;
static int regobjs = 0;
static int subobjs = 0;

static enum st_mode global_st_mode;           /*!< Mode of operation for Session-Timers           */
static enum st_refresher global_st_refresher; /*!< Session-Timer refresher                        */
static int global_st_refresher_force;         /*!< Send refresher in the outgoing initial invite  */
static int global_min_se;                     /*!< Lowest threshold for session refresh interval  */
static int global_max_se;                     /*!< Highest threshold for session refresh interval */

static int global_allowguest = 1;    /*!< allow unauthenticated users/peers to connect? */
static int global_allow_p2p_calls = 1; /*!< allow peer to peer communication */

/* PRACK support as defined in RFC 3262. May be either none, supported (but not
 * required), or required. */

#define PRACK_LEVEL_NONE        0
#define PRACK_LEVEL_SUPPORTED   1
#define PRACK_LEVEL_REQUIRE     2

#define DEFAULT_MWITIME 10
static int global_mwitime = DEFAULT_MWITIME;	/*!< Time between MWI checks for peers */

static int usecnt =0;
AST_MUTEX_DEFINE_STATIC(usecnt_lock);

AST_MUTEX_DEFINE_STATIC(rand_lock);

/*! \brief Protect the interface list (of sip_pvt's) */
AST_MUTEX_DEFINE_STATIC(iflock);

/*! \brief Protect the monitoring thread, so only one process can kill or start it, and not
   when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(netlock);

AST_MUTEX_DEFINE_STATIC(monlock);
AST_MUTEX_DEFINE_STATIC(bw_mgt_lock);

/*! \brief This is the thread for the monitor which checks for input on the channels
   which are not currently in use.  */
static pthread_t monitor_thread = AST_PTHREADT_NULL;

static int restart_monitor(void);

#if defined(T38_SUPPORT)
/* T.38 set of flags */
#define T38FAX_FILL_BIT_REMOVAL			(1 << 0) 	/*!< Default: 0 (unset)*/
#define T38FAX_TRANSCODING_MMR			(1 << 1)	/*!< Default: 0 (unset)*/
#define T38FAX_TRANSCODING_JBIG			(1 << 2)	/*!< Default: 0 (unset)*/
/* Rate management */
#define T38FAX_RATE_MANAGEMENT_TRANSFERED_TCF	(0 << 3)
#define T38FAX_RATE_MANAGEMENT_LOCAL_TCF	(1 << 3)	/*!< Unset for transferedTCF (UDPTL), set for localTCF (TPKT) */
/* UDP Error correction */
#define T38FAX_UDP_EC_NONE			(0 << 4)	/*!< two bits, if unset NO t38UDPEC field in T38 SDP*/
#define T38FAX_UDP_EC_FEC			(1 << 4)	/*!< Set for t38UDPFEC */
#define T38FAX_UDP_EC_REDUNDANCY		(2 << 4)	/*!< Set for t38UDPRedundancy */
/* T38 Spec version */
#define T38FAX_VERSION				(3 << 6)	/*!< two bits, 2 values so far, up to 4 values max */ 
#define T38FAX_VERSION_0			(0 << 6)	/*!< Version 0 */ 
#define T38FAX_VERSION_1			(1 << 6)	/*!< Version 1 */
/* Maximum Fax Rate */
#define T38FAX_RATE_2400			(1 << 8)	/*!< 2400 bps t38FaxRate */
#define T38FAX_RATE_4800			(1 << 9)	/*!< 4800 bps t38FaxRate */
#define T38FAX_RATE_7200			(1 << 10)	/*!< 7200 bps t38FaxRate */
#define T38FAX_RATE_9600			(1 << 11)	/*!< 9600 bps t38FaxRate */
#define T38FAX_RATE_12000			(1 << 12)	/*!< 12000 bps t38FaxRate */
#define T38FAX_RATE_14400			(1 << 13)	/*!< 14400 bps t38FaxRate */
#endif

/*! \brief Codecs that we support by default: */
static int global_capability = AST_FORMAT_ULAW | AST_FORMAT_ALAW | AST_FORMAT_GSM | AST_FORMAT_H263;

#if defined(T38_SUPPORT)
static int global_t38_capability = T38FAX_VERSION_0 | T38FAX_RATE_2400 | T38FAX_RATE_4800 | T38FAX_RATE_7200 | T38FAX_RATE_9600 | T38FAX_RATE_14400; /* This is default: NO MMR and JBIG trancoding, NO fill bit removal, transfered TCF, UDP FEC, Version 0 and 14400 max fax rate */
#endif

#if defined(T38_SUPPORT)
static int t38udptlsupport = 0;
static int t38rtpsupport = 0;
static int t38tcpsupport = 0;
#endif

static struct in_addr __ourip;
static struct sockaddr_in outboundproxyip;
static int ourport;

#define SIP_DEBUG_CONFIG 1 << 0
#define SIP_DEBUG_CONSOLE 1 << 1
static int sipdebug = 0;
static struct sockaddr_in debugaddr;

static int tos = 0;

static int so_mark = 0;

static int videosupport = 0;

static int compactheaders = 0;				/*!< send compact sip headers */

static int recordhistory = 0;				/*!< Record SIP history. Off by default */
static int dumphistory = 0;				/*!< Dump history to verbose before destroying SIP dialog */

static char global_musicclass[MAX_MUSICCLASS] = "";	/*!< Global music on hold class */
#define DEFAULT_REALM	"asterisk"
static char global_realm[MAXHOSTNAMELEN] = DEFAULT_REALM; 	/*!< Default realm */
static char regcontext[AST_MAX_CONTEXT] = "";		/*!< Context for auto-extensions */

#define DEFAULT_EXPIRY 900				/*!< Expire slowly */
static int expiry = DEFAULT_EXPIRY;

#define DEFAULT_T1MIN	100				/*!< Minimial T1 roundtrip time - ms */

static struct sched_context *sched;
static struct io_context *io;
static int *sipsock_read_id = NULL;

#define SIP_MAX_HEADERS		64			/*!< Max amount of SIP headers to read */
#define SIP_MAX_LINES 		64			/*!< Max amount of lines in SIP attachment (like SDP) */

#define SIP_MAX_OFFER_MEDIA 3			/*!< Max media types in SDP */
#define DEC_CALL_LIMIT	0
#define INC_CALL_LIMIT	1
#define SCHED_CANCEL(TO) \
    {if (TO > -1) {       \
        ast_sched_del(sched, TO); \
        TO = -1; \
    }}

static struct ast_codec_pref global_prefs;
static int global_preferred_codec = 1;
static struct ast_codec_pref sip_proxy_internal_call_prefs;
static struct sip_peer_quality global_highqualitycalls;

typedef struct contact_list_t {
	struct contact_list_t *next;
	char host[256];
	int port;
} contact_list_t;


/*! \brief sip_request: The data grabbed from the UDP socket */
struct sip_request {
	char *rlPart1; 		/*!< SIP Method Name or "SIP/2.0" protocol version */
	char *rlPart2; 		/*!< The Request URI or Response Status */
	int len;		/*!< Length */
	int headers;		/*!< # of SIP Headers */
	int method;		/*!< Method of this request */
	char *header[SIP_MAX_HEADERS];
	int lines;		/*!< SDP Content */
	char *line[SIP_MAX_LINES];
	char data[SIP_MAX_PACKET];
	char *body; /*!< Beginning of SIP message body, in received requests */
	int body_len; /*!< Length of SIP message body, in received requests */
	int debug;		/*!< Debug flag for this packet */
	unsigned int flags;	/*!< SIP_PKT Flags for this packet */
};

struct sip_pkt;

/*! \brief Parameters to the transmit_invite function */
struct sip_invite_param {
	char *distinctive_ring;	/*!< Distinctive ring header */
	char *osptoken;		/*!< OSP token for this call */
	int addsipheaders;	/*!< Add extra SIP headers */
	char *uri_options;	/*!< URI options to add to the URI */
	char *vxml_url;		/*!< VXML url for Cisco phones */
	char *auth;		/*!< Authentication */
	char *authheader;	/*!< Auth header */
	enum sip_auth_type auth_type;	/*!< Authentication type */
};

struct sip_route {
	struct sip_route *next;
	char hop[0];
};

enum domain_mode {
	SIP_DOMAIN_AUTO,	/*!< This domain is auto-configured */
	SIP_DOMAIN_CONFIG,	/*!< This domain is from configuration */
};

struct domain {
	char domain[MAXHOSTNAMELEN];		/*!< SIP domain we are responsible for */
	char context[AST_MAX_EXTENSION];	/*!< Incoming context for this domain */
	enum domain_mode mode;			/*!< How did we find this domain? */
	AST_LIST_ENTRY(domain) list;		/*!< List mechanics */
};

static AST_LIST_HEAD_STATIC(domain_list, domain);	/*!< The SIP domain list */

int allow_external_domains;		/*!< Accept calls to external SIP domains? */

/*! \brief sip_history: Structure for saving transactions within a SIP dialog */
struct sip_history {
	char event[80];
	struct sip_history *next;
};

/*! \brief sip_auth: Creadentials for authentication to other SIP services */
struct sip_auth {
	char realm[AST_MAX_EXTENSION];  /*!< Realm in which these credentials are valid */
	char username[256];             /*!< Username */
	char secret[256];               /*!< Secret */
	char md5secret[256];            /*!< MD5Secret */
	struct sip_auth *next;          /*!< Next auth structure in list */
};

#define SIP_ALREADYGONE		(1 << 0)	/*!< Whether or not we've already been destroyed by our peer */
#define SIP_NEEDDESTROY		(1 << 1)	/*!< if we need to be destroyed */
#define SIP_NOVIDEO		(1 << 2)	/*!< Didn't get video in invite, don't offer */
#define SIP_RINGING		(1 << 3)	/*!< Have sent 180 ringing */
#define SIP_PROGRESS_SENT	(1 << 4)	/*!< Have sent 183 message progress */
#define SIP_NEEDREINVITE	(1 << 5)	/*!< Do we need to send another reinvite? */
#define SIP_PENDINGBYE		(1 << 6)	/*!< Need to send bye after we ack? */
#define SIP_GOTREFER		(1 << 7)	/*!< Got a refer? */
#define SIP_PROMISCREDIR	(1 << 8)	/*!< Promiscuous redirection */
#define SIP_TRUSTRPID		(1 << 9)	/*!< Trust RPID headers? */
#define SIP_USEREQPHONE		(1 << 10)	/*!< Add user=phone to numeric URI. Default off */
#define SIP_REALTIME		(1 << 11)	/*!< Flag for realtime users */
#define SIP_USECLIENTCODE	(1 << 12)	/*!< Trust X-ClientCode info message */
#define SIP_OUTGOING		(1 << 13)	/*!< Is this an outgoing call? */
#define SIP_SELFDESTRUCT	(1 << 14)	
#define SIP_DYNAMIC		(1 << 15)	/*!< Is this a dynamic peer? */
/* --- Choices for DTMF support in SIP channel */
#define SIP_DTMF		(3 << 16)	/*!< three settings, uses two bits */
#define SIP_DTMF_RFC2833	(0 << 16)	/*!< RTP DTMF */
#define SIP_DTMF_INBAND		(1 << 16)	/*!< Inband audio, only for ULAW/ALAW */
#define SIP_DTMF_INFO		(2 << 16)	/*!< SIP Info messages */
#define SIP_DTMF_AUTO		(3 << 16)	/*!< AUTO switch between rfc2833 and in-band DTMF */
/* NAT settings */
#define SIP_NAT			(3 << 18)	/*!< four settings, uses two bits */
#define SIP_NAT_NEVER		(0 << 18)	/*!< No nat support */
#define SIP_NAT_RFC3581		(1 << 18)
#define SIP_NAT_ROUTE		(2 << 18)
#define SIP_NAT_ALWAYS		(3 << 18)
/* re-INVITE related settings */
#define SIP_REINVITE		(3 << 20)	/*!< two bits used */
#define SIP_CAN_REINVITE	(1 << 20)	/*!< allow peers to be reinvited to send media directly p2p */
#define SIP_REINVITE_UPDATE	(2 << 20)	/*!< use UPDATE (RFC3311) when reinviting this peer */
/* "insecure" settings */
#define SIP_INSECURE_PORT	(1 << 22)	/*!< don't require matching port for incoming requests */
#define SIP_INSECURE_INVITE	(1 << 23)	/*!< don't require authentication for incoming INVITEs */
/* Sending PROGRESS in-band settings */
#define SIP_PROG_INBAND		(3 << 24)	/*!< three settings, uses two bits */
#define SIP_PROG_INBAND_NEVER	(0 << 24)
#define SIP_PROG_INBAND_NO	(1 << 24)
#define SIP_PROG_INBAND_YES	(2 << 24)
/* Open Settlement Protocol authentication */
#define SIP_OSPAUTH		(3 << 26)	/*!< four settings, uses two bits */
#define SIP_OSPAUTH_NO		(0 << 26)
#define SIP_OSPAUTH_GATEWAY	(1 << 26)
#define SIP_OSPAUTH_PROXY	(2 << 26)
#define SIP_OSPAUTH_EXCLUSIVE	(3 << 26)
/* Call states */
#define SIP_CALL_ONHOLD		(1 << 28)	 
#define SIP_CALL_LIMIT		(1 << 29)
/* Remote Party-ID Support */
#define SIP_SENDRPID		(1 << 30)
/* Compatibility Mode */
#define SIP_COMPAT				(1 << 31)
#define SIP_COMPAT_OFF			(0 << 31)
#define SIP_COMPAT_BROADSOFT	(1 << 31)
/* Did this connection increment the counter of in-use calls? */
#define SIP_INC_COUNT (1 << 31)

#define SIP_FLAGS_TO_COPY \
	(SIP_PROMISCREDIR | SIP_TRUSTRPID | SIP_SENDRPID | SIP_DTMF | SIP_REINVITE | \
	 SIP_PROG_INBAND | SIP_OSPAUTH | SIP_USECLIENTCODE | SIP_NAT | \
	 SIP_INSECURE_PORT | SIP_INSECURE_INVITE | SIP_COMPAT)

/* a new page of flags for peer */
#define SIP_PAGE2_RTCACHEFRIENDS	(1 << 0)
#define SIP_PAGE2_RTUPDATE		(1 << 1)
#define SIP_PAGE2_RTAUTOCLEAR		(1 << 2)
#define SIP_PAGE2_IGNOREREGEXPIRE	(1 << 3)
#define SIP_PAGE2_RT_FROMCONTACT 	(1 << 4)
#define SIP_PAGE2_G729_ANNEXB	 	(1 << 5)
#define SIP_PAGE2_INVITECANCELLED   	(1 << 6)
#define SIP_PAGE2_OUTGOING_CALL		(1 << 7)
#define SIP_PAGE2_UNANSWERED_OFFER  (1 << 8)
#define SIP_PAGE2_FEATURE_3GPP_SMS  (1 << 9)
#define SIP_PAGE2_TEMPLATE  		(1 << 10)
#define SIP_PAGE2_FROM_TEMPLATE  	(1 << 11)
#define SIP_PAGE2_SESSION_REFRESH_UPDATE (1 << 13)
#define SIP_PAGE2_SESSION_TIMERS_FORCE (1 << 14)
#define SIP_PAGE2_DIALOG_ESTABLISHED    (1 << 15) 
#define SIP_PAGE2_PEERONHOLD	 	(1 << 31)

#define SIP_SESSION_TIMERS_FLAGS_TO_COPY \
	(SIP_PAGE2_SESSION_REFRESH_UPDATE | SIP_PAGE2_SESSION_TIMERS_FORCE)

/* SIP packet flags */
#define SIP_PKT_DEBUG		(1 << 0)	/*!< Debug this packet */
#define SIP_PKT_WITH_TOTAG	(1 << 1)	/*!< This packet has a to-tag */

/* SIP sdp media types */
#define SIP_MEDIA_NONE 0
#define SIP_MEDIA_AUDIO 1
#define SIP_MEDIA_VIDEO 2
#define SIP_MEDIA_T38 3

static int global_rtautoclear = 120;

AST_MUTEX_DEFINE_STATIC(quality_lock);
 	 
struct sip_peer_quality {
    int max;        /*!< What's the maximum quality possible */
    int current;    /*!< What's the current quality in use */
};

/*! \brief T38 States for a call */
enum t38state {
	T38_DISABLED = 0,                /*!< Not enabled */
	T38_LOCAL_DIRECT,                /*!< Offered from local */
	T38_LOCAL_REINVITE,              /*!< Offered from local - REINVITE */
	T38_PEER_DIRECT,                 /*!< Offered from peer */
	T38_PEER_REINVITE,               /*!< Offered from peer - REINVITE */
	T38_ENABLED                      /*!< Negotiated (enabled) */
};

/* Voice Band Data Mode */
enum vbdmode {
	VBD_MODE_NONE = 0,
	VBD_MODE_FAX,
	VBD_MODE_MODEM
};

/*! \brief Structure that encapsulates all attributes related to running 
 *   SIP Session-Timers feature on a per dialog basis.
 */
struct sip_st_dlg {
	int st_active;                          /*!< Session-Timers on/off */ 
	int st_interval;                        /*!< Session-Timers negotiated session refresh interval */
	int st_min_se;
	int st_schedid;                         /*!< Session-Timers ast_sched scheduler id */
	enum st_refresher st_ref;               /*!< Session-Timers session refresher */
	int st_expirys;                         /*!< Session-Timers number of expirys */
	int st_active_peer_ua;                  /*!< Session-Timers on/off in peer UA */
	int st_cached_min_se;                   /*!< Session-Timers cached Min-SE */
	int st_cached_max_se;                   /*!< Session-Timers cached Session-Expires */
	enum st_mode st_cached_mode;            /*!< Session-Timers cached M.O. */
	enum st_refresher st_cached_ref;        /*!< Session-Timers cached refresher */
};

/*! \brief Structure that encapsulates all attributes related to configuration 
 *   of SIP Session-Timers feature on a per user/peer basis.
 */
struct sip_st_cfg {
	enum st_mode st_mode_oper;      /*!< Mode of operation for Session-Timers           */
	enum st_refresher st_ref;       /*!< Session-Timer refresher                        */
	int st_min_se;                  /*!< Lowest threshold for session refresh interval  */
	int st_max_se;                  /*!< Highest threshold for session refresh interval */
};

enum callwaiting {
    	CALLWAITING_NONE = 0, /* no callwaiting policy applied */
	CALLWAITING_ON = 1,   
	CALLWAITING_OFF = 2
};

/* According to RFC 3262,
 * "After the first reliable provisional response for a request has been
 * acknowledged, the UAS MAY send additional reliable provisional
 * responses.  The UAS MUST NOT send a second reliable provisional
 * response until the first is acknowledged.  After the first, it is
 * RECOMMENDED that the UAS not send an additional reliable provisional
 * response until the previous is acknowledged.  The first reliable
 * provisional response receives special treatment because it conveys
 * the initial sequence number.  If additional reliable provisional
 * responses were sent before the first was acknowledged, the UAS could
 * not be certain these were received in order." 
 * Thus we save the state of our responses sent with 100rel, and avoid sending
 * further responses with 100rel until PRACK is received for first 100rel
 * response sent. */
typedef enum {
	PRACK_NONE = 0,
	PRACK_FIRST_ACK_PENDING = 1,
	PRACK_FIRST_ACK_RECEIVED = 2,
} prack_status_t;

/*! \brief sip_pvt: PVT structures are used for each SIP conversation, ie. a call  */
static struct sip_pvt {
	ast_mutex_t lock;			/*!< Channel private lock */
	int method;				/*!< SIP method of this packet */
	char callid[80];			/*!< Global CallID */
	char randdata[80];			/*!< Random data */
	struct ast_codec_pref userprefs;	/*!< codec prefs */
	int usercapability;                     /*!< user capabilities */
	struct ast_codec_pref formats;          /*!< current channel formats */
	unsigned int ocseq;			/*!< Current outgoing seqno */
	unsigned int icseq;			/*!< Current incoming seqno */
	ast_group_t callgroup;			/*!< Call group */
	ast_group_t pickupgroup;		/*!< Pickup group */
	int lastinvite;				/*!< Last Cseq of invite */
	int lastprack;				/*!< Last Cseq of prack */
	unsigned int flags;			/*!< SIP_ flags */	
	int timer_t1;				/*!< SIP timer T1, ms rtt */
	unsigned int sipoptions;		/*!< Supported SIP sipoptions on the other end */
	unsigned int reqsipoptions;		/*!< Required SIP options on the other end */
	int noncodeccapability;
#if defined(T38_SUPPORT)
	int t38capability;			/*!< Our T38 capability */
	int t38peercapability;			/*!< Peers T38 capability */
	int t38jointcapability;			/*!< Supported T38 capability at both ends */
	int t38state;				/*!< T.38 state : 0 not enabled, 1 offered from local - direct, 2 - offered from local - reinvite, 3 - offered from peer - direct, 4 offered from peer - reinvite, 5 negotiated (enabled) */
#endif
	long used_upstream;			/*!< used upstream bandwidth */
	int vbdmode;
	int callingpres;			/*!< Calling presentation */
	int authtries;				/*!< Times we've tried to authenticate */
	int expiry;				/*!< How long we take to expire */
	int branch;				/*!< One random number */
	long invite_branch;			/*!< The branch used when we sent the initial INVITE */
	char tag[11];				/*!< Another random number */
	int sessionid;				/*!< SDP Session ID */
	int sessionversion;			/*!< SDP Session Version */
	int sessionversion_remote;		/*!< Remote UA's SDP Session Version */
	int session_modify;			/*!< Session modification request true/false  */
	struct sockaddr_in sa;			/*!< Our peer */
	struct sockaddr_in redirip;		/*!< Where our RTP should be going if not to us */
	struct sockaddr_in vredirip;		/*!< Where our Video RTP should be going if not to us */
	struct ast_codec_pref redircodecs;	/*!< Redirect codecs */

#if defined(T38_SUPPORT)
	struct sockaddr_in udptlredirip;	/*!< Where our T.38 UDPTL should be going if not to us */
#endif
	struct sockaddr_in recv;		/*!< Received as */
	struct in_addr ourip;			/*!< Our IP */
	struct ast_channel *owner;		/*!< Who owns us */
	char exten[AST_MAX_EXTENSION];		/*!< Extension where to start */
	char refer_to[256];			/*!< Place to store REFER-TO extension */
	char refer_to_domain[256];		/*!< Place to store REFER-TO domain */
	char referred_by[AST_MAX_EXTENSION];	/*!< Place to store REFERRED-BY extension */
	char refer_contact[AST_MAX_EXTENSION];	/*!< Place to store Contact info from a REFER extension */
	struct sip_pvt *refer_call;		/*!< Call we are referring */
	struct sip_route *route;		/*!< Head of linked list of routing steps (fm Record-Route) */
	int route_persistant;			/*!< Is this the "real" route? */
	char from[256];				/*!< The From: header */
	char useragent[256];			/*!< User agent in SIP request */
	char context[AST_MAX_CONTEXT];		/*!< Context for this call */
	char subscribecontext[AST_MAX_CONTEXT];	/*!< Subscribecontext */
	char fromdomain[MAXHOSTNAMELEN];	/*!< Domain to show in the from field */
	char fromuser[AST_MAX_EXTENSION];	/*!< User to show in the user field */
	int displayinfo; 			/*!< Should we send Display Info in request header? */
	char fromname[AST_MAX_EXTENSION];	/*!< Name to show in the user field */
	char fromuri[MAX_LEN];
	char organization[MAX_LEN];		/*!< From: organization name */	
	char todomain[MAXHOSTNAMELEN];	/*!< Domain to show in the to field */
	char tohost[MAXHOSTNAMELEN];		/*!< Host we should put in the "to" field */
	int toport;				/*!< Port we should put in the "to" field */
	char language[MAX_LANGUAGE];		/*!< Default language for this call */
	char musicclass[MAX_MUSICCLASS];	/*!< Music on Hold class */
	char rdnis[256];			/*!< Referring DNIS */
	char theirtag[256];			/*!< Their tag */
	char username[256];			/*!< [user] name */
	char peername[256];			/*!< [peer] name, not set if [user] */
	char regname[80];		        /*!<  name of registry entry
									  last updated the peer in
									  Backup/Proxy config*/
	char authname[256];			/*!< Who we use for authentication */
	char uri[256];				/*!< Original requested URI */
	char okcontacturi[256];			/*!< URI from the 200 OK on INVITE */
	char peersecret[256];			/*!< Password */
	char peermd5secret[256];
	struct sip_auth *peerauth;		/*!< Realm authentication */
	char cid_num[256];			/*!< Caller*ID */
	char cid_name[256];			/*!< Caller*ID */
	char via[256];				/*!< Via: header */
	char fullcontact[128];			/*!< The Contact: that the UA registers with us */
	char accountcode[AST_MAX_ACCOUNT_CODE];	/*!< Account code */
	char our_contact[256];			/*!< Our contact header */
	char *rpid;				/*!< Our RPID header */
	char *rpid_from;			/*!< Our RPID From header */
	char realm[MAXHOSTNAMELEN];		/*!< Authorization realm */
	char nonce[256];			/*!< Authorization nonce */
	int noncecount;				/*!< Nonce-count */
	char opaque[256];			/*!< Opaque nonsense */
	char qop[80];				/*!< Quality of Protection, since SIP wasn't complicated enough yet. */
	char domain[MAXHOSTNAMELEN];		/*!< Authorization domain */
	char lastmsg[256];			/*!< Last Message sent/received */
	int amaflags;				/*!< AMA Flags */
	int pendinginvite;			/*!< Any pending invite */
	int glareinvite;			/*!< A invite received while a pending invite is already present is stored here.  Its seqno is the
						value. Since this glare invite's seqno is not the same as the pending invite's, it must be 
						held in order to properly process acknowledgements for our 491 response. */

#ifdef OSP_SUPPORT
	int osphandle;				/*!< OSP Handle for call */
	time_t ospstart;			/*!< OSP Start time */
	unsigned int osptimelimit;		/*!< OSP call duration limit */
#endif
	struct sip_request initreq;		/*!< Initial request */
	
	int maxtime;				/*!< Max time for first response */
	int initid;				/*!< Auto-congest ID if appropriate */
	int waitid;       			/*!< Wait ID for scheduler after 491 or other delays */
	int autokillid;				/*!< Auto-kill ID */
	time_t lastrtprx;			/*!< Last RTP received */
	time_t lastrtptx;			/*!< Last RTP sent */
	int rtptimeout;				/*!< RTP timeout time */
	int rtpholdtimeout;			/*!< RTP timeout when on hold */
	int rtpkeepalive;			/*!< Send RTP packets for keepalive */
	enum subscriptiontype subscribed;	/*!< Is this call a subscription?  */
	int stateid;
	int laststate;                          /*!< Last known extension state */
	int dialogver;
	int faxtxcodecs;
	int modemtxcodecs;
	faxmethod_t faxmethod;
	
	struct ast_dsp *vad;			/*!< Voice Activation Detection dsp */
	int DTMFschedid;				/*!< Scheduler for SIP-INFO DTMF */
	int curDTMF;				/*!< DTMF tone being generated to Asterisk side */
	
	struct sip_peer *peerpoke;		/*!< If this calls is to poke a peer, which one */
	struct sip_registry *registry;		/*!< If this is a REGISTER call, to which registry */
	struct sip_subscription *subscription;	/*!< If this is a SUBSCRIBE call, to which subscription */	
	struct ast_rtp *rtp;			/*!< RTP Session */
	struct ast_rtp *vrtp;			/*!< Video RTP session */
#if defined(T38_SUPPORT)
	struct ast_udptl *udptl;		/*!< T.38 UDPTL session */
#endif
	struct sip_pkt *packets;		/*!< Packets scheduled for re-transmission */
	struct sip_history *history;		/*!< History of this SIP dialog */
	struct ast_variable *chanvars;		/*!< Channel variables to set for call */
	struct sip_pvt *next;			/*!< Next call in chain */
	struct sip_invite_param *options;	/*!< Options for INVITE */
	struct ast_flags flags_page2;		/*!< SIP_PAGE2 flags */
	ast_codec_quality quality_meter;        /*!< What's the quality of the call */
        struct sip_peer_quality *peer_quality;  /*!< Pointer to peer quality struct */
	int is_cli_hangup;
	int prack_level;			/*!< Should we use reliable provisional responses? */
	unsigned int prack_rseq;		/*!< The value for the outgoing RSeq header */
	unsigned int prack_rack;		/*!< The value for the outgoing RAck header */
	unsigned int prack_expected_rseq;	/*!< Expected value for an incoming RSeq header */
	unsigned int prack_expected_rack;	/*!< Expected value for an incoming RSeq header */
	char prack_cseq[80];			/*!< The CSeq value of the last PRACKed message */
	prack_status_t prack_status;		/*!< Is there a packet sent with 100rel (so don't send another one till we get PRACK) */
	int offer_m_order[SIP_MAX_OFFER_MEDIA];	/*!< order of media streams in SDP offer (we
									 			suuport only 3 types now, audio, video and
											    T38 */
	int last_offer_m_order[SIP_MAX_OFFER_MEDIA]; /*!< cache of last SDP media types we
												   sent. */
	int ptime;					/*!< Owner channel's ptime, saved in case of
								  SDP processing without an owner */
	struct sip_st_dlg *stimer;		/*!< SIP Session-Timers */              
	int instant_dial_added;           /*!< Was destination saved into instant
									  dial list during this call */
	int is_sdp_disabled;			/*!< SDP exist */
	int clir;						/*!< is caller id restricted */
	int q850_hangupcause;           /*!< Hangup cause according to Q850 / Q931 */
	int sip_hangupcause;            /*!< Hangup cause according to SIP */
	char sip_hanguptext[256];       /*!< Textual representation of
									  sip_hangupcause */
	unsigned int allowed_methods;	/*! bitmap for allowed sip methods */
	struct sip_request *transparent_response;
} *iflist = NULL;

#define FLAG_RESPONSE (1 << 0)
#define FLAG_FATAL (1 << 1)

/*! \brief sip packet - read in sipsock_read, transmitted in send_request */
struct sip_pkt {
	struct sip_pkt *next;			/*!< Next packet */
	int retrans;				/*!< Retransmission number */
	int method;				/*!< SIP method for this packet */
	int seqno;				/*!< Sequence number */
	unsigned int flags;			/*!< non-zero if this is a response packet (e.g. 200 OK) */
	struct sip_pvt *owner;			/*!< Owner call */
	int retransid;				/*!< Retransmission ID */
	int timer_a;				/*!< SIP timer A, retransmission timer */
	int timer_t1;				/*!< SIP Timer T1, estimated RTT or 500 ms */
	int packetlen;				/*!< Length of packet */
	char data[0];
};	

/*! \brief Structure for SIP user data. User's place calls to us */
struct sip_user {
	/* Users who can access various contexts */
	ASTOBJ_COMPONENTS(struct sip_user);
	char secret[80];		/*!< Password */
	char md5secret[80];		/*!< Password in md5 */
	char context[AST_MAX_CONTEXT];	/*!< Default context for incoming calls */
	char subscribecontext[AST_MAX_CONTEXT];	/* Default context for subscriptions */
	char username[80];
	char cid_num[80];		/*!< Caller ID num */
	char cid_name[80];		/*!< Caller ID name */
	char accountcode[AST_MAX_ACCOUNT_CODE];	/* Account code */
	char language[MAX_LANGUAGE];	/*!< Default language for this user */
	char musicclass[MAX_MUSICCLASS];/*!< Music on Hold class */
	char useragent[256];		/*!< User agent in SIP request */
	struct ast_codec_pref prefs;	/*!< codec prefs */
	ast_group_t callgroup;		/*!< Call group */
	ast_group_t pickupgroup;	/*!< Pickup Group */
	unsigned int flags;		/*!< SIP flags */	
	unsigned int sipoptions;	/*!< Supported SIP options */
	struct ast_flags flags_page2;	/*!< SIP_PAGE2 flags */
	int amaflags;			/*!< AMA flags for billing */
	int callingpres;		/*!< Calling id presentation */
	int capability;			/*!< Codec capability */
	int inUse;			/*!< Number of calls in use */
	int call_limit;			/*!< Limit of concurrent calls */
	int callwaiting;
	struct ast_ha *ha;		/*!< ACL setting */
	struct ast_variable *chanvars;	/*!< Variables to set for channel created by user */
	struct sip_st_cfg stimer;	/*!< SIP Session-Timers */
};

/* Structure for SIP peer data, we place calls to peers if registered  or fixed IP address (host) */
struct sip_peer {
	ASTOBJ_COMPONENTS(struct sip_peer);	/*!< name, refcount, objflags,  object pointers */
					/*!< peer->name is the unique name of this object */
	char secret[80];		/*!< Password */
	char md5secret[80];		/*!< Password in MD5 */
	struct sip_auth *auth;		/*!< Realm authentication list */
	char context[AST_MAX_CONTEXT];	/*!< Default context for incoming calls */
	char subscribecontext[AST_MAX_CONTEXT];	/*!< Default context for subscriptions */
	char username[80];		/*!< Temporary username until registration */ 
	char accountcode[AST_MAX_ACCOUNT_CODE];	/*!< Account code */
	int amaflags;			/*!< AMA Flags (for billing) */
	char todomain[MAXHOSTNAMELEN];	/*!< Domain to show in the to field */
	char tohost[MAXHOSTNAMELEN];	/*!< If not dynamic, IP address */
	int toport;			/*!< If not dynamic, Port */
	char regexten[AST_MAX_EXTENSION]; /*!< Extension to register (if regcontext is used) */
	char fromuser[80];		/*!< From: user when calling this peer */
	char fromdomain[MAXHOSTNAMELEN];	/*!< From: domain when calling this peer */
	int displayinfo; /*!< Should we send Display Info in request header? */
	char fromname[AST_MAX_EXTENSION];	/*!< From: name when calling this peer */
	char fromuri[MAX_LEN];
	char organization[MAX_LEN];		/*!< From: organization name */
	char fullcontact[256];		/*!< Contact registered with us (not in sip.conf) */
	char cid_num[80];		/*!< Caller ID num */
	char cid_name[80];		/*!< Caller ID name */
	int callingpres;		/*!< Calling id presentation */
	int inUse;			/*!< Number of calls in use */
	int call_limit;			/*!< Limit of concurrent calls */
	int callwaiting;
	char vmexten[AST_MAX_EXTENSION]; /*!< Dialplan extension for MWI notify message*/
	char mailbox[AST_MAX_EXTENSION]; /*!< Mailbox setting for MWI checks */
	char remotemailbox[AST_MAX_EXTENSION]; /*!< Mailbox setting for MWI checks against SIP server */
	char language[MAX_LANGUAGE];	/*!<  Default language for prompts */
	char musicclass[MAX_MUSICCLASS];/*!<  Music on Hold class */
	char useragent[256];		/*!<  User agent in SIP request (saved from registration) */
	struct ast_codec_pref prefs;	/*!<  codec prefs */
	int lastmsgssent;
	time_t	lastmsgcheck;		/*!<  Last time we checked for MWI */
	unsigned int flags;		/*!<  SIP flags */	
	unsigned int sipoptions;	/*!<  Supported SIP options */
	struct ast_flags flags_page2;	/*!<  SIP_PAGE2 flags */
	int expire;			/*!<  When to expire this peer registration */
	int capability;			/*!<  Codec capability */
	int rtptimeout;			/*!<  RTP timeout */
	int rtpholdtimeout;		/*!<  RTP Hold Timeout */
	int rtpkeepalive;		/*!<  Send RTP packets for keepalive */
	ast_group_t callgroup;		/*!<  Call group */
	ast_group_t pickupgroup;	/*!<  Pickup group */
	struct ast_dnsmgr_entry *dnsmgr;/*!<  DNS refresh manager for peer */
	struct sockaddr_in addr;	/*!<  IP address of peer */
	int faxtxcodecs;
	int modemtxcodecs;
	faxmethod_t faxmethod;
	char regname[80];		/*!<  name of registry entry last
							  updated the peer in Backup/Proxy
							  config*/
	/* Qualification */
	struct sip_pvt *call;		/*!<  Call pointer */
	int pokeexpire;			/*!<  When to expire poke (qualify= checking) */
	int lastms;			/*!<  How long last response took (in ms), or -1 for no response */
	int maxms;			/*!<  Max ms we will accept for the host to be up, 0 to not monitor */
	struct timeval ps;		/*!<  Ping send time */
	
	struct sockaddr_in defaddr;	/*!<  Default IP address, used until registration */
	struct ast_ha *ha;		/*!<  Access control list */
	struct ast_variable *chanvars;	/*!<  Variables to set for channel created by user */
	int lastmsg;
	int prack_level;		/*!< Should we use reliable provisional responses? */
	int rtcp_interval;      /*!< rtcp interval in ms */
	struct sip_st_cfg stimer;	/*!<  SIP Session-Timers */
	rtp_stats_t rtp_stats;          /*!< Statistics */
	int clir;					/*!< is caller id restricted */
	struct sip_registry *registry;	/*!< Related registry, used for srv failover */
	char obproxy[MAXHOSTNAMELEN];	/*!< Domain or host we register through */
	int total_down_time;
	int last_down_timestamp;
	sip_bw_mgt_t *bw_mgt;
};

AST_MUTEX_DEFINE_STATIC(sip_reload_lock);
static int sip_reloading = 0;

/* States for outbound registrations (with register= lines in sip.conf */
#define REG_STATE_UNREGISTERED		0
#define REG_STATE_REGSENT		1
#define REG_STATE_AUTHSENT		2
#define REG_STATE_REGISTERED   		3
#define REG_STATE_REJECTED	   	4
#define REG_STATE_TIMEOUT	   	5
#define REG_STATE_NOAUTH	   	6
#define REG_STATE_FAILED		7


/* States for outbound subscriptions (with subscribe= lines in sip.conf */
#define SUB_STATE_UNSUBSCRIBED		0
#define SUB_STATE_SUBSENT		1
#define SUB_STATE_AUTHSENT		2
#define SUB_STATE_SUBSCRIBED   		3
#define SUB_STATE_REJECTED	   	4
#define SUB_STATE_TIMEOUT	   	5
#define SUB_STATE_NOAUTH	   	6
#define SUB_STATE_FAILED		7

/* States for fetch binding procedure */
typedef enum {
	FETCH_BINDING_STATE_UNBOUND = 0,
	FETCH_BINDING_STATE_QUERY = 1,
	FETCH_BINDING_STATE_UNREGISTER = 2,
} fetch_bindings_state_t;

/* sip_subscription: Subscribe with other SIP notifiers */
struct sip_subscription {
	ASTOBJ_COMPONENTS_FULL(struct sip_subscription,1,1);
	int portno;			/* Optional port override */
	int obproxyport;		/*!< Optional outbound proxy port */
	char username[80];		/* Who we are subscribing as */
	char hostname[MAXHOSTNAMELEN];	/* Domain or host we subscribe to */
	char obproxy[MAXHOSTNAMELEN];	/*!< Domain or host we register through */
	char authuser[80];		/* Who we *authenticate* as */
	char secret[80];		/* Password or key name in []'s */	
	char contact[256];		/* Contact extension */
	char callid[80];		/* Global CallID for this subscription */
	char md5secret[80];
 	char nonce[256];		/* Authorization nonce */
 	char realm[MAXHOSTNAMELEN];	/* Authorization realm XXX check if need to change, for now it's the 
					 * same as in registration*/
 	char domain[MAXHOSTNAMELEN];	/* Authorization domain */
 	char subdomain[MAXHOSTNAMELEN];	/* Subscription domain */
 	char opaque[256];		/* Opaque nonsense */
 	char qop[80];			/* Quality of Protection. */
	int refresh;			/* How often to refresh */
	int timeout; 			/* sched id of sip_sub_timeout */
	int expire;			/* Sched ID of expiration */
	int has_message_waiting;        /* Do we have a message waiting for this username */
	int callid_valid;		/* 0 means we haven't chosen callid for this subscription yet. */
	int substate;			/* Subscription state (see above) */
	int subattempts;		/* Number of attempts */
	unsigned int ocseq;		/* Sequence number we got to for SUBSCRIBEs for this subscription */
	struct sip_pvt *call;		/* create a sip_pvt structure for each outbound "subscription call" in progress */
};

/*! \brief sip_uas: Responsible for address resolving of UAS */
struct sip_uas {
	char hostname[MAXHOSTNAMELEN]; 	/*!< Domain or host we register to */
	struct sockaddr_in addr;	 /*!< Current IP and port of proxy */
	struct srv_context *context; 	/*!< List of srv records */
	int healthy; 	/*!< Register succeeded using this proxy */
};
    	
/*! \brief sip_registry: Registrations with other SIP proxies */
struct sip_registry {
	ASTOBJ_COMPONENTS(struct sip_registry);
	int portno;			/*!<  Optional port override */
	int obproxyport;		/*!< Optional outbound proxy port */
	char username[80];		/*!<  Who we are registering as */
	char authuser[80];		/*!< Who we *authenticate* as */
	struct sip_uas uas_srv; 	/*!< Which UAS are we register to */
	int srv_failover;		/*!< srv failover is enabled and obproxy was found */
	char hostname[MAXHOSTNAMELEN];	/*!< Domain or host we register to */
	char obproxy[MAXHOSTNAMELEN];	/*!< Domain or host we register through */
	char secret[80];		/*!< Password in clear text */	
	char md5secret[80];		/*!< Password in md5 */
	char contact[256];		/*!< Contact extension */
	char random[80];
	int expire;			/*!< Sched ID of expiration */
	int regattempts;		/*!< Number of attempts (since the last success) */
	int timeout; 			/*!< sched id of sip_reg_timeout */
	int refresh;			/*!< How often to refresh */
	int expiry;
	int reg_period;
	struct sip_pvt *call;		/*!< create a sip_pvt structure for each outbound "registration call" in progress */
	int regstate;			/*!< Registration state (see above) */
	int callid_valid;		/*!< 0 means we haven't chosen callid for this registry yet. */
	char callid[80];		/*!< Global CallID for this registry */
	unsigned int ocseq;		/*!< Sequence number we got to for REGISTERs for this registry */
	struct sockaddr_in us;		/*!< Who the server thinks we are */
 	
 					/* Saved headers */
 	char realm[MAXHOSTNAMELEN];	/*!< Authorization realm */
 	char nonce[256];		/*!< Authorization nonce */
	char nextnonce[256];		/*! Authorization nonce that should be used in
									authorization header for the next request */
 	char domain[MAXHOSTNAMELEN];	/*!< Authorization domain */
 	char regdomain[MAXHOSTNAMELEN];	/*!< Authorization domain */
 	char opaque[256];		/*!< Opaque nonsense */
 	char qop[80];			/*!< Quality of Protection. */
	int noncecount;			/*!< Nonce-count */
 
 	char lastmsg[256];		/*!< Last Message sent/received */
	int  backup;                    /* is this a backup proxy? */
	int  is_backup_active;          /*!<indicate whether a registry is
									  active in a failover mode */
	int  need_recover_to_primary;    /* indicate if need to recover to primary
										proxy */
	struct sip_registry *reg_primary;/*!<For backup registry, points to
									   primary one  */
	struct sip_registry *reg_backup; /*!<For primary registry, points to
									   backup one. */
	int  sched_recover_primary;     /*!<Sched ID of timer to try to return
									  to primary proxy. */
	int  sched_failover_delay;      /*!<Sched ID of timer to backoff
									  if timeout on backup proxy. */

	int server_na; /*!<Server is not available. Used for Vod DE diagnostics. */
	fetch_bindings_state_t fetch_state; /* Fetch Bindings procedure state */
	contact_list_t *contact_list;
	int ip_changed;
	int got_response_from_server;
};

/*! \brief  The user list: Users and friends ---*/
static struct ast_user_list {
	ASTOBJ_CONTAINER_COMPONENTS(struct sip_user);
} userl;

/*! \brief  The peer list: Peers and Friends ---*/
static struct ast_peer_list {
	ASTOBJ_CONTAINER_COMPONENTS(struct sip_peer);
} peerl;

/*! \brief  The register list: Other SIP proxys we register with and call ---*/
static struct ast_register_list {
	ASTOBJ_CONTAINER_COMPONENTS(struct sip_registry);
	int recheck;
} regl;

/*--- The subscribe list: Other SIP notifiers we subscribe with and call ---*/
static struct ast_subscription_list {
	ASTOBJ_CONTAINER_COMPONENTS(struct sip_subscription);
	int recheck;
} subl;

static int __sip_do_register(struct sip_registry *r, int unregister);
static int __sip_do_subscribe(struct sip_subscription *r);

static int sipsock  = -1;


static struct sockaddr_in bindaddr = { 0, };
static struct sockaddr_in externip;
static char externhost[MAXHOSTNAMELEN] = "";
static time_t externexpire = 0;
static int externrefresh = 10;
static struct ast_ha *localaddr;
static struct ast_ha *register_whitelist;

/* The list of manual NOTIFY types we know how to send */
struct ast_config *notify_types;

static struct sip_auth *authl;          /*!< Authentication list */

/* used for diagnostics */
static long last_answered_call_timestamp = 0;

static int transmit_response(struct sip_pvt *p, char *msg, struct sip_request *req);
static int transmit_response_with_sdp(struct sip_pvt *p, char *msg, struct sip_request *req, int retrans, int oldsdp);
#if defined(T38_SUPPORT)
static int transmit_response_with_t38_sdp(struct sip_pvt *p, char *msg, struct sip_request *req, int retrans, int oldsdp);
#endif
static int transmit_response_with_unsupported(struct sip_pvt *p, char *msg, struct sip_request *req, char *unsupported);
static int transmit_response_with_auth(struct sip_pvt *p, char *msg, struct sip_request *req, char *rand, int reliable, char *header, int stale);
static int transmit_request(struct sip_pvt *p, int sipmethod, int inc, int reliable, int newbranch);
static int transmit_request_with_auth(struct sip_pvt *p, int sipmethod, int inc, int reliable, int newbranch);
static int transmit_invite(struct sip_pvt *p, int sipmethod, int sendsdp, int init);
static int transmit_reinvite(struct sip_pvt *p, int oldsdp, int t38, int sipmethod);
static int transmit_reinvite_with_sdp(struct sip_pvt *p, int oldsdp);
#if defined(T38_SUPPORT)
static int transmit_reinvite_with_t38_sdp(struct sip_pvt *p, int oldsdp);
#endif
static int transmit_info_with_digit(struct sip_pvt *p, char digit);
static int transmit_info_with_broadsoft_flash(struct sip_pvt *p);
static int transmit_info_with_vidupdate(struct sip_pvt *p);
static int transmit_message_with_text(struct sip_pvt *p, const char *text);
static int transmit_refer(struct sip_pvt *p, struct sip_pvt *target, const char *dest);
static int transmit_prack(struct sip_pvt *p, int sdp, struct sip_request *resp);
static int sip_sipredirect(struct sip_pvt *p, const char *dest);
static struct sip_peer *temp_peer(const char *name);
static int do_proxy_auth(struct sip_pvt *p, struct sip_request *req, char *header, char *respheader, int sipmethod, int init);
static void free_old_route(struct sip_route *route);
static int build_reply_digest(struct sip_pvt *p, int method, char *digest, int digest_len);
static int update_call_counter(struct sip_pvt *fup, int event);
static struct sip_peer *build_peer(const char *name, struct ast_variable *v, int realtime, int reload);
static struct sip_registry *get_registry_for_sip(struct sip_pvt *sip);
static struct sip_registry *find_registry(const char* username, const char* obproxy);
static struct sip_registry *find_registry_by_addr(struct sockaddr_in *sin);
static void sip_registry_destroy(struct sip_registry *reg);
static struct sip_user *build_user(const char *name, struct ast_variable *v, int realtime);
static int sip_do_reload(void);
static int expire_register(void *data);
static int sip_addheader(struct ast_channel *chan, void *data);
static int callevents = 0;

static struct ast_channel *sip_request_call(const char *type, const struct ast_codec_pref *format, void *data, int *cause);
static int sip_devicestate(void *data);
static int sip_sendtext(struct ast_channel *ast, const char *text);
static int sip_call(struct ast_channel *ast, char *dest, int timeout);
static int sip_hangup(struct ast_channel *ast);
static int sip_answer(struct ast_channel *ast);
static struct ast_frame *sip_read(struct ast_channel *ast);
static int sip_write(struct ast_channel *ast, struct ast_frame *frame);
static int sip_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen);
static int sip_transfer(struct ast_channel *ast, const char *dest);
static int sip_attendedtransfer(struct ast_channel *ast, struct ast_channel *target);
static int sip_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static struct ast_rtp *sip_get_rtp(struct ast_channel *ast);
static struct ast_udptl *sip_get_udptl(struct ast_channel *ast);
static int sip_senddigit_begin(struct ast_channel *ast, char digit);
static int sip_senddigit_end(struct ast_channel *ast, char digit);
static char *sip_get_callid(struct ast_channel *chan);
static int clear_realm_authentication(struct sip_auth *authlist);                            /* Clear realm authentication list (at reload) */
static struct sip_auth *add_realm_authentication(struct sip_auth *authlist, char *configuration, int lineno);   /* Add realm authentication in list */
static struct sip_auth *find_realm_authentication(struct sip_auth *authlist, char *realm);         /* Find authentication for a specific realm */
static void parse_request(struct sip_request *req);
static char *get_header(struct sip_request *req, char *name);
static void copy_request(struct sip_request *dst,struct sip_request *src);
static int check_sip_domain(const char *domain, char *context, size_t len); /* Check if domain is one of our local domains */
static void append_date(struct sip_request *req);	/* Append date to SIP packet */
static int determine_firstline_parts(struct sip_request *req);
static void sip_fixup_codecs(struct ast_channel *chan, const struct ast_codec_pref *peer_codecs);
static void sip_dump_history(struct sip_pvt *dialog);	/* Dump history to LOG_DEBUG at end of dialog, before destroying data */
static const struct cfsubscription_types *find_subscription_type(enum subscriptiontype subtype);
static int transmit_state_notify(struct sip_pvt *p, int state, int full, int substate);
static char *gettag(struct sip_request *req, char *header, char *tagbuf, int tagbufsize);
static void sip_pre_bridge(struct ast_channel *chan);
static int add_header(struct sip_request *req, const char *var, const char *value);
static int transmit_response_with_warning(struct sip_pvt *p, char *msg, struct
		sip_request *req, char *warn_text);
static struct sip_peer *find_peer(const char *peer, struct sockaddr_in *sin, int realtime);
static struct sip_pvt *sip_alloc(char *callid, struct sockaddr_in *sin, int useglobal_nat, const int intended_method);
static int create_addr_from_peer(struct sip_pvt *r, struct sip_peer *peer);
static int transmit_message_with_content(struct sip_pvt *p, const char *type, const char *buf, int len, int in_dialog);
static void sip_destroy_peer(struct sip_peer *peer);
static void sip_destroy_user(struct sip_user *user);
struct in_addr sip_get_ourip(struct ast_channel *chan);
static void add_hangup_reason_header(struct sip_request *req, struct sip_pvt *p);

#if defined(T38_SUPPORT)
static int sip_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo,struct ast_channel **rc, int timeoutms); /* Function to bridge to SIP channels if NOT T.38 enabled */
static int sip_handle_t38_reinvite(struct ast_channel *chan, struct sip_pvt *pvt, int reinvite); /* T38 negotiation helper function */
#endif

/*------ Session-Timers functions --------- */
static void proc_422_rsp(struct sip_pvt *p, struct sip_request *rsp);
static int  proc_session_timer(void *vp);
static void stop_session_timer(struct sip_pvt *p);
static void start_session_timer(struct sip_pvt *p);
static void restart_session_timer(struct sip_pvt *p);
static const char *strefresher2str(enum st_refresher r);
static const char *strefresher2header(enum st_refresher r, int uas);
static int parse_session_expires(const char *p_hdrval, int *const p_interval, enum st_refresher *const p_ref, int uas);
static int parse_minse(char *p_hdrval, int *const p_interval);
static int st_get_se(struct sip_pvt *, int max);
static enum st_refresher st_get_refresher(struct sip_pvt *);
static enum st_mode st_get_mode(struct sip_pvt *, int no_cached);
static struct sip_st_dlg* sip_st_alloc(struct sip_pvt *const p);
static void manager_event_sip_registry(void);
static int check_supported_required(struct sip_pvt *p, struct sip_request *req,
	char *unsupported, size_t unsupported_len);

/*! \brief Definition of this channel for PBX channel registration */
static const struct ast_channel_tech sip_tech = {
	.type = channeltype,
	.description = "Session Initiation Protocol (SIP)",
	.capabilities = ((AST_FORMAT_MAX_VIDEO << 1) - 1),
	.properties = AST_CHAN_TP_WANTSJITTER,
	.requester = sip_request_call,
	.devicestate = sip_devicestate,
	.call = sip_call,
	.hangup = sip_hangup,
	.answer = sip_answer,
	.read = sip_read,
	.write = sip_write,
	.write_video = sip_write,
	.indicate = sip_indicate,
	.transfer = sip_transfer,
	.attendedtransfer = sip_attendedtransfer,
	.fixup = sip_fixup,
	.send_digit_begin = sip_senddigit_begin,
	.send_digit_end = sip_senddigit_end,
#if defined(T38_SUPPORT)
	.bridge = sip_bridge,
	.get_udptl = sip_get_udptl,
#else
 	.bridge = ast_rtp_bridge,
#endif
	.send_text = sip_sendtext,
	.fixup_codecs = sip_fixup_codecs,
	.get_rtp = sip_get_rtp,
	.pre_bridge = sip_pre_bridge,
	.get_pvt_uniqueid = sip_get_callid,
        .get_ourip = sip_get_ourip,
};

#define BLOCKED_ADDR_MSG_SUPPRESS_TIME_SEC 5
#define BLOCKED_ADDR_HASH_SIZE 30

static time_t sip_blocked_addr_hash[BLOCKED_ADDR_HASH_SIZE];

static void bw_mgt_free_bw(struct sip_pvt *pvt, struct sip_peer *peer)
{
	if (peer->bw_mgt)
	{
		ast_log(LOG_DEBUG, "[%s:%s] used bw=%lu->%lu/%lu\n",
			pvt->callid, pvt->peername,
			peer->bw_mgt->used_upstream,
			peer->bw_mgt->used_upstream - pvt->used_upstream,
			peer->bw_mgt->cfg_upstream);
		peer->bw_mgt->used_upstream -= pvt->used_upstream;
	}
	pvt->used_upstream = 0;
}

static void bw_mgt_alloc_bw(struct sip_pvt *pvt, struct sip_peer *peer, long bw)
{
	bw_mgt_free_bw(pvt, peer);

	pvt->used_upstream = bw;

	if (!peer->bw_mgt)
		return;

	ast_log(LOG_DEBUG, "[%s:%s] used bw=%lu->%lu/%lu\n",
		pvt->callid, pvt->peername,
		peer->bw_mgt->used_upstream, peer->bw_mgt->used_upstream + bw,
		peer->bw_mgt->cfg_upstream);

	peer->bw_mgt->used_upstream += bw;
}

/* Returns the minimal/maximal bandwidth that may be used by a codec in this
 * mask */
static int get_minmax_audio_bw(int mask, int is_max)
{
	int bit;
	long bit_bw, best_bw = 0;

	ast_log(LOG_DEBUG, "minmax audio. mask %d, is_max %d\n", mask, is_max);

	for (bit = 1 << 0; bit != AST_FORMAT_MAX_AUDIO; bit <<= 1)
	{
		if (!(mask & bit))
			continue;

		bit_bw = ast_rtp_get_codec_bw(bit);

		if (!best_bw || (is_max && bit_bw > best_bw) ||
			(!is_max && bit_bw < best_bw))
		{
			best_bw = bit_bw;
		}

		ast_log(LOG_DEBUG, "minmax best_bw %ld\n", best_bw);
	}
	return best_bw;
}

/* Returns the codecs that need the less bandwidth than the current */
static int get_economic_audio_codecs(int mask, int current_bw)
{
	int bit;
	int codecs = 0;

	for (bit = 1 << 0; bit != AST_FORMAT_MAX_AUDIO; bit <<= 1)
	{
		if (!(mask & bit))
			continue;

		if (ast_rtp_get_codec_bw(bit) < current_bw)
			codecs |= bit;
	}
	return codecs;
}

/* Returns the capability mask of the codecs that fits in "bandwidth" */
static int get_codecs_by_bandwidth(int capability, long bandwidth)
{
	int x, codecs = 0;

	for (x = 1 << 0; x < AST_FORMAT_MAX_VIDEO; x <<= 1)
	{
		if (!(x & capability))
			continue;

		if (x <= AST_FORMAT_MAX_AUDIO && ast_rtp_get_codec_bw(x) <= bandwidth)
			codecs |= x;

		/* XXX need to handle video correctly */
		if (x > AST_FORMAT_MAX_AUDIO)
			codecs |= x;

	}

	return codecs;
}

/* try to find some codec that can fit in the available bandwidth,
 * if no codec is found - try to release bandwidth by re-inviting existing
 * calls */
static int find_fitting_codecs(struct sip_peer *peer, struct sip_pvt *new_call)
{
	int codecs;

	if (!peer->bw_mgt)
		return new_call->usercapability;

	/* We assume that iflock is locked, since in chan_sip the order of locks
	 * is lock(iflock), lock(pvt), unlock(pvt), unlock(iflock) */
	while (!(codecs = get_codecs_by_bandwidth(new_call->usercapability,
		peer->bw_mgt->cfg_upstream - peer->bw_mgt->used_upstream)))
	{
		struct sip_pvt *pvt;
		struct sip_peer *pvt_peer;
		struct sip_pvt *best = NULL;
		long best_bw = 0, min_bw;
		int fmt;
		sip_bw_mgt_t *bw_mgt;

		/* Look for another call that can be used to free bandwidth */
		for (pvt = iflist; pvt; pvt = pvt->next)
		{
			ast_mutex_lock(&pvt->lock);
			pvt_peer = find_peer(pvt->peername, NULL, 1);
			bw_mgt = pvt_peer->bw_mgt;
			ASTOBJ_UNREF(pvt_peer, sip_destroy_peer);

			/* skip call if:
			 * - it is the current (newly-created) call
			 * - it has pending invite
			 * - it has no owner
			 * - it is not established
			 * - reducing its bandwidth usage will not help us
			 * - it is fax T38 call
			 * TODO - skip emergency and fax passthrough calls
			 */
			if (pvt == new_call || pvt->pendinginvite || !pvt->owner ||
				pvt->owner->_state != AST_STATE_UP || bw_mgt != peer->bw_mgt
#if defined(T38_SUPPORT)
				|| pvt->t38state != T38_DISABLED
#endif
				)
			{
				ast_mutex_unlock(&pvt->lock);
				continue;
			}

			/* the best call is the one that uses the most bandwidth and its
			 * bandwidth usage can be reduced */
			min_bw = get_minmax_audio_bw(pvt->usercapability, 0);
			if (pvt->used_upstream > best_bw && pvt->used_upstream > min_bw)
			{
				if (best)
					ast_mutex_unlock(&best->lock);
				best = pvt;
				best_bw = best->used_upstream;
			}
			else
				ast_mutex_unlock(&pvt->lock);
		}

		if (!best)
		{
			ast_log(LOG_NOTICE, "No victim for reinvite found\n");
			break;
		}

		/* re-negotiate codec for best call */
		/* TODO XXX for now just assumes re-negotation will be successful, and
		 * the bandwidth can be used immediately */
		ast_log(LOG_DEBUG, "found victim for reinvite %s:%s\n",
			best->callid, best->peername);
		fmt = get_economic_audio_codecs(best->usercapability,
			best->used_upstream);
		best->usercapability = fmt;
		bw_mgt_alloc_bw(best, peer, get_minmax_audio_bw(fmt, 1));
		
		ast_codec_pref_set2(&best->formats, best->usercapability);
		transmit_reinvite_with_sdp(best, FALSE);
		ast_mutex_unlock(&best->lock);
	}

	return codecs;
}

static int contact_list_add_entry(contact_list_t **list, char *host, int port)
{
	contact_list_t *c;

	if (!(c = calloc(1, sizeof(*c))))
	{
		ast_log(LOG_ERROR, "Out of memory\n");
		return -1;
	}

	strncpy(c->host, host, sizeof(c->host));
	c->port = port;

	c->next = *list;
	*list = c;
	return 0;
}

static void contact_list_free(contact_list_t **list)
{
	contact_list_t *tmp;

	while ((tmp = *list))
	{
		*list = (*list)->next;
		free(tmp);
	}
}

static int is_binary_content_type(char *content_type)
{
	int i;

	if (!content_type)
		return 0;

	for(i = 0; binary_content_types[i]; i++)
	{
		if (!strcmp(content_type, binary_content_types[i]))
			return 1;
	}

	return 0;
}

static long get_time_from_boot(void)
{
    struct sysinfo si;

    sysinfo(&si);

    return si.uptime;
}

static void set_server_na(struct sip_registry *reg, int na)
{
	reg->got_response_from_server = !na;
	if (reg->server_na == na)
		return;

	reg->server_na = na;
	manager_event_sip_registry();
}

/*!
  \brief Thread-safe random number generator
  \return a random number

  This function uses a mutex lock to guarantee that no
  two threads will receive the same random number.
 */
static force_inline int thread_safe_rand(void)
{
	int val;

	ast_mutex_lock(&rand_lock);
	val = rand();
	ast_mutex_unlock(&rand_lock);
	
	return val;
}

/* add media type to sdp order. if the media type already exist, it does
 * nothing. else, it adds the type to the end of the order */
static void add_sdp_media_to_order(struct sip_pvt *p, int type)
{
	int i = 0;

	for (; p->offer_m_order[i] && i < SIP_MAX_OFFER_MEDIA; i++)
	{
		if (p->offer_m_order[i] == type)
			return;
	}
	p->offer_m_order[i] = type;
}

/*! \brief  find_sip_method: Find SIP method from header
 * Strictly speaking, SIP methods are case SENSITIVE, but we don't check 
 * following Jon Postel's rule: Be gentle in what you accept, strict with what you send */
int find_sip_method(char *msg)
{
	int i, res = 0;
	
	if (ast_strlen_zero(msg))
		return 0;

	for (i = 1; (i < (sizeof(sip_methods) / sizeof(sip_methods[0]))) && !res; i++) {
		if (!strcasecmp(sip_methods[i].text, msg)) 
			res = sip_methods[i].id;
	}
	return res;
}

/*!
 * \brief Parse supported header in incoming packet
 *
 * \details This function parses through the options parameters and
 * builds a bit field representing all the SIP options in that field. When an
 * item is found that is not supported, it is copied to the unsupported
 * out buffer.
 *
 * \param option list
 * \param unsupported out buffer (optional)
 * \param unsupported out buffer length (optional)
 */
unsigned int parse_sip_options(struct sip_pvt *pvt, const char *options, 
    char *unsupported, size_t unsupported_len)
{
	char *next, *sep;
	char *temp;
	int i, found, supported;
	unsigned int profile = 0;

	char *out = unsupported;
	size_t outlen = unsupported_len;
	char *cur_out = out;

	if (out && (outlen > 0)) {
		memset(out, 0, outlen);
	}

	if (ast_strlen_zero(options) )
		return 0;

	temp = ast_strdupa(options);

	ast_log(LOG_DEBUG, "Begin: parsing SIP \"Supported: %s\"\n", options);

	for (next = temp; next; next = sep) {
		found = 0;
		supported = 0;
		if ((sep = strchr(next, ',')) != NULL) {
			*sep++ = '\0';
		}

		/* trim leading and trailing whitespace */
		next = ast_strip(next);

		if (ast_strlen_zero(next)) {
			continue; /* if there is a blank argument in there just skip it */
		}

		ast_log(LOG_DEBUG, "Found SIP option: -%s-\n", next);
		for (i = 0; i < ARRAY_LEN(sip_options); i++) {
			if (!strcasecmp(next, sip_options[i].text)) {
				profile |= sip_options[i].id;
				if (sip_options[i].supported == SUPPORTED) {
					supported = 1;
				}
				found = 1;
				ast_log(LOG_DEBUG, "Matched SIP option: %s\n", next);
				break;
			}
		}

		/* If option is not supported, add to unsupported out buffer */
		if (!supported && out && outlen) {
			size_t copylen = strlen(next);
			size_t cur_outlen = strlen(out);
			/* Check to see if there is enough room to store this option.
			 * Copy length is string length plus 2 for the ',' and '\0' */
			if ((cur_outlen + copylen + 2) < outlen) {
				/* if this isn't the first item, add the ',' */
				if (cur_outlen) {
					*cur_out = ',';
					cur_out++;
					cur_outlen++;
				}
				ast_copy_string(cur_out, next, (outlen - cur_outlen));
				cur_out += copylen;
			}
		}

		if (!found) {
			profile |= SIP_OPT_UNKNOWN;
			if (!strncasecmp(next, "x-", 2))
				ast_log(LOG_DEBUG, "Found private SIP option, not supported: %s\n", next);
			else
				ast_log(LOG_DEBUG, "Found no match for SIP option: %s (Please file bug report!)\n", next);
		}
	}
	if (pvt) {
		pvt->sipoptions = profile;
		if (option_debug)
			ast_log(LOG_DEBUG, "* SIP extension value: %d for call %s\n", profile, pvt->callid);
	}
	return profile;
}

/*! \brief  sip_debug_test_addr: See if we pass debug IP filter */
static inline int sip_debug_test_addr(struct sockaddr_in *addr) 
{
	if (sipdebug == 0)
		return 0;
	if (debugaddr.sin_addr.s_addr) {
		if (((ntohs(debugaddr.sin_port) != 0)
			&& (debugaddr.sin_port != addr->sin_port))
			|| (debugaddr.sin_addr.s_addr != addr->sin_addr.s_addr))
			return 0;
	}
	return 1;
}

/*! \brief  sip_debug_test_pvt: Test PVT for debugging output */
static inline int sip_debug_test_pvt(struct sip_pvt *p) 
{
	if (sipdebug == 0)
		return 0;
	return sip_debug_test_addr(((ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE) ? &p->recv : &p->sa));
}

static void manager_event_diagnostic_trace(char *buf, int len, int is_out)
{
    char *hex = malloc(len*2+1);

	/* encode sip message to hex */
    ast_hexencode(buf, hex, len);
	/* send event */
    manager_event(EVENT_FLAG_CALL, "DiagnosticSipMessage", 
	"Direction: %s\r\n"
	"Length: %d\r\n"
	"Data: %s\r\n",
	is_out ? "Tx" : "Rx", len*2, hex);

	free(hex);
}

/*! \brief  __sip_xmit: Transmit SIP message ---*/
static int __sip_xmit(struct sip_pvt *p, char *data, int len)
{
	int res;
	char iabuf[INET_ADDRSTRLEN];

	if (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE)
		res=sendto(sipsock, data, len, 0, (struct sockaddr *)&p->recv, sizeof(struct sockaddr_in));
	else
		res=sendto(sipsock, data, len, 0, (struct sockaddr *)&p->sa, sizeof(struct sockaddr_in));

	if (res != len) {
		ast_verbose("__sip_xmit of %p (len %d) to %s:%d returned %d: %s\n", data, len, ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr), ntohs(p->sa.sin_port), res, strerror(errno));
	}
	else if (pbx_builtin_getvar_helper(p->owner, "DIAGNOSTIC_CALL"))
	    manager_event_diagnostic_trace(data, len, 1);

	return res;
}

static void sip_destroy(struct sip_pvt *p);
static void sip_peer_server_down_time_calc(struct sip_registry *r, int reset);

/*! \brief  build_via: Build a Via header for a request ---*/
static void build_via(struct sip_pvt *p, char *buf, int len)
{
	char iabuf[INET_ADDRSTRLEN];

	/* z9hG4bK is a magic cookie.  See RFC 3261 section 8.1.1.7 */
	if (ast_test_flag(p, SIP_NAT) & SIP_NAT_RFC3581)
		snprintf(buf, len, "SIP/2.0/UDP %s:%d;branch=z9hG4bK%08x;rport", ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip), ourport, p->branch);
	else /* Work around buggy UNIDEN UIP200 firmware */
		snprintf(buf, len, "SIP/2.0/UDP %s:%d;branch=z9hG4bK%08x", ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip), ourport, p->branch);
}

/*! \brief  ast_sip_ouraddrfor: NAT fix - decide which IP address to use for ASterisk server? ---*/
/* Only used for outbound registrations */
static int ast_sip_ouraddrfor(struct in_addr *them, struct in_addr *us)
{
	/*
	 * Using the localaddr structure built up with localnet statements
	 * apply it to their address to see if we need to substitute our
	 * externip or can get away with our internal bindaddr
	 */
	struct sockaddr_in theirs;
	theirs.sin_addr = *them;
	if (localaddr && externip.sin_addr.s_addr &&
	   ast_apply_ha(localaddr, &theirs)) {
		char iabuf[INET_ADDRSTRLEN];
		if (externexpire && (time(NULL) >= externexpire)) {
			struct ast_hostent ahp;
			struct hostent *hp;
			time(&externexpire);
			externexpire += externrefresh;
			if ((hp = ast_gethostbyname(externhost, &ahp))) {
				memcpy(&externip.sin_addr, hp->h_addr, sizeof(externip.sin_addr));
			} else
				ast_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", externhost);
		}
		memcpy(us, &externip.sin_addr, sizeof(struct in_addr));
		ast_inet_ntoa(iabuf, sizeof(iabuf), *(struct in_addr *)&them->s_addr);
		ast_log(LOG_DEBUG, "Target address %s is not local, substituting externip\n", iabuf);
	}
	else if (bindaddr.sin_addr.s_addr)
		memcpy(us, &bindaddr.sin_addr, sizeof(struct in_addr));
	else
		return ast_ouraddrfor(them, us);
	return 0;
}

/*! \brief  append_history: Append to SIP dialog history */
/*	Always returns 0 */
static int append_history(struct sip_pvt *p, const char *event, const char *data)
{
	struct sip_history *hist, *prev;
	char *c;

	if (!recordhistory || !p)
		return 0;
	if(!(hist = malloc(sizeof(struct sip_history)))) {
		ast_log(LOG_WARNING, "Can't allocate memory for history");
		return 0;
	}
	memset(hist, 0, sizeof(struct sip_history));
	snprintf(hist->event, sizeof(hist->event), "%-15s %s", event, data);
	/* Trim up nicely */
	c = hist->event;
	while(*c) {
		if ((*c == '\r') || (*c == '\n')) {
			*c = '\0';
			break;
		}
		c++;
	}
	/* Enqueue into history */
	prev = p->history;
	if (prev) {
		while(prev->next)
			prev = prev->next;
		prev->next = hist;
	} else {
		p->history = hist;
	}
	return 0;
}

/*! \brief  retrans_pkt: Retransmit SIP message if no answer ---*/
static int retrans_pkt(void *data)
{
	struct sip_pkt *pkt=data, *prev, *cur = NULL;
	char iabuf[INET_ADDRSTRLEN];
	int reschedule = DEFAULT_RETRANS;
	struct sip_registry *reg;

	/* Lock channel */
	ast_mutex_lock(&pkt->owner->lock);

	if (pkt->owner && pkt->method == SIP_INVITE &&
	    (reg = get_registry_for_sip(pkt->owner)))
	{			    
		set_server_na(reg, 1);
	}

	if (pkt->retrans < (pkt->method == SIP_INVITE ? MAX_INVITE_RETRANS : MAX_RETRANS)) {
		char buf[80];

		pkt->retrans++;
 		if (!pkt->timer_t1) {	/* Re-schedule using timer_a and timer_t1 */
			if (sipdebug && option_debug > 3)
 				ast_log(LOG_DEBUG, "SIP TIMER: Not rescheduling id #%d:%s (Method %d) (No timer T1)\n", pkt->retransid, sip_methods[pkt->method].text, pkt->method);
		} else {
 			int siptimer_a;

 			if (sipdebug && option_debug > 3)
 				ast_log(LOG_DEBUG, "SIP TIMER: Rescheduling retransmission #%d (%d) %s - %d\n", pkt->retransid, pkt->retrans, sip_methods[pkt->method].text, pkt->method);
 			if (!pkt->timer_a)
 				pkt->timer_a = 2 ;
 			else
 				pkt->timer_a = 2 * pkt->timer_a;
 
 			/* For non-invites, a maximum of 4 secs */
 			siptimer_a = pkt->timer_t1 * pkt->timer_a;	/* Double each time */
 			if (pkt->method != SIP_INVITE && siptimer_a > 4000)
 				siptimer_a = 4000;

			/* According the RFC3261, the final timeout on INVITE
			 * expires after 64*t1. This means that the timeout on
			 * the last retry is only 0.5s. */
			if (pkt->method == SIP_INVITE && pkt->retrans == MAX_INVITE_RETRANS)
				siptimer_a = DEFAULT_RETRANS;
 		
 			/* Reschedule re-transmit */
			reschedule = siptimer_a;
 			if (option_debug > 3)
 				ast_log(LOG_DEBUG, "** SIP timers: Rescheduling retransmission %d to %d ms (t1 %d ms (Retrans id #%d)) \n", pkt->retrans +1, siptimer_a, pkt->timer_t1, pkt->retransid);
 		} 

		if (pkt->owner && sip_debug_test_pvt(pkt->owner)) {
			if (ast_test_flag(pkt->owner, SIP_NAT) & SIP_NAT_ROUTE)
				ast_verbose("Retransmitting #%d (NAT) to %s:%d:\n%s\n---\n", pkt->retrans, ast_inet_ntoa(iabuf, sizeof(iabuf), pkt->owner->recv.sin_addr), ntohs(pkt->owner->recv.sin_port), pkt->data);
			else
				ast_verbose("Retransmitting #%d (no NAT) to %s:%d:\n%s\n---\n", pkt->retrans, ast_inet_ntoa(iabuf, sizeof(iabuf), pkt->owner->sa.sin_addr), ntohs(pkt->owner->sa.sin_port), pkt->data);
		}

		if (pkt->method == SIP_REGISTER && pkt->owner->registry)
			sip_peer_server_down_time_calc(pkt->owner->registry, 0);

		snprintf(buf, sizeof(buf), "ReTx %d", reschedule);

		append_history(pkt->owner, buf, pkt->data);
		__sip_xmit(pkt->owner, pkt->data, pkt->packetlen);
		ast_mutex_unlock(&pkt->owner->lock);
		return  reschedule;
	} 
	/* Too many retries */
	if (pkt->owner && pkt->method != SIP_OPTIONS) {
		if (ast_test_flag(pkt, FLAG_FATAL) || sipdebug)	/* Tell us if it's critical or if we're debugging */
			ast_log(LOG_WARNING, "Maximum retries exceeded on transmission %s for seqno %d (%s %s)\n", pkt->owner->callid, pkt->seqno, (ast_test_flag(pkt, FLAG_FATAL)) ? "Critical" : "Non-critical", (ast_test_flag(pkt, FLAG_RESPONSE)) ? "Response" : "Request");
	} else {
		if (pkt->method == SIP_OPTIONS && sipdebug)
			ast_log(LOG_WARNING, "Cancelling retransmit of OPTIONs (call id %s) \n", pkt->owner->callid);
	}
	append_history(pkt->owner, "MaxRetries", (ast_test_flag(pkt, FLAG_FATAL)) ? "(Critical)" : "(Non-critical)");
 		
	pkt->retransid = -1;

	if (ast_test_flag(pkt, FLAG_FATAL)) {
		while(pkt->owner->owner && ast_mutex_trylock(&pkt->owner->owner->lock)) {
			ast_mutex_unlock(&pkt->owner->lock);
			usleep(1);
			ast_mutex_lock(&pkt->owner->lock);
		}
		if (pkt->owner->owner) {
			ast_set_flag(pkt->owner, SIP_ALREADYGONE);
			ast_log(LOG_WARNING, "Hanging up call %s - no reply to our critical packet.\n", pkt->owner->callid);
			ast_queue_hangup(pkt->owner->owner);
			ast_mutex_unlock(&pkt->owner->owner->lock);
		} else {
			/* If no channel owner, destroy now */
			ast_set_flag(pkt->owner, SIP_NEEDDESTROY);	
		}
	}
	/* In any case, go ahead and remove the packet */
	prev = NULL;
	cur = pkt->owner->packets;
	while(cur) {
		if (cur == pkt)
			break;
		prev = cur;
		cur = cur->next;
	}
	if (cur) {
		if (prev)
			prev->next = cur->next;
		else
			pkt->owner->packets = cur->next;
		ast_mutex_unlock(&pkt->owner->lock);
		free(cur);
		pkt = NULL;
	} else
		ast_log(LOG_WARNING, "Weird, couldn't find packet owner!\n");
	if (pkt)
		ast_mutex_unlock(&pkt->owner->lock);
	return 0;
}

/*! \brief  __sip_reliable_xmit: transmit packet with retransmits ---*/
static int __sip_reliable_xmit(struct sip_pvt *p, int seqno, int resp, char *data, int len, int fatal, int sipmethod)
{
	struct sip_pkt *pkt;
	int siptimer_a = DEFAULT_RETRANS;

	pkt = malloc(sizeof(struct sip_pkt) + len + 1);
	if (!pkt)
		return -1;
	memset(pkt, 0, sizeof(struct sip_pkt));
	memcpy(pkt->data, data, len);
	pkt->method = sipmethod;
	pkt->packetlen = len;
	pkt->next = p->packets;
	pkt->owner = p;
	pkt->seqno = seqno;
	pkt->flags = resp;
	pkt->data[len] = '\0';
	pkt->timer_t1 = p->timer_t1;	/* Set SIP timer T1 */
	if (fatal)
		ast_set_flag(pkt, FLAG_FATAL);
	if (pkt->timer_t1)
		siptimer_a = pkt->timer_t1;

	/* Schedule retransmission */
	pkt->retransid = ast_sched_add_variable(sched, siptimer_a, retrans_pkt, pkt, 1);
	if (option_debug > 3 && sipdebug)
		ast_log(LOG_DEBUG, "*** SIP TIMER: Initalizing retransmit timer on packet: Id  #%d\n", pkt->retransid);
	pkt->next = p->packets;
	p->packets = pkt;

	__sip_xmit(pkt->owner, pkt->data, pkt->packetlen);	/* Send packet */
	if (sipmethod == SIP_INVITE) {
		/* Note this is a pending invite */
		p->pendinginvite = seqno;
	}
	return 0;
}

/*! \brief  __sip_autodestruct: Kill a call (called by scheduler) ---*/
static int __sip_autodestruct(void *data)
{
	struct sip_pvt *p = data;


	/* If this is a subscription, tell the phone that we got a timeout */
	if (p->subscribed) {
		p->subscribed = TIMEOUT;
		transmit_state_notify(p, AST_EXTENSION_DEACTIVATED, 1, 1);	/* Send first notification */
		p->subscribed = NONE;
		append_history(p, "Subscribestatus", "timeout");
		return 10000;	/* Reschedule this destruction so that we know that it's gone */
	}

	/* This scheduled event is now considered done. */
	p->autokillid = -1;

	ast_log(LOG_DEBUG, "Auto destroying call '%s'\n", p->callid);
	append_history(p, "AutoDestroy", "");
	if (p->owner) {
		ast_log(LOG_WARNING, "Autodestruct on call '%s' with owner in place\n", p->callid);
		ast_queue_hangup(p->owner);
	} else {
		sip_destroy(p);
	}
	return 0;
}

/*! \brief  sip_scheddestroy: Schedule destruction of SIP call ---*/
static int sip_scheddestroy(struct sip_pvt *p, int ms)
{
	char tmp[80];
	if (sip_debug_test_pvt(p))
		ast_verbose("Scheduling destruction of call '%s' in %d ms\n", p->callid, ms);
	if (recordhistory) {
		snprintf(tmp, sizeof(tmp), "%d ms", ms);
		append_history(p, "SchedDestroy", tmp);
	}

	if (p->autokillid > -1)
		ast_sched_del(sched, p->autokillid);
	p->autokillid = ast_sched_add(sched, ms, __sip_autodestruct, p);

	if (p->stimer && p->stimer->st_schedid > 0)
		stop_session_timer(p);

	return 0;
}

/*! \brief  sip_cancel_destroy: Cancel destruction of SIP call ---*/
static int sip_cancel_destroy(struct sip_pvt *p)
{
	if (p->autokillid > -1)
		ast_sched_del(sched, p->autokillid);
	append_history(p, "CancelDestroy", "");
	p->autokillid = -1;
	return 0;
}

/*! \brief  __sip_ack: Acknowledges receipt of a packet and stops retransmission ---*/
static int __sip_ack(struct sip_pvt *p, int seqno, int resp, int sipmethod)
{
	struct sip_pkt *cur, *prev = NULL;
	int res = -1;
	int resetinvite = 0;
	/* Just in case... */
	char *msg;

	msg = sip_methods[sipmethod].text;

	cur = p->packets;
	while(cur) {
		if ((cur->seqno == seqno) && ((ast_test_flag(cur, FLAG_RESPONSE)) == resp) &&
			((ast_test_flag(cur, FLAG_RESPONSE)) || 
			 (!strncasecmp(msg, cur->data, strlen(msg)) && (cur->data[strlen(msg)] < 33)))) {
			ast_mutex_lock(&p->lock);
			if (!resp && (seqno == p->pendinginvite)) {
				ast_log(LOG_DEBUG, "Acked pending invite %d\n", p->pendinginvite);
				p->pendinginvite = 0;
				resetinvite = 1;
			}
			/* this is our baby */
			if (prev)
				prev->next = cur->next;
			else
				p->packets = cur->next;
			if (cur->retransid > -1) {
				if (sipdebug && option_debug > 3)
					ast_log(LOG_DEBUG, "** SIP TIMER: Cancelling retransmit of packet (reply received) Retransid #%d\n", cur->retransid);
				ast_sched_del(sched, cur->retransid);
			}
			free(cur);
			ast_mutex_unlock(&p->lock);
			res = 0;
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	ast_log(LOG_DEBUG, "Stopping retransmission on '%s' of %s %d: Match %s\n", p->callid, resp ? "Response" : "Request", seqno, res ? "Not Found" : "Found");
	return res;
}

/*--- __sip_ack_prack: Acknowledges receipt of a packet and stops retransmission ---*/
static int __sip_ack_prack(struct sip_pvt *p, int rackno)
{
	struct sip_pkt *cur, *prev = NULL;
	int res = -1;

	cur = p->packets;
	while (cur) {
	    struct sip_request resp;
	    char *rseq_header, *dummy;
	    unsigned rseqno;

		memset(&resp, 0, sizeof(resp));
		resp.len = cur->packetlen;
		memcpy(resp.data, cur->data, cur->packetlen);
		parse_request(&resp);
		rseq_header = get_header(&resp, "RSeq");
		rseqno = strtoul(rseq_header, &dummy, 10);
		if (rseqno == rackno)
		{
			/* this is our baby */
			if (prev)
				prev->next = cur->next;
			else
				p->packets = cur->next;
			if (cur->retransid > -1)
				ast_sched_del(sched, cur->retransid);
			free(cur);
			res = 0;
			p->prack_status = PRACK_FIRST_ACK_RECEIVED;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	return res;
}

/* Pretend to ack all packets */
static int __sip_pretend_ack(struct sip_pvt *p)
{
	struct sip_pkt *cur=NULL;

	while(p->packets) {
		if (cur == p->packets) {
			ast_log(LOG_WARNING, "Have a packet that doesn't want to give up! %s\n", sip_methods[cur->method].text);
			return -1;
		}
		cur = p->packets;
		if (cur->method)
			__sip_ack(p, p->packets->seqno, (ast_test_flag(p->packets, FLAG_RESPONSE)), cur->method);
		else {	/* Unknown packet type */
			char *c;
			char method[128];
			ast_copy_string(method, p->packets->data, sizeof(method));
			c = ast_skip_blanks(method); /* XXX what ? */
			*c = '\0';
			__sip_ack(p, p->packets->seqno, (ast_test_flag(p->packets, FLAG_RESPONSE)), find_sip_method(method));
		}
	}
	return 0;
}

/*! \brief  __sip_semi_ack: Acks receipt of packet, keep it around (used for provisional responses) ---*/
static int __sip_semi_ack(struct sip_pvt *p, int seqno, int resp, int sipmethod)
{
	struct sip_pkt *cur;
	int res = -1;
	char *msg = sip_methods[sipmethod].text;

	cur = p->packets;
	while(cur) {
		if ((cur->seqno == seqno) && ((ast_test_flag(cur, FLAG_RESPONSE)) == resp) &&
			((ast_test_flag(cur, FLAG_RESPONSE)) || 
			 (!strncasecmp(msg, cur->data, strlen(msg)) && (cur->data[strlen(msg)] < 33)))) {
			/* this is our baby */
			if (cur->retransid > -1) {
				if (option_debug > 3 && sipdebug)
					ast_log(LOG_DEBUG, "*** SIP TIMER: Cancelling retransmission #%d - %s (got response)\n", cur->retransid, msg);
				ast_sched_del(sched, cur->retransid);
			}
			cur->retransid = -1;
			res = 0;
			break;
		}
		cur = cur->next;
	}
	ast_log(LOG_DEBUG, "(Provisional) Stopping retransmission (but retaining packet) on '%s' %s %d: %s\n", p->callid, resp ? "Response" : "Request", seqno, res ? "Not Found" : "Found");
	return res;
}

/* Pretend to semi_ack all packets */
static int __sip_pretend_semi_ack(struct sip_pvt *p)
{
	struct sip_pkt *cur=p->packets;

	while(cur) {
		if (cur->method)
			__sip_semi_ack(p, p->packets->seqno, (ast_test_flag(p->packets, FLAG_RESPONSE)), cur->method);
		else {  /* Unknown packet type */
			char *c;
			char method[128];
			ast_copy_string(method, p->packets->data, sizeof(method));
			c = ast_skip_blanks(method); /* XXX what ? */
			*c = '\0';
			__sip_semi_ack(p, p->packets->seqno, (ast_test_flag(p->packets, FLAG_RESPONSE)), find_sip_method(method));
		}
		cur = cur->next;
	}
	return 0;
}
static void parse_request(struct sip_request *req);
static char *get_header(struct sip_request *req, char *name);
static void copy_request(struct sip_request *dst,struct sip_request *src);

/*! \brief  parse_copy: Copy SIP request, parse it */
static void parse_copy(struct sip_request *dst, struct sip_request *src)
{
	memset(dst, 0, sizeof(*dst));
	memcpy(dst->data, src->data, sizeof(dst->data));
	dst->len = src->len;
	parse_request(dst);
}

static void addr2str(struct sockaddr_in addr, char *str, int strlen)
{
	char iabuf[INET_ADDRSTRLEN];

	snprintf(str, strlen, "%s:%d", ast_inet_ntoa(iabuf, sizeof(iabuf),
		addr.sin_addr), ntohs(addr.sin_port));
}

/*! \brief  send_response: Transmit response on SIP request---*/
static int send_response(struct sip_pvt *p, struct sip_request *req, int reliable, int seqno)
{
	int res;
	struct sip_request tmp;
	char tmpmsg[80];
	char dest[256];
	int is_nat = ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE;

	addr2str(is_nat ? p->recv : p->sa, dest, sizeof(dest));

	if (sip_debug_test_pvt(p)) {
		ast_verbose("%sTransmitting (%sNAT) to %s:\n%s\n---\n",
			reliable ? "Reliably " : "", is_nat ? "" : "no ", dest, req->data);
	} else if (option_debug > 4)
		ast_log(LOG_EVENT, "[out] to %s:\n%s\n", dest, req->data);
	if (reliable) {
		if (recordhistory) {
			parse_copy(&tmp, req);
			snprintf(tmpmsg, sizeof(tmpmsg), "%s / %s", tmp.data, get_header(&tmp, "CSeq"));
			append_history(p, "TxRespRel", tmpmsg);
		}
		res = __sip_reliable_xmit(p, seqno, 1, req->data, req->len, (reliable > 1), req->method);
	} else {
		if (recordhistory) {
			parse_copy(&tmp, req);
			snprintf(tmpmsg, sizeof(tmpmsg), "%s / %s", tmp.data, get_header(&tmp, "CSeq"));
			append_history(p, "TxResp", tmpmsg);
		}
		res = __sip_xmit(p, req->data, req->len);
	}
	if (res > 0)
		return 0;
	return res;
}

/*! \brief  send_request: Send SIP Request to the other part of the dialogue ---*/
static int send_request(struct sip_pvt *p, struct sip_request *req, int reliable, int seqno)
{
	int res;
	struct sip_request tmp;
	char tmpmsg[80];
	char dest[256];
	int is_nat = ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE;

	addr2str(is_nat ? p->recv : p->sa, dest, sizeof(dest));

	if (sip_debug_test_pvt(p)) {
		ast_verbose("%sTransmitting (%sNAT) to %s:\n%s\n---\n",
			reliable ? "Reliably " : "", is_nat ? "" : "no ", dest, req->data);
	} else if (option_debug > 4)
		ast_log(LOG_EVENT, "[out] to %s:\n%s\n", dest, req->data);
	if (reliable) {
		if (recordhistory) {
			parse_copy(&tmp, req);
			snprintf(tmpmsg, sizeof(tmpmsg), "%s / %s", tmp.data, get_header(&tmp, "CSeq"));
			append_history(p, "TxReqRel", tmpmsg);
		}
		res = __sip_reliable_xmit(p, seqno, 0, req->data, req->len, (reliable > 1), req->method);
	} else {
		if (recordhistory) {
			parse_copy(&tmp, req);
			snprintf(tmpmsg, sizeof(tmpmsg), "%s / %s", tmp.data, get_header(&tmp, "CSeq"));
			append_history(p, "TxReq", tmpmsg);
		}
		res = __sip_xmit(p, req->data, req->len);
	}
	return res;
}

/*! \brief  get_in_brackets: Pick out text in brackets from character string ---*/
/* returns pointer to terminated stripped string. modifies input string. */
static char *get_in_brackets(char *tmp)
{
	char *parse;
	char *first_quote;
	char *first_bracket;
	char *second_bracket;
	char last_char;

	parse = tmp;
	while (1) {
		first_quote = strchr(parse, '"');
		first_bracket = strchr(parse, '<');
		if (first_quote && first_bracket && (first_quote < first_bracket)) {
			last_char = '\0';
			for (parse = first_quote + 1; *parse; parse++) {
				if ((*parse == '"') && (last_char != '\\'))
					break;
				last_char = *parse;
			}
			if (!*parse) {
				ast_log(LOG_WARNING, "No closing quote found in '%s'\n", tmp);
				return tmp;
			}
			parse++;
			continue;
		}
		if (first_bracket) {
			second_bracket = strchr(first_bracket + 1, '>');
			if (second_bracket) {
				*second_bracket = '\0';
				return first_bracket + 1;
			} else {
				ast_log(LOG_WARNING, "No closing bracket found in '%s'\n", tmp);
				return tmp;
			}
		}
		return tmp;
	}
}

/*! \brief  sip_sendtext: Send SIP MESSAGE text within a call ---*/
/*      Called from PBX core text message functions */
static int sip_sendtext(struct ast_channel *ast, const char *text)
{
	struct sip_pvt *p = ast->tech_pvt;
	int debug=sip_debug_test_pvt(p);

	if (debug)
		ast_verbose("Sending text %s on %s\n", text, ast->name);
	if (!p)
		return -1;
	if (ast_strlen_zero(text))
		return 0;
	if (debug)
		ast_verbose("Really sending text %s on %s\n", text, ast->name);
	transmit_message_with_text(p, text);
	return 0;	
}

/* Send a SIP MESSAGE with arbitrary content out-of-call */
static int sip_sendmessage(char *peername, char *smsc, char *ip_sm_gw, char *content_type, char *content, int len)
{
	struct sip_pvt *p;
	struct sip_peer *peer;

	p = sip_alloc(NULL, NULL, 0, SIP_MESSAGE);
	if (!p) {
		ast_log(LOG_ERROR, "Unable to allocate MESSAGE dialog\n");
		return 0;
	}

	peer = find_peer(peername, NULL, 1);
	if (!peer)
	{
		sip_destroy(p);
		ast_log(LOG_ERROR, "Couldn't find peer %s!\n", peername);
		return 0;
	}

	if (create_addr_from_peer(p, peer))
	{
		sip_destroy(p);
		ASTOBJ_UNREF(peer, sip_destroy_peer);
		return 0;
	}

	if (!ast_strlen_zero(ip_sm_gw))
		ast_copy_string(p->fullcontact, ip_sm_gw, sizeof(p->fullcontact));
	else if (!ast_strlen_zero(smsc))
		ast_copy_string(p->username, smsc, sizeof(p->username));
	ast_set_flag(p, SIP_OUTGOING);
	transmit_message_with_content(p, content_type, content, len, 0);
	sip_scheddestroy(p, 15000);
	ASTOBJ_UNREF(peer, sip_destroy_peer);
	return 0;
}

static struct ast_rtp *sip_get_rtp(struct ast_channel *ast)
{
	struct sip_pvt *p = ast->tech_pvt;
	struct ast_rtp *rtp;

	if (!p)
		return NULL;
	ast_mutex_lock(&p->lock);
	rtp = p->rtp;
	ast_mutex_unlock(&p->lock);
	return rtp;
}

/*! \brief Deliver SIP call ID for the call */
static char *sip_get_callid(struct ast_channel *chan)
{
	return chan->tech_pvt ? ((struct sip_pvt *) chan->tech_pvt)->callid : "";
}

static struct ast_udptl *sip_get_udptl(struct ast_channel *ast)
{
	struct sip_pvt *p = ast->tech_pvt;
	struct ast_udptl *udptl;

	if (!p)
		return NULL;
	ast_mutex_lock(&p->lock);
	udptl = p->udptl;
	ast_mutex_unlock(&p->lock);
	return udptl;
}

/*! \brief  realtime_update_peer: Update peer object in realtime storage ---*/
static void realtime_update_peer(const char *peername, struct sockaddr_in *sin, const char *username, const char *fullcontact, int expirey)
{
	char port[10];
	char ipaddr[20];
	char regseconds[20];
	time_t nowtime;
	
	time(&nowtime);
	nowtime += expirey;
	snprintf(regseconds, sizeof(regseconds), "%d", (int)nowtime);	/* Expiration time */
	ast_inet_ntoa(ipaddr, sizeof(ipaddr), sin->sin_addr);
	snprintf(port, sizeof(port), "%d", ntohs(sin->sin_port));
	
	if (fullcontact)
		ast_update_realtime("sippeers", "name", peername, "ipaddr", ipaddr, "port", port, "regseconds", regseconds, "username", username, "fullcontact", fullcontact, NULL);
	else
		ast_update_realtime("sippeers", "name", peername, "ipaddr", ipaddr, "port", port, "regseconds", regseconds, "username", username, NULL);
}

/*! \brief  register_peer_exten: Automatically add peer extension to dial plan ---*/
static void register_peer_exten(struct sip_peer *peer, int onoff)
{
	char multi[256];
	char *stringp, *ext;
	if (!ast_strlen_zero(regcontext)) {
		ast_copy_string(multi, ast_strlen_zero(peer->regexten) ? peer->name : peer->regexten, sizeof(multi));
		stringp = multi;
		while((ext = strsep(&stringp, "&"))) {
			if (onoff)
				ast_add_extension(regcontext, 1, ext, 1, NULL, NULL, "Noop", strdup(peer->name), free, channeltype);
			else
				ast_context_remove_extension(regcontext, ext, 1, NULL);
		}
	}
}

/*! \brief  sip_destroy_peer: Destroy peer object from memory */
static void sip_destroy_peer(struct sip_peer *peer)
{
	/* Delete it, it needs to disappear */
	if (peer->call)
		sip_destroy(peer->call);
	if (peer->chanvars) {
		ast_variables_destroy(peer->chanvars);
		peer->chanvars = NULL;
	}
	if (peer->expire > -1)
		ast_sched_del(sched, peer->expire);
	if (peer->pokeexpire > -1)
		ast_sched_del(sched, peer->pokeexpire);
	register_peer_exten(peer, 0);
	ast_free_ha(peer->ha);
	if (ast_test_flag(peer, SIP_SELFDESTRUCT))
		apeerobjs--;
	else if (ast_test_flag(peer, SIP_REALTIME))
		rpeerobjs--;
	else
		speerobjs--;
	clear_realm_authentication(peer->auth);
	peer->auth = (struct sip_auth *) NULL;
	if (peer->dnsmgr)
		ast_dnsmgr_release(peer->dnsmgr);
	if (ast_test_flag(&(peer->flags_page2), SIP_PAGE2_FROM_TEMPLATE))
	{
	    	ast_run_openrg_cmd("template_update");
	    	global_reg_from_templates--;
	}
	if (peer->registry)
		ASTOBJ_UNREF(peer->registry, sip_registry_destroy);
	free(peer);
}

/*! \brief  update_peer: Update peer data in database (if used) ---*/
static void update_peer(struct sip_peer *p, int expiry)
{
	int rtcachefriends = ast_test_flag(&(p->flags_page2), SIP_PAGE2_RTCACHEFRIENDS);
	if (ast_test_flag((&global_flags_page2), SIP_PAGE2_RTUPDATE) &&
		(ast_test_flag(p, SIP_REALTIME) || rtcachefriends)) {
		realtime_update_peer(p->name, &p->addr, p->username, rtcachefriends ? p->fullcontact : NULL, expiry);
	}
}


/*! \brief  realtime_peer: Get peer from realtime storage
 * Checks the "sippeers" realtime family from extconfig.conf */
static struct sip_peer *realtime_peer(const char *peername, struct sockaddr_in *sin)
{
	struct sip_peer *peer=NULL;
	struct ast_variable *var;
	struct ast_variable *tmp;
	char *newpeername = (char *) peername;
	char iabuf[80];

	/* First check on peer name */
	if (newpeername) 
		var = ast_load_realtime("sippeers", "name", peername, NULL);
	else if (sin) {	/* Then check on IP address */
		ast_inet_ntoa(iabuf, sizeof(iabuf), sin->sin_addr);
		var = ast_load_realtime("sippeers", "ipaddr", iabuf, NULL);
	} else
		return NULL;

	if (!var)
		return NULL;

	tmp = var;
	/* If this is type=user, then skip this object. */
	while(tmp) {
		if (!strcasecmp(tmp->name, "type") &&
		    !strcasecmp(tmp->value, "user")) {
			ast_variables_destroy(var);
			return NULL;
		} else if (!newpeername && !strcasecmp(tmp->name, "name")) {
			newpeername = tmp->value;
		}
		tmp = tmp->next;
	}
	
	if (!newpeername) {	/* Did not find peer in realtime */
		ast_log(LOG_WARNING, "Cannot Determine peer name ip=%s\n", iabuf);
		ast_variables_destroy(var);
		return (struct sip_peer *) NULL;
	}

	/* Peer found in realtime, now build it in memory */
	peer = build_peer(newpeername, var, !ast_test_flag((&global_flags_page2), SIP_PAGE2_RTCACHEFRIENDS), 0);
	if (!peer) {
		ast_variables_destroy(var);
		return (struct sip_peer *) NULL;
	}

	if (ast_test_flag((&global_flags_page2), SIP_PAGE2_RTCACHEFRIENDS)) {
		/* Cache peer */
		ast_copy_flags((&peer->flags_page2),(&global_flags_page2), SIP_PAGE2_RTAUTOCLEAR|SIP_PAGE2_RTCACHEFRIENDS);
		if (ast_test_flag((&global_flags_page2), SIP_PAGE2_RTAUTOCLEAR)) {
			if (peer->expire > -1) {
				ast_sched_del(sched, peer->expire);
			}
			peer->expire = ast_sched_add(sched, (global_rtautoclear) * 1000, expire_register, (void *)peer);
		}
		ASTOBJ_CONTAINER_LINK(&peerl,peer);
	} else {
		ast_set_flag(peer, SIP_REALTIME);
	}
	ast_variables_destroy(var);

	return peer;
}

/*! \brief  sip_addrcmp: Support routine for find_peer ---*/
static int sip_addrcmp(char *name, struct sockaddr_in *sin)
{
	/* We know name is the first field, so we can cast */
	struct sip_peer *p = (struct sip_peer *)name;
	return 	!(!inaddrcmp(&p->addr, sin) || 
					(ast_test_flag(p, SIP_INSECURE_PORT) &&
					(p->addr.sin_addr.s_addr == sin->sin_addr.s_addr)));
}

/*! \brief check peer_name with template */
static int sip_peer_name_template(char *template, char *peer_name)
{
    int i;

    for (i = 0; i < strlen(peer_name); i++)
    {
	if (template[i] == 'X' && isxdigit(peer_name[i]))
	    continue;

	return 0;
    }

    return 1;
}

/*! \brief  sip_template_cmp: Support routine for find_peer ---*/
static int sip_template_cmp(char *template, char *peer_name)
{
	/* We know name is the first field, so we can cast */
	struct sip_peer *p = (struct sip_peer *)template;

	if (!ast_test_flag(&(p->flags_page2), SIP_PAGE2_TEMPLATE) || strlen(template) != strlen(peer_name))
	    return -1;

	if (!sip_peer_name_template(template, peer_name))
	    return -1;
	
	return 0;
}

/*! \brief  find_peer: Locate peer by name or ip address 
 *	This is used on incoming SIP message to find matching peer on ip
	or outgoing message to find matching peer on name */
static struct sip_peer *find_peer(const char *peer, struct sockaddr_in *sin, int realtime)
{
	struct sip_peer *p = NULL;

	if (peer)
		p = ASTOBJ_CONTAINER_FIND(&peerl,peer);
	else
		p = ASTOBJ_CONTAINER_FIND_FULL(&peerl,sin,name,sip_addr_hashfunc,1,sip_addrcmp);

	if (!p && realtime) {
		p = realtime_peer(peer, sin);
	}

	return p;
}

/*! \brief  find_create_template_peer: Locate template and create new
 * peer instance
 *	This is used on SIP REGISTER to find matching template */
static struct sip_peer *find_create_template_peer(char *peer_name, struct sockaddr_in *sin, int realtime, int *res)
{
	struct sip_peer *p = NULL, *tmp = NULL;
	struct sip_user *user;

	/* Check if the peer already exist */
	if ((p = find_peer(peer_name, sin, realtime)))
	    return p;

	/* Find peer template */
	tmp = ASTOBJ_CONTAINER_FIND_FULL(&peerl,peer_name,name,sip_addr_hashfunc,1,sip_template_cmp);
	if (tmp)
	{
	    struct ast_config *cfg = ast_config_load(config);
	    struct ast_variable *var = ast_variable_browse(cfg, tmp->name);

	    if (global_reg_from_templates >= global_max_from_templates)
	    {
		*res = -4;
		goto Exit;
	    }

	    /* Create new peer from the template */
	    p = build_peer(peer_name, var, 0, 0);
	    if (p)
	    {
		ast_clear_flag(&(p->flags_page2), SIP_PAGE2_TEMPLATE);
		ast_set_flag(&(p->flags_page2), SIP_PAGE2_FROM_TEMPLATE);
		ast_set_flag(p, SIP_SELFDESTRUCT);
		ast_copy_string(p->username, peer_name, sizeof(p->username));
		ast_copy_string(p->mailbox, peer_name, sizeof(p->mailbox));
		global_reg_from_templates++;
		ASTOBJ_CONTAINER_LINK(&peerl,p);
	    }

	    /* Create user for the newly created peer */
	    user = build_user(peer_name, var, 0);
	    if (user) {
		ASTOBJ_CONTAINER_LINK(&userl,user);
		ASTOBJ_UNREF(user, sip_destroy_user);
	    }
	    ast_config_destroy(cfg);

	    ast_run_openrg_cmd("template_update");
	}

Exit:
	if (tmp)
	    ASTOBJ_UNREF(tmp, sip_destroy_peer);
	return p;
}

/*! \brief  sip_destroy_user: Remove user object from in-memory storage ---*/
static void sip_destroy_user(struct sip_user *user)
{
	ast_free_ha(user->ha);
	if (user->chanvars) {
		ast_variables_destroy(user->chanvars);
		user->chanvars = NULL;
	}
	if (ast_test_flag(user, SIP_REALTIME))
		ruserobjs--;
	else
		suserobjs--;
	free(user);
}

/*! \brief  realtime_user: Load user from realtime storage
 * Loads user from "sipusers" category in realtime (extconfig.conf)
 * Users are matched on From: user name (the domain in skipped) */
static struct sip_user *realtime_user(const char *username)
{
	struct ast_variable *var;
	struct ast_variable *tmp;
	struct sip_user *user = NULL;

	var = ast_load_realtime("sipusers", "name", username, NULL);

	if (!var)
		return NULL;

	tmp = var;
	while (tmp) {
		if (!strcasecmp(tmp->name, "type") &&
			!strcasecmp(tmp->value, "peer")) {
			ast_variables_destroy(var);
			return NULL;
		}
		tmp = tmp->next;
	}
	


	user = build_user(username, var, !ast_test_flag((&global_flags_page2), SIP_PAGE2_RTCACHEFRIENDS));
	
	if (!user) {	/* No user found */
		ast_variables_destroy(var);
		return NULL;
	}

	if (ast_test_flag((&global_flags_page2), SIP_PAGE2_RTCACHEFRIENDS)) {
		ast_set_flag((&user->flags_page2), SIP_PAGE2_RTCACHEFRIENDS);
		suserobjs++;
		ASTOBJ_CONTAINER_LINK(&userl,user);
	} else {
		/* Move counter from s to r... */
		suserobjs--;
		ruserobjs++;
		ast_set_flag(user, SIP_REALTIME);
	}
	ast_variables_destroy(var);
	return user;
}

/*! \brief  find_user: Locate user by name 
 * Locates user by name (From: sip uri user name part) first
 * from in-memory list (static configuration) then from 
 * realtime storage (defined in extconfig.conf) */
static struct sip_user *find_user(const char *name, int realtime)
{
	struct sip_user *u = NULL;
	u = ASTOBJ_CONTAINER_FIND(&userl,name);
	if (!u && realtime) {
		u = realtime_user(name);
	}
	return u;
}

/* Resolves the peer address and if successful updates the peer.
 * return 0 if failed. */
static int resolve_peer_addr(struct sip_peer *peer)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    if (!peer->obproxy[0] && !peer->tohost[0])
	return 0;

    if (ast_get_ip_or_srv(&addr, peer->obproxy[0] ? peer->obproxy :
	peer->tohost, srvlookup ? "_sip._udp" : NULL))
    {
	ast_log(LOG_WARNING, "Failed to resolve peer \'%s\' addr.\n",
	    peer->name);
	return 0;
    }
    peer->addr.sin_family = addr.sin_family;
    peer->addr.sin_addr = addr.sin_addr;

    return 1;
}

/*! \brief  update_peer_from_registry: Sets the address and user of peer based on registry entry ---*/
static void update_peer_from_registry(struct sip_registry *reg, struct sockaddr_in addr)
{
	struct sip_peer *peer;

	if (!(peer=find_peer(reg->contact, NULL, 1))) 
	{
		ast_log(LOG_ERROR, "Fail to find peer %s'\n", reg->contact);
		return;
	}

	ast_log(LOG_DEBUG, "Updating address of peer '%s'\n", peer->name);
	ast_copy_string(peer->obproxy, reg->obproxy, sizeof(peer->obproxy));
	ast_copy_string(peer->tohost, reg->hostname, sizeof(peer->tohost));
	peer->addr.sin_family = addr.sin_family;
	peer->addr.sin_addr = addr.sin_addr;
	peer->addr.sin_port = addr.sin_port;
	ast_copy_string(peer->username, cCONFIG_RG_VODAFONE_NZ ? reg->username : reg->contact, sizeof(peer->username));
	ast_copy_string(peer->secret, reg->secret, sizeof(peer->secret));
	ast_copy_string(peer->md5secret, reg->md5secret, sizeof(peer->md5secret));
	ast_copy_string(peer->fromdomain, reg->regdomain, sizeof(peer->fromdomain));
	ast_copy_string(peer->todomain, reg->regdomain, sizeof(peer->todomain));
	ast_copy_string(peer->regname, reg->name, sizeof(peer->regname));
	ASTOBJ_UNREF(peer,sip_destroy_peer);

    return;
}

static int update_addr_from_registry(struct sip_peer *peer)
{
	struct sip_uas *uas_srv = &peer->registry->uas_srv;

	if (!uas_srv->addr.sin_addr.s_addr)
		return -1;

	peer->addr.sin_family = uas_srv->addr.sin_family;
	peer->addr.sin_addr = uas_srv->addr.sin_addr;
	peer->addr.sin_port = uas_srv->addr.sin_port;
	return 0;
}

/*! \brief  create_addr_from_peer: create address structure from peer reference ---*/
static int create_addr_from_peer(struct sip_pvt *r, struct sip_peer *peer)
{
	char *callhost;

	/* Check for bound registry */
	if (peer->registry && update_addr_from_registry(peer))
	{
	    ast_log(LOG_WARNING, "peer %s registry does not have IP.\n",
		peer->name);
	    return -1;
	}

 	if (!peer->maxms ||
 	    ((peer->lastms >= 0) && (peer->lastms <= peer->maxms))) {
 		if (peer->addr.sin_addr.s_addr || resolve_peer_addr(peer)) {
			r->sa.sin_family = peer->addr.sin_family;
			r->sa.sin_addr = peer->addr.sin_addr;
			r->sa.sin_port = peer->addr.sin_port;
 		} else if (peer->defaddr.sin_addr.s_addr) {
			r->sa.sin_family = peer->defaddr.sin_family;
			r->sa.sin_addr = peer->defaddr.sin_addr;
			r->sa.sin_port = peer->defaddr.sin_port;
 		} else {
 		    return -1;
		}
		memcpy(&r->recv, &r->sa, sizeof(r->recv));
	} else {
		return -1;
	}

	ast_copy_flags(r, peer, SIP_FLAGS_TO_COPY);
	r->usercapability = peer->capability;
	r->userprefs = peer->prefs;
	 
#if defined(T38_SUPPORT)
	r->t38capability = global_t38_capability;
	if (r->udptl) {
		if ( ast_udptl_get_error_correction_scheme(r->udptl) == UDPTL_ERROR_CORRECTION_FEC )
			r->t38capability |= T38FAX_UDP_EC_FEC;
		else if ( ast_udptl_get_error_correction_scheme(r->udptl) == UDPTL_ERROR_CORRECTION_REDUNDANCY )
			r->t38capability |= T38FAX_UDP_EC_REDUNDANCY;			
		else if (  ast_udptl_get_error_correction_scheme(r->udptl) == UDPTL_ERROR_CORRECTION_NONE )
			r->t38capability |= T38FAX_UDP_EC_NONE;
		r->t38capability |= T38FAX_RATE_MANAGEMENT_TRANSFERED_TCF;
		ast_log(LOG_DEBUG,"Our T38 capability (%d)\n", r->t38capability);
	}
	r->t38jointcapability = r->t38capability;
#endif
	
	if (r->rtp) {
		ast_log(LOG_DEBUG, "Setting NAT on RTP to %d\n", (ast_test_flag(r, SIP_NAT) & SIP_NAT_ROUTE));
		ast_rtp_setnat(r->rtp, (ast_test_flag(r, SIP_NAT) & SIP_NAT_ROUTE));
		if (peer->rtcp_interval) {
			ast_log(LOG_DEBUG, "Setting RTCP interval to %dms\n",
				peer->rtcp_interval);
			ast_rtp_setrtcpinterval(r->rtp, peer->rtcp_interval);
		}
	}
	if (r->vrtp) {
		ast_log(LOG_DEBUG, "Setting NAT on VRTP to %d\n", (ast_test_flag(r, SIP_NAT) & SIP_NAT_ROUTE));
		ast_rtp_setnat(r->vrtp, (ast_test_flag(r, SIP_NAT) & SIP_NAT_ROUTE));
	}
#if defined(T38_SUPPORT)
	if (r->udptl) {
		ast_log(LOG_DEBUG, "Setting NAT on UDPTL to %d\n", (ast_test_flag(r, SIP_NAT) & SIP_NAT_ROUTE));
		ast_udptl_setnat(r->udptl, (ast_test_flag(r, SIP_NAT) & SIP_NAT_ROUTE));
	}
#endif
	ast_copy_string(r->peername, peer->username, sizeof(r->peername));
	ast_copy_string(r->authname, peer->username, sizeof(r->authname));
	ast_copy_string(r->username, peer->username, sizeof(r->username));
	ast_copy_string(r->peersecret, peer->secret, sizeof(r->peersecret));
	ast_copy_string(r->peermd5secret, peer->md5secret, sizeof(r->peermd5secret));
	ast_copy_string(r->tohost, peer->tohost, sizeof(r->tohost));
	if (!ast_strlen_zero(peer->todomain))
		ast_copy_string(r->todomain, peer->todomain, sizeof(r->todomain));
	ast_copy_string(r->fullcontact, peer->fullcontact, sizeof(r->fullcontact));
	if (!ast_strlen_zero(peer->regname))
	    ast_copy_string(r->regname, peer->regname, sizeof(r->regname));
	r->toport = peer->toport;
	if (!r->initreq.headers && !ast_strlen_zero(peer->fromdomain)) {
		if ((callhost = strchr(r->callid, '@'))) {
			strncpy(callhost + 1, peer->fromdomain, sizeof(r->callid) - (callhost - r->callid) - 2);
		}
	}
	if (ast_strlen_zero(r->tohost)) {
		if (peer->addr.sin_addr.s_addr) {
			ast_inet_ntoa(r->tohost, sizeof(r->tohost), peer->addr.sin_addr);
			r->toport = peer->addr.sin_port;
		}
		else {
			ast_inet_ntoa(r->tohost, sizeof(r->tohost), peer->defaddr.sin_addr);
			r->toport = peer->defaddr.sin_port;
		}
	}
	if (!ast_strlen_zero(peer->fromdomain))
		ast_copy_string(r->fromdomain, peer->fromdomain, sizeof(r->fromdomain));
	if (!ast_strlen_zero(peer->fromuser))
		ast_copy_string(r->fromuser, peer->fromuser, sizeof(r->fromuser));
	if (!ast_strlen_zero(peer->fromname))
		ast_copy_string(r->fromname, peer->fromname, sizeof(r->fromname));
	if (!ast_strlen_zero(peer->fromuri))
		ast_copy_string(r->fromuri, peer->fromuri, sizeof(r->fromuri));	
	if (!ast_strlen_zero(peer->organization))
		ast_copy_string(r->organization, peer->organization, sizeof(r->organization));	
	r->prack_level = peer->prack_level;
	r->maxtime = peer->maxms;
	r->callgroup = peer->callgroup;
	r->faxtxcodecs = peer->faxtxcodecs;
	r->modemtxcodecs = peer->modemtxcodecs;
	r->faxmethod = peer->faxmethod;
	r->pickupgroup = peer->pickupgroup;
	r->clir = peer->clir;
	r->displayinfo = peer->displayinfo;
	/* Set timer T1 to RTT for this peer (if known by qualify=) */
	if (peer->maxms && peer->lastms)
		r->timer_t1 = peer->lastms < DEFAULT_T1MIN ? DEFAULT_T1MIN : peer->lastms;
	if ((ast_test_flag(r, SIP_DTMF) == SIP_DTMF_RFC2833) || (ast_test_flag(r, SIP_DTMF) == SIP_DTMF_AUTO))
		r->noncodeccapability |= AST_RTP_DTMF;
	else
		r->noncodeccapability &= ~AST_RTP_DTMF;
	ast_copy_string(r->context, peer->context,sizeof(r->context));
	r->rtptimeout = peer->rtptimeout;
	r->rtpholdtimeout = peer->rtpholdtimeout;
	r->rtpkeepalive = peer->rtpkeepalive;
	if (peer->call_limit)
		ast_set_flag(r, SIP_CALL_LIMIT);
	ast_copy_flags((&r->flags_page2), (&peer->flags_page2), SIP_PAGE2_G729_ANNEXB);
	ast_copy_flags(&r->flags_page2, &peer->flags_page2, 
		SIP_SESSION_TIMERS_FLAGS_TO_COPY);

	return 0;
}

/*! \brief create_uas_addr: Creates address structure and srv_entry list
           from global DNS. */
static int create_uas_addr(struct sip_pvt *dialog, struct sip_uas *uas_srv, 
	char *opeer)
{
	char service[MAXHOSTNAMELEN];
	int fresh_query = !!uas_srv->context;
	int ret;

	if (uas_srv->healthy)
	{
	    /* Current SRV record was found to work, let's use it.
	     * XXX: we are ignoring its TTL.
	     */
	    goto Exit;
	}

	snprintf(service, sizeof(service), "_sip._udp.%s", opeer);

	if ((ret = ast_get_next_srv_ip(&uas_srv->context, &uas_srv->addr, service, 
		min_srv_ttl)))
	{
		if (fresh_query)
		{
			/* Failed getting SRV result on a fresh query. Nothing
			 * to do.
			 */
			return ret;
		}
		/* We may have had only stale results, let's try again */
		if ((ret = ast_get_next_srv_ip(&uas_srv->context, &uas_srv->addr, 
			service, min_srv_ttl)))
		{
			/* Bad luck. Nothing to do */
			return ret;
		}
	}

Exit:
	dialog->sa.sin_family = uas_srv->addr.sin_family;
	dialog->sa.sin_addr = uas_srv->addr.sin_addr;
	dialog->sa.sin_port = uas_srv->addr.sin_port;
	memcpy(&dialog->recv, &dialog->sa, sizeof(dialog->recv));
	return 0;
}

/*! \brief  create_addr: create address structure from peer name
 *      Or, if peer not found, find it in the global DNS 
 *      returns TRUE (-1) on failure, FALSE on success */
static int create_addr(struct sip_pvt *dialog, char *opeer)
{
	struct hostent *hp;
	struct ast_hostent ahp;
	struct sip_peer *p;
	int found=0;
	char *port;
	int portno;
	char host[MAXHOSTNAMELEN], *hostn;
	char peer[256];

	ast_copy_string(peer, opeer, sizeof(peer));
	port = strchr(peer, ':');
	if (port) {
		*port = '\0';
		port++;
	}
	dialog->sa.sin_family = AF_INET;
	dialog->timer_t1 = 500; /* Default SIP retransmission timer T1 (RFC 3261) */
	p = find_peer(peer, NULL, 1);

	if (p) {
		found++;
		if (create_addr_from_peer(dialog, p))
			ASTOBJ_UNREF(p, sip_destroy_peer);
	}
	if (!p) {
		char *ptr, *hostp;

		if (found)
			return -1;

		hostn = peer;
		if (port)
			portno = atoi(port);
		else
			portno = DEFAULT_SIP_PORT;
		if (srvlookup) {
			char service[MAXHOSTNAMELEN];
			int tportno;
			int ret;
			snprintf(service, sizeof(service), "_sip._udp.%s", peer);

			ret = ast_get_srv(NULL, host, sizeof(host), &tportno, service);
			if (ret > 0) {
				hostn = host;
				portno = tportno;
			}
		}
		if ((hostp = ast_strdupa(hostn))) {
			if ((ptr = strchr(hostp, '?'))) {
				*ptr = '\0';
			}
		} else {
			hostp = peer;
		}
		
		hp = ast_gethostbyname(hostp, &ahp);
		if (hp) {
			ast_copy_string(dialog->tohost, hostp, sizeof(dialog->tohost));
			memcpy(&dialog->sa.sin_addr, hp->h_addr, sizeof(dialog->sa.sin_addr));
			dialog->sa.sin_port = htons(portno);
			dialog->toport = portno;
			memcpy(&dialog->recv, &dialog->sa, sizeof(dialog->recv));
			return 0;
		} else {
			ast_log(LOG_WARNING, "No such host: %s\n", peer);
			return -1;
		}
	} else {
		ASTOBJ_UNREF(p, sip_destroy_peer);
		return 0;
	}
}

/*! \brief  auto_congest: Scheduled congestion on a call ---*/
static int auto_congest(void *nothing)
{
	struct sip_pvt *p = nothing;
	ast_mutex_lock(&p->lock);
	p->initid = -1;
	if (p->owner) {
		if (!ast_mutex_trylock(&p->owner->lock)) {
			ast_log(LOG_NOTICE, "Auto-congesting %s\n", p->owner->name);
			ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
			ast_mutex_unlock(&p->owner->lock);
		}
	}
	ast_mutex_unlock(&p->lock);
	return 0;
}

static void set_template_caller_name(struct sip_pvt *p)
{
	struct sip_peer *peer = NULL;

	if (p->username && (peer = find_peer(p->username, NULL, 0)) && 
	    ast_test_flag(&(peer->flags_page2), SIP_PAGE2_FROM_TEMPLATE)) 
	{
	        snprintf(p->owner->template_caller_name, sizeof(p->owner->template_caller_name), "SIP/%s", p->username);			    	
	}

	if (peer)
		ASTOBJ_UNREF(peer, sip_destroy_peer);
}

static void set_internal_call_allowed_codecs_list(struct sip_pvt *p)
{
	char buf[512];

	ast_codec_pref_dump(buf, sizeof(buf), &sip_proxy_internal_call_prefs);
    ast_log(LOG_DEBUG, "SIP proxy internal call allowed codecs: %s\n",  buf);

	ast_codec_pref_dump(buf, sizeof(buf), &p->formats);
    ast_log(LOG_DEBUG, "Current allowed formats: %s\n",  buf);

	ast_codec_pref_combine(&p->formats, &sip_proxy_internal_call_prefs, p->usercapability);

	ast_codec_pref_dump(buf, sizeof(buf), &p->formats);
    ast_log(LOG_DEBUG, 
		"Allowed formats combined with SIP proxy internal call allowed formats: %s\n",  buf);
}

/*! \brief  sip_call: Initiate SIP call from PBX 
 *      used from the dial() application      */
static int sip_call(struct ast_channel *ast, char *dest, int timeout)
{
	int res;
	struct sip_pvt *p;
#ifdef OSP_SUPPORT
	char *osphandle = NULL;
#endif	
	struct varshead *headp;
	struct ast_var_t *current;
	struct sip_peer *peer;

	
	p = ast->tech_pvt;
	if ((ast->_state != AST_STATE_DOWN) && (ast->_state != AST_STATE_RESERVED)) {
		ast_log(LOG_WARNING, "sip_call called on %s, neither down nor reserved\n", ast->name);
		return -1;
	}
	ast_mutex_lock(&iflock);
	ast_mutex_lock(&p->lock);

	/* Check whether there is vxml_url, distinctive ring variables */

	headp=&ast->varshead;
	AST_LIST_TRAVERSE(headp,current,entries) {
		/* Check whether there is a VXML_URL variable */
		if (!p->options->vxml_url && !strcasecmp(ast_var_name(current), "VXML_URL")) {
			p->options->vxml_url = ast_var_value(current);
               } else if (!p->options->uri_options && !strcasecmp(ast_var_name(current), "SIP_URI_OPTIONS")) {
                       p->options->uri_options = ast_var_value(current);
		} else if (!p->options->distinctive_ring && !strcasecmp(ast_var_name(current), "ALERT_INFO")) {
			/* Check whether there is a ALERT_INFO variable */
			p->options->distinctive_ring = ast_var_value(current);
		} else if (!p->options->addsipheaders && !strncasecmp(ast_var_name(current), "SIPADDHEADER", strlen("SIPADDHEADER"))) {
			/* Check whether there is a variable with a name starting with SIPADDHEADER */
			p->options->addsipheaders = 1;
#if defined(T38_SUPPORT)
		} else if (!strncasecmp(ast_var_name(current), "T38CALL", strlen("T38CALL"))) {
			/* Check whether there is a variable with a name starting with T38CALL */
			p->t38state = T38_LOCAL_DIRECT;
			ast_log(LOG_DEBUG,"T38State change to %d on channel %s\n",p->t38state, ast->name);
#endif
		} else if (!strncasecmp(ast_var_name(current),
			"EXT_CALLERID_RESTRICTED", strlen("EXT_CALLERID_RESTRICTED"))) {
			p->clir = 1;
		}

		
#ifdef OSP_SUPPORT
		else if (!p->options->osptoken && !strcasecmp(ast_var_name(current), "OSPTOKEN")) {
			p->options->osptoken = ast_var_value(current);
		} else if (!osphandle && !strcasecmp(ast_var_name(current), "OSPHANDLE")) {
			osphandle = ast_var_value(current);
		}
#endif
	}
	
	res = 0;
	ast_set_flag(p, SIP_OUTGOING);
#ifdef OSP_SUPPORT
	if (!p->options->osptoken || !osphandle || (sscanf(osphandle, "%d", &p->osphandle) != 1)) {
		/* Force Disable OSP support */
		ast_log(LOG_DEBUG, "Disabling OSP support for this call. osptoken = %s, osphandle = %s\n", p->options->osptoken, osphandle);
		p->options->osptoken = NULL;
		osphandle = NULL;
		p->osphandle = -1;
	}
#endif
	ast_log(LOG_DEBUG, "Outgoing Call for %s\n", p->username);
	res = update_call_counter(p, INC_CALL_LIMIT);
	if ( res != -1 ) {
		p->callingpres = ast->cid.cid_pres;

		ast_mutex_lock(&bw_mgt_lock);

		peer = find_peer(p->peername, NULL, 1);
		if (peer && peer->bw_mgt)
		{
			p->usercapability = find_fitting_codecs(peer, p);
			ast_log(LOG_DEBUG, "[%s:%s] pvt->usercap=%#x\n",
				p->callid, p->peername, p->usercapability);
			/* Didn't find any codec that fit the available upstream. */
			if (!p->usercapability)
			{
				ast_mutex_unlock(&bw_mgt_lock);
				ast_mutex_unlock(&p->lock);
				ast_mutex_unlock(&iflock);
				ast_log(LOG_WARNING, "Failed to place call for user %s, no available upstream "
					"exists\n", p->username);
				ast_set_flag(p, SIP_NEEDDESTROY);
				ASTOBJ_UNREF(peer, sip_destroy_peer);
				return -1;
			}
			bw_mgt_alloc_bw(p, peer, get_minmax_audio_bw(p->usercapability, 1));
			ast_codec_pref_set2(&p->formats, p->usercapability);
		}

		if (peer)
		    ASTOBJ_UNREF(peer, sip_destroy_peer);
		ast_mutex_unlock(&bw_mgt_lock);

#if defined(T38_SUPPORT)
		p->t38jointcapability = p->t38capability;
		ast_log(LOG_DEBUG,"Our T38 capability (%d), joint T38 capability (%d)\n", p->t38capability, p->t38jointcapability);
#endif
		struct sip_user *u0 = NULL, *u1 = NULL;
            
		if ((u0 = find_user(p->username, 1)) && p->owner && p->owner->cid.cid_num &&
			(u1 = find_user(p->owner->cid.cid_num, 1)))
		{
			ast_log(LOG_DEBUG, "This is a sip proxy internal call\n");
			set_internal_call_allowed_codecs_list(p);
		}
		if (u0)
		    ASTOBJ_UNREF(u0,sip_destroy_user);
		if (u1)
			ASTOBJ_UNREF(u1,sip_destroy_user);

		transmit_invite(p, SIP_INVITE, 1, 2);
		if (p->maxtime) {
			/* Initialize auto-congest time */
			p->initid = ast_sched_add(sched, p->maxtime * 4, auto_congest, p);
		}
	}

	ast_mutex_unlock(&p->lock);
	ast_mutex_unlock(&iflock);

	return res;
}

/*--- sip_subscription_destroy: Destroy subscription object ---*/
/*	Objects created with the subscribe=> statement */
static void sip_subscription_destroy(struct sip_subscription *sub)
{
	/* Really delete */
	if (sub->call) {
		/* Clear subscription before destroying to ensure
		   we don't get reentered trying to grab the subscription lock */
		sub->call->subscription = NULL;
		sip_destroy(sub->call);
	}
	if (sub->expire > -1)
		ast_sched_del(sched, sub->expire);
	if (sub->timeout > -1)
		ast_sched_del(sched, sub->timeout);
	subobjs--;
	free(sub);
}

/*! \brief  sip_registry_destroy: Destroy registry object ---*/
/*	Objects created with the register= statement in static configuration */
static void sip_registry_destroy(struct sip_registry *reg)
{
	/* Really delete */
	if (reg->call) {
		/* Clear registry before destroying to ensure
		   we don't get reentered trying to grab the registry lock */
		reg->call->registry = NULL;
		sip_destroy(reg->call);
	}
	if (reg->expire > -1)
		ast_sched_del(sched, reg->expire);
	if (reg->timeout > -1)
		ast_sched_del(sched, reg->timeout);
	if (reg->sched_recover_primary > -1)
	    ast_sched_del(sched, reg->sched_recover_primary);
	if (reg->sched_failover_delay > -1)
	    ast_sched_del(sched, reg->sched_failover_delay);

	/* If I have a backup proxy... */
	if (reg->reg_backup) {
	    reg->reg_backup->reg_primary = NULL;
	    ASTOBJ_UNREF(reg->reg_backup, sip_registry_destroy);
	    reg->reg_backup = NULL;
	} else if (reg->reg_primary)
	{
	    /* If I am a backup proxy */
	    reg->reg_primary = NULL;
	}

	if (reg->srv_failover)
		ast_srv_cleanup(&reg->uas_srv.context);

	if (reg->contact_list)
		contact_list_free(&reg->contact_list);

	regobjs--;
	free(reg);
	
}

static void print_debug_stats(struct sip_peer *peer)
{
	char msg[256];

	snprintf(msg, sizeof(msg), "RxPackets: %d, TxPackets: %d, "
		"RxOctets: %d, TxOctets: %d, RxPacketsLost: %d",
		peer->rtp_stats.rx_packets, peer->rtp_stats.tx_packets, peer->rtp_stats.rx_octets,
		peer->rtp_stats.tx_octets, peer->rtp_stats.rx_packets_lost);

	ast_log(LOG_DEBUG, "Stats for peer - %s\n", msg);
}

static void notify_manager_rtp_save(struct sip_peer *peer)
{
    manager_event(EVENT_FLAG_CALL, "SaveRtpStats",
	"Username: %s\r\n"
	"AverageRoundTripDelay: %d\r\n"
	"FarEndPacketLossRateSum: %d\r\n"
	"ReceivePacketLossRateSum: %d\r\n"
	"ReceivedRtcpCount: %d\r\n"
	"SentRtcpCount: %d\r\n",peer->username,
	peer->rtp_stats.avg_round_trip_delay,
	peer->rtp_stats.tx_fraction_sum,
	peer->rtp_stats.rx_fraction_sum,
	peer->rtp_stats.num_of_rtcp_received,
	peer->rtp_stats.num_of_rtcp_sent);
}

/*! \brief   __sip_destroy: Execute destrucion of call structure, release memory---*/
static void __sip_destroy(struct sip_pvt *p, int lockowner)
{
	struct sip_pvt *cur, *prev = NULL;
	struct sip_pkt *cp;
	struct sip_history *hist;
	struct sip_peer *peer = find_peer(p->peername, NULL,0);

	if (sip_debug_test_pvt(p))
		ast_verbose("Destroying call '%s'\n", p->callid);

	if (p->DTMFschedid > -1) {
		ast_sched_del(sched, p->DTMFschedid);
		p->DTMFschedid = -1;
	}
	
	if (dumphistory)
		sip_dump_history(p);

	if (p->options)
	{
		free(p->options);
		p->options = NULL;
	}

	if (p->stateid > -1)
		ast_extension_state_del(p->stateid, NULL);
	if (p->waitid > -1)
	{
		ast_sched_del(sched, p->waitid);
		p->waitid = -1;
	}
	if (p->initid > -1)
		ast_sched_del(sched, p->initid);
	if (p->autokillid > -1)
		ast_sched_del(sched, p->autokillid);

	if (p->rtp) 
	{
		if(peer)
		{
			rtp_stats_accumulate(&peer->rtp_stats, p->rtp);
			notify_manager_rtp_save(peer);
			print_debug_stats(peer);
		}
		ast_rtp_destroy(p->rtp);
		p->rtp = NULL;
	}
	if (peer)
	{
		ast_mutex_lock(&bw_mgt_lock);
		bw_mgt_free_bw(p, peer);
		ast_mutex_unlock(&bw_mgt_lock);
	    ASTOBJ_UNREF(peer, sip_destroy_peer);
	}

	if (p->vrtp) {
		ast_rtp_destroy(p->vrtp);
		p->vrtp = NULL;
	}
#if defined(T38_SUPPORT)
	if (p->udptl) {
		ast_udptl_destroy(p->udptl);
		p->udptl = NULL;
	}
#endif
	if (p->route) {
		free_old_route(p->route);
		p->route = NULL;
	}
	if (p->registry) {
		if (p->registry->call == p)
			p->registry->call = NULL;
		ASTOBJ_UNREF(p->registry,sip_registry_destroy);
	}
	if (p->subscription) {
	    	if (p->subscription->call == p)
		    	p->subscription->call = NULL;
		ASTOBJ_UNREF(p->subscription,sip_subscription_destroy);
	}

	if (p->rpid)
		free(p->rpid);

	if (p->rpid_from)
		free(p->rpid_from);

	/* Destroy Session-Timers if allocated */
	if (p->stimer) {
		if (p->stimer->st_schedid > -1)
			ast_sched_del(sched, p->stimer->st_schedid);
		free(p->stimer);
		p->stimer = NULL;
	}

	/* Unlink us from the owner if we have one */
	if (p->owner) {
		if (lockowner)
			ast_mutex_lock(&p->owner->lock);
		ast_log(LOG_DEBUG, "Detaching from %s\n", p->owner->name);
		p->owner->tech_pvt = NULL;
		if (lockowner)
			ast_mutex_unlock(&p->owner->lock);
	}
	/* Clear history */
	while(p->history) {
		hist = p->history;
		p->history = p->history->next;
		free(hist);
	}

	cur = iflist;
	while(cur) {
		if (cur == p) {
			if (prev)
				prev->next = cur->next;
			else
				iflist = cur->next;
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
	    ASTOBJ_RDLOCK(iterator);
	    if (iterator->call == p)
		iterator->call = NULL;
	    ASTOBJ_UNLOCK(iterator);
	} while (0));

	if (!cur) {
		ast_log(LOG_WARNING, "Trying to destroy \"%s\", not found in dialog list?!?! \n", p->callid);
		return;
	} 
	if (p->initid > -1)
		ast_sched_del(sched, p->initid);

	while((cp = p->packets)) {
		p->packets = p->packets->next;
		if (cp->retransid > -1) {
			ast_sched_del(sched, cp->retransid);
		}
		free(cp);
	}
	if (p->chanvars) {
		ast_variables_destroy(p->chanvars);
		p->chanvars = NULL;
	}
	ast_mutex_destroy(&p->lock);
	free(p);
}

/*! \brief  update_call_counter: Handle call_limit for SIP users 
 * Note: This is going to be replaced by app_groupcount 
 * Thought: For realtime, we should propably update storage with inuse counter... */
static int update_call_counter(struct sip_pvt *fup, int event)
{
	char name[256];
	int *inuse, *call_limit;
	int outgoing = ast_test_flag(fup, SIP_OUTGOING);
	struct sip_user *u = NULL;
	struct sip_peer *p = NULL;

	if (option_debug > 2)
		ast_log(LOG_DEBUG, "Updating call counter for %s call\n", outgoing ? "outgoing" : "incoming");
	/* Test if we need to check call limits, in order to avoid 
	   realtime lookups if we do not need it */
	if (!ast_test_flag(fup, SIP_CALL_LIMIT))
		return 0;

	ast_copy_string(name, fup->username, sizeof(name));

	/* Check the list of users */
	u = find_user(name, 1);
	if (u) {
		inuse = &u->inUse;
		call_limit = &u->call_limit;
		p = NULL;
	} else {
		/* Try to find peer */
		if (!p)
			p = find_peer(fup->peername, NULL, 1);
		if (p) {
			inuse = &p->inUse;
			call_limit = &p->call_limit;
			ast_copy_string(name, fup->peername, sizeof(name));
		} else {
			if (option_debug > 1)
				ast_log(LOG_DEBUG, "%s is not a local user, no call limit\n", name);
			return 0;
		}
	}
	switch(event) {
		/* incoming and outgoing affects the inUse counter */
		case DEC_CALL_LIMIT:
			if ( *inuse > 0 ) {
			         if (ast_test_flag(fup,SIP_INC_COUNT))
				         (*inuse)--;
			} else {
				*inuse = 0;
			}
			if (option_debug > 1 || sipdebug) {
				ast_log(LOG_DEBUG, "Call %s %s '%s' removed from call limit %d\n", outgoing ? "to" : "from", u ? "user":"peer", name, *call_limit);
			}
			break;
		case INC_CALL_LIMIT:
			if (*call_limit > 0 ) {
				if (*inuse >= *call_limit) {
					ast_log(LOG_ERROR, "Call %s %s '%s' rejected due to usage limit of %d\n", outgoing ? "to" : "from", u ? "user":"peer", name, *call_limit);
					if (u)
						ASTOBJ_UNREF(u,sip_destroy_user);
					else
						ASTOBJ_UNREF(p,sip_destroy_peer);
					return -1; 
				}
			}
			(*inuse)++;
	                ast_set_flag(fup,SIP_INC_COUNT);
			if (option_debug > 1 || sipdebug) {
				ast_log(LOG_DEBUG, "Call %s %s '%s' is %d out of %d\n", outgoing ? "to" : "from", u ? "user":"peer", name, *inuse, *call_limit);
			}
			break;
		default:
			ast_log(LOG_ERROR, "update_call_counter(%s, %d) called with no event!\n", name, event);
	}
	if (u)
		ASTOBJ_UNREF(u,sip_destroy_user);
	else
		ASTOBJ_UNREF(p,sip_destroy_peer);
	return 0;
}

/*! \brief  sip_destroy: Destroy SIP call structure ---*/
static void sip_destroy(struct sip_pvt *p)
{
	ast_mutex_lock(&iflock);
	__sip_destroy(p, 1);
	ast_mutex_unlock(&iflock);
}


static int transmit_response_reliable(struct sip_pvt *p, char *msg, struct sip_request *req, int fatal);

/*! \brief  hangup_sip2cause: Convert SIP hangup causes to Asterisk hangup causes ---*/
static int hangup_sip2cause(int cause)
{
/* Possible values taken from causes.h */

	switch(cause) {
		case 603:	/* Declined */
			return AST_CAUSE_CALL_DECLINED;
		case 403:	/* Not found */
		case 487:	/* Call cancelled */
			return AST_CAUSE_CALL_REJECTED;
		case 404:	/* Not found */
			return AST_CAUSE_UNALLOCATED;
		case 408:	/* No reaction */
			return AST_CAUSE_NO_USER_RESPONSE;
		case 480:	/* No answer */
			return AST_CAUSE_FAILURE;
		case 483:	/* Too many hops */
			return AST_CAUSE_NO_ANSWER;
		case 486:	/* Busy everywhere */
			return AST_CAUSE_BUSY;
		case 488:	/* No codecs approved */
			return AST_CAUSE_BEARERCAPABILITY_NOTAVAIL;
		case 500:	/* Server internal failure */
			return AST_CAUSE_FAILURE;
		case 501:	/* Call rejected */
			return AST_CAUSE_FACILITY_REJECTED;
		case 502:	
			return AST_CAUSE_DESTINATION_OUT_OF_ORDER;
		case 503:	/* Service unavailable */
			return AST_CAUSE_CONGESTION;
		default:
			return AST_CAUSE_NORMAL;
	}
	/* Never reached */
	return 0;
}


/*! \brief  hangup_cause2sip: Convert Asterisk hangup causes to SIP codes 
\verbatim
 Possible values from causes.h
        AST_CAUSE_NOTDEFINED    AST_CAUSE_NORMAL        AST_CAUSE_BUSY
        AST_CAUSE_FAILURE       AST_CAUSE_CONGESTION    AST_CAUSE_UNALLOCATED

	In addition to these, a lot of PRI codes is defined in causes.h 
	...should we take care of them too ?
	
	Quote RFC 3398

   ISUP Cause value                        SIP response
   ----------------                        ------------
   1  unallocated number                   404 Not Found
   2  no route to network                  404 Not found
   3  no route to destination              404 Not found
   16 normal call clearing                 --- (*)
   17 user busy                            486 Busy here
   18 no user responding                   408 Request Timeout
   19 no answer from the user              480 Temporarily unavailable
   20 subscriber absent                    480 Temporarily unavailable
   21 call rejected                        403 Forbidden (+)
   22 number changed (w/o diagnostic)      410 Gone
   22 number changed (w/ diagnostic)       301 Moved Permanently
   23 redirection to new destination       410 Gone
   26 non-selected user clearing           404 Not Found (=)
   27 destination out of order             502 Bad Gateway
   28 address incomplete                   484 Address incomplete
   29 facility rejected                    501 Not implemented
   31 normal unspecified                   480 Temporarily unavailable
\endverbatim
*/
static char *hangup_cause2sip(int cause)
{
	switch(cause)
	{
		case AST_CAUSE_UNALLOCATED:		/* 1 */
		case AST_CAUSE_NO_ROUTE_DESTINATION:	/* 3 IAX2: Can't find extension in context */
		case AST_CAUSE_NO_ROUTE_TRANSIT_NET:	/* 2 */
			return "404 Not Found";
                case AST_CAUSE_CONGESTION:		/* 34 */
                case AST_CAUSE_SWITCH_CONGESTION:	/* 42 */
                        return "503 Service Unavailable";
                case AST_CAUSE_CALL_LIMIT:		/* 35 */
			return "480 Temporarily Unavailable (Call limit)";
		case AST_CAUSE_NO_USER_RESPONSE:	/* 18 */
			return "408 Request Timeout";
		case AST_CAUSE_NO_ANSWER:		/* 19 */
			return "480 Temporarily unavailable";
		case AST_CAUSE_CALL_REJECTED:		/* 21 */
			return "403 Forbidden";
		case AST_CAUSE_CALL_DECLINED:		/* 128 */
			return "603 Decline";
		case AST_CAUSE_NUMBER_CHANGED:		/* 22 */
			return "410 Gone";
		case AST_CAUSE_NORMAL_UNSPECIFIED:	/* 31 */
			return "480 Temporarily unavailable";
		case AST_CAUSE_INVALID_NUMBER_FORMAT:
			return "484 Address incomplete";
		case AST_CAUSE_USER_BUSY:
			return "486 Busy here";
		case AST_CAUSE_FAILURE:
                	return "500 Server internal failure";
		case AST_CAUSE_FACILITY_REJECTED:	/* 29 */
			return "501 Not Implemented";
		case AST_CAUSE_CHAN_NOT_IMPLEMENTED:
			return "503 Service Unavailable";
		/* Used in chan_iax2 */
		case AST_CAUSE_DESTINATION_OUT_OF_ORDER:
			return "502 Bad Gateway";
		case AST_CAUSE_BEARERCAPABILITY_NOTAVAIL:	/* Can't find codec to connect to host */
			return "488 Not Acceptable Here";
			
		case AST_CAUSE_NOTDEFINED:
		default:
			ast_log(LOG_DEBUG, "AST hangup cause %d (no match found in SIP)\n", cause);
			return NULL;
	}

	/* Never reached */
	return 0;
}

static void sip_alloc_quality(struct sip_pvt *p)
{
	ast_mutex_lock(&quality_lock);
	if (global_highqualitycalls.max > -1)
	{
		if (p->peer_quality)
			ast_log(LOG_NOTICE, "Quality for the call %s already allocated\n", p->from);

		if (global_highqualitycalls.current < global_highqualitycalls.max)
			p->quality_meter = MAX_QUALITY;
		else
		{
			struct sip_pvt *pvt;

			for (pvt = iflist; pvt; pvt = pvt->next)
			{
				if (!pvt->quality_meter || pvt->owner->_state != AST_STATE_UP ||
			    		pvt->pendinginvite || !pvt->owner
#if defined(T38_SUPPORT)
			    		|| pvt->t38state != T38_DISABLED
#endif
			   	)
				{
			        	continue;
				}

				ast_mutex_lock(&pvt->lock);

				pvt->quality_meter = 0;

				memcpy(&pvt->formats, &global_prefs, sizeof(global_prefs));
				ast_codec_pref_set_top_quality(&pvt->formats, pvt->quality_meter);

				transmit_reinvite_with_sdp(pvt, FALSE);

				ast_log(LOG_DEBUG, "Peer %s quality alloc max=%d, curr=%d, meter=%d\n",
			    		pvt->peername, global_highqualitycalls.max, global_highqualitycalls.current, pvt->quality_meter);

				ast_mutex_unlock(&pvt->lock);
			}
		}
		global_highqualitycalls.current++;

		p->peer_quality = &global_highqualitycalls;
		ast_log(LOG_DEBUG, "format quality:%d\n", p->formats.audio_bits);
		memcpy(&p->formats, &global_prefs, sizeof(global_prefs));
		ast_codec_pref_set_top_quality(&p->formats,p->quality_meter);

		ast_log(LOG_DEBUG, "Peer %s quality alloc max=%d, curr=%d, meter=%d\n",
			p->peername, global_highqualitycalls.max, global_highqualitycalls.current, p->quality_meter);
	}
	else
	{
		p->quality_meter = MAX_QUALITY;
		ast_log(LOG_DEBUG, "Peer %s won't be checked for quality\n", p->peername);
	}
	ast_mutex_unlock(&quality_lock);
}

static void sip_realloc_quality(struct sip_pvt *p, ast_codec_quality new_q)
{
	struct sip_peer_quality *qty;
	ast_codec_quality diff;

	ast_mutex_lock(&quality_lock);
	diff = new_q - p->quality_meter;
	qty = p->peer_quality;
	if (qty && (diff < 0 || (qty->current + diff < qty->max)))
	{
		if (diff < 0)
			ast_codec_pref_set_top_quality(&p->formats, p->quality_meter);
		/* else we could add additional codecs, but such a case should happen
		 * only when sending fax and there's additional quality available,
		 * in that scenario the codec will be added manually */
		p->quality_meter = new_q;
		ast_log(LOG_DEBUG, "Call %s quality metering has changed from %d to %d\n", 
			p->from, new_q - diff, new_q);
	}
	else
		ast_log(LOG_DEBUG, "Call %s quality metering %s\n", p->from, qty ? "hasn't changed" : "is disabled");
	ast_mutex_unlock(&quality_lock);
}

static void sip_free_quality(struct sip_pvt *p)
{
	ast_mutex_lock(&quality_lock);
	if (p->peer_quality)
	{
		struct sip_peer_quality *qty = p->peer_quality;

		qty->current--;
		p->peer_quality = NULL;

		ast_log(LOG_DEBUG, "Call %s quality free max=%d, curr=%d, meter=%d\n",
			p->from, qty->max, qty->current, p->quality_meter);
	}
	else
		ast_log(LOG_DEBUG, "Call %s don't have quality metering\n", p->from);

	p->quality_meter = 0;
	ast_mutex_unlock(&quality_lock);
}

static void sip_pre_bridge(struct ast_channel *chan)
{
	struct sip_pvt *p = chan->tech_pvt;

	sip_realloc_quality(p, ast_codec_get_quality(p->owner->writeformat));
}

static int get_active_calls_count_on_proxy(struct sip_registry *reg)
{
	struct sip_pvt *sip;
	int calls_count = 0;

	ast_mutex_lock(&iflock);
	sip = iflist;
	while(sip)
	{
		ast_mutex_lock(&sip->lock);
		if (sip->owner) //we have owner if this is a call session
		{
			/* if the call is on backup proxy */
			if (!ast_strlen_zero(sip->regname) && 
				!strcmp(reg->name, sip->regname))
			{
				calls_count++;
			}
		}
		ast_mutex_unlock(&sip->lock);
		sip = sip->next;
	}
	ast_mutex_unlock(&iflock);
	return calls_count;
}

static int sip_expire_redoregister(void *data);

/*! \brief  sip_hangup: Hangup SIP call 
 * Part of PBX interface, called from ast_hangup */
static int sip_hangup(struct ast_channel *ast)
{
	struct sip_pvt *p = ast->tech_pvt;
	int needcancel = 0;
	struct ast_flags locflags = {0};
	struct sip_registry *reg;
	int active_calls_count = 0;

	if (!p) {
		ast_log(LOG_DEBUG, "Asked to hangup channel not connected\n");
		return 0;
	}
	if (option_debug)
		ast_log(LOG_DEBUG, "Hangup call %s, SIP callid %s)\n", ast->name, p->callid);
	if (ast_test_flag(ast, AST_FLAG_ANSWERED_ELSEWHERE)) {
		if (option_debug)
			ast_log(LOG_DEBUG, "This call was answered elsewhere");
		p->sip_hangupcause = 200;
		strncpy(p->sip_hanguptext, "Call answered elsewhere",sizeof(p->sip_hanguptext) );
		append_history(p, "Cancel", p->sip_hanguptext);
	}

	ast_mutex_lock(&p->lock);
#ifdef OSP_SUPPORT
	if ((p->osphandle > -1) && (ast->_state == AST_STATE_UP)) {
		ast_osp_terminate(p->osphandle, AST_CAUSE_NORMAL, p->ospstart, time(NULL) - p->ospstart);
	}
#endif	
	ast_log(LOG_DEBUG, "update_call_counter(%s) - decrement call limit counter\n", p->username);
	sip_free_quality(p);
	update_call_counter(p, DEC_CALL_LIMIT);
	/* Determine how to disconnect */
	if (p->owner != ast) {
		ast_log(LOG_WARNING, "Huh?  We aren't the owner? Can't hangup call.\n");
		ast_mutex_unlock(&p->lock);
		return 0;
	}
	/* If the call is not UP, we need to send CANCEL instead of BYE */
	if (ast->_state != AST_STATE_UP)
		needcancel = 1;

	/* Disconnect */
	p = ast->tech_pvt;
	if (p->vad) {
		ast_dsp_free(p->vad);
	}

	p->q850_hangupcause = p->owner->hangupcause;

	

	ast_mutex_lock(&usecnt_lock);
	usecnt--;
	ast_mutex_unlock(&usecnt_lock);
	ast_update_use_count();

	ast_set_flag(&locflags, SIP_NEEDDESTROY);	
	manager_event(EVENT_FLAG_SYSTEM, "SessionChanged", 
	    "Established: 0\r\n"
	    "Peer: %s\r\n",
	    p->peername);

	/* Start the process if it's not already started */
	if (!ast_test_flag(p, SIP_ALREADYGONE) && !ast_strlen_zero(p->initreq.data)) {
		if (needcancel) {	/* Outgoing call, not up */
			if (ast_test_flag(p, SIP_OUTGOING)) {
				/* stop retransmitting an INVITE that has not received a response */
				__sip_pretend_semi_ack(p);

				transmit_request_with_auth(p, SIP_CANCEL, p->lastinvite, 1, 0);
				/* Actually don't destroy us yet, wait for the 487 on our original 
				   INVITE, but do set an autodestruct just in case we never get it. */
				ast_set_flag(&p->flags_page2, SIP_PAGE2_INVITECANCELLED);
				ast_clear_flag(&locflags, SIP_NEEDDESTROY);
				sip_scheddestroy(p, 32000);
				if ( p->initid != -1 ) {
					/* channel still up - reverse dec of inUse counter
					   only if the channel is not auto-congested */
					update_call_counter(p, INC_CALL_LIMIT);
				}
			} else {	/* Incoming call, not up */
				char *res;
				if (ast->hangupcause && ((res = hangup_cause2sip(ast->hangupcause)))) {
					transmit_response_reliable(p, res, &p->initreq, 1);
				} else 
					transmit_response_reliable(p, "603 Declined", &p->initreq, 1);
			}
		} else {	/* Call is in UP state, send BYE */
			if (!p->pendinginvite) {
				/* Send a hangup */
				transmit_request_with_auth(p, SIP_BYE, 0, 1, 1);
			} else {
				/* Note we will need a BYE when this all settles out
				   but we can't send one while we have "INVITE" outstanding. */
				ast_set_flag(p, SIP_PENDINGBYE);	
				ast_clear_flag(p, SIP_NEEDREINVITE);	
				if (p->waitid > -1) {
					ast_sched_del(sched, p->waitid);
					p->waitid = -1;
				}
			}
		}
	}
	
	p->owner = NULL;
	ast->tech_pvt = NULL;

	ast_copy_flags(p, (&locflags), SIP_NEEDDESTROY);	

	if (ast->_state == AST_STATE_UP)
		ast_rtp_stats_logging_stop(p->rtp, p->is_cli_hangup);

	reg = ASTOBJ_CONTAINER_FIND(&regl, p->regname);
	ast_mutex_unlock(&p->lock);
	if (!reg)
	{
		ast_log(LOG_DEBUG, "could not find registry\n");
		return 0;
	}
	/* Handle proxy failover if needed */
	if (reg->reg_primary && reg->need_recover_to_primary)
	{
		active_calls_count = get_active_calls_count_on_proxy(reg);

		/* if no more calls are on this proxy, try to recover to primary */
		if (active_calls_count == 0)
		{
			/* start recovery*/
			ast_log(LOG_DEBUG, "No more active calls on backup proxy. Try to "
				"recover primary proxy\n");

			reg->need_recover_to_primary = 0;
			/* do register to primary*/
			sip_expire_redoregister(reg->reg_primary);
		}
		else
		{
			ast_log(LOG_DEBUG, "there are %d more active calls on proxy %s\n",
				active_calls_count, reg->name);
		}
	}
	ASTOBJ_UNREF(reg, sip_registry_destroy);
	return 0;
}

/*! \brief Try setting codec suggested by the SIP_CODEC channel variable */
static void try_suggested_sip_codec(struct sip_pvt *p)
{
	int fmt;
	char *codec;

	codec = pbx_builtin_getvar_helper(p->owner, "SIP_CODEC");
	if (!codec) 
		return;

	fmt = ast_getformatbyname(codec);
	if (fmt) {
		if (fmt & p->usercapability)
		{
			ast_log(LOG_NOTICE, "Changing codec to '%s' for this call because of ${SIP_CODEC) variable\n",codec);
			if (ast_codec_pref_bits(&p->formats) & fmt) {
				ast_codec_pref_set2(&p->formats, fmt);
				if (p->owner) {
					memcpy(&p->owner->nativeformats, &p->formats, sizeof(p->formats));
				}
			} else
				ast_log(LOG_NOTICE, "Ignoring ${SIP_CODEC} variable because it is not shared by both ends.\n");
		} else
			ast_log(LOG_NOTICE, "Ignoring ${SIP_CODEC} variable because codec %s is disallowed by user preferences.\n", codec);
	} else
		ast_log(LOG_NOTICE, "Ignoring ${SIP_CODEC} variable because of unrecognized/not configured codec (check allow/disallow in sip.conf): %s\n",codec);
	return;	
}

/*! \brief  sip_answer: Answer SIP call , send 200 OK on Invite 
 * Part of PBX interface */
static int sip_answer(struct ast_channel *ast)
{
	int res = 0;
	struct sip_pvt *p = ast->tech_pvt;

	ast_mutex_lock(&p->lock);
	if (ast->_state != AST_STATE_UP) {
#ifdef OSP_SUPPORT	
		time(&p->ospstart);
#endif
		try_suggested_sip_codec(p);	
	
		ast_setstate(ast, AST_STATE_UP);
		if (option_debug)
			ast_log(LOG_DEBUG, "sip_answer(%s)\n", ast->name);
#if defined(T38_SUPPORT)
		if (p->t38state == T38_PEER_DIRECT) {
			p->t38state=T38_ENABLED;
			ast_log(LOG_DEBUG,"T38State change to %d on channel %s\n",p->t38state, ast->name);
			res = transmit_response_with_t38_sdp(p, "200 OK", &p->initreq, 1, FALSE);
		} else
#endif
		{
		    	int is_sdp_disabled = 0;

			if (ast->answer_data)
			    is_sdp_disabled = *(int *)ast->answer_data;
			if (is_sdp_disabled)
 				res = transmit_response(p, "200 OK", &p->initreq);
			else
	 			res = transmit_response_with_sdp(p, "200 OK", &p->initreq, 1, FALSE);
			manager_event(EVENT_FLAG_SYSTEM, "SessionChanged", 
			    "Established: 1\r\n"
			    "Peer: %s\r\n",
			    p->peername);
		}

		ast_set_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
                last_answered_call_timestamp = get_time_from_boot();

		ast_rtp_stats_logging_start(p->rtp, 0, ast_strlen_zero(p->cid_num) ? "Hidden" : p->cid_num);
	}
	ast_mutex_unlock(&p->lock);
	return res;
}

/*! \brief  sip_write: Send frame to media channel (rtp) ---*/
static int sip_write(struct ast_channel *ast, struct ast_frame *frame)
{
	struct sip_pvt *p = ast->tech_pvt;
	int res = 0;
	switch (frame->frametype) {
	case AST_FRAME_VOICE:
		if (!(frame->subclass & ast->nativeformats.audio_bits)) {
			ast_log(LOG_WARNING, "Asked to transmit frame type %d, while native formats is %d (read/write = %d/%d)\n",
				frame->subclass, ast->nativeformats.audio_bits, ast->readformat, ast->writeformat);
			return 0;
		}
		if (p) {
			ast_mutex_lock(&p->lock);
			if (p->rtp) {
				/* If channel is not up, activate early media session */
				if ((ast->_state != AST_STATE_UP) && !ast_test_flag(p, SIP_PROGRESS_SENT) && !ast_test_flag(p, SIP_OUTGOING)) {
					int compatible_codecs = ast_compatible_audio_formats(frame->subclass) | ast->nativeformats.video_bits;
					ast_codec_pref_remove2(&ast->nativeformats, ~compatible_codecs);
					ast_codec_pref_remove2(&p->formats, ~compatible_codecs);
					transmit_response_with_sdp(p, "183 Session Progress", &p->initreq, 0, FALSE);
					ast_set_flag(p, SIP_PROGRESS_SENT);	
				}
				time(&p->lastrtptx);
				res =  ast_rtp_write(p->rtp, frame);
			}
			ast_mutex_unlock(&p->lock);
		}
		break;
	case AST_FRAME_VIDEO:
		if (p) {
			ast_mutex_lock(&p->lock);
			if (p->vrtp) {
				/* Activate video early media */
				if ((ast->_state != AST_STATE_UP) && !ast_test_flag(p, SIP_PROGRESS_SENT) && !ast_test_flag(p, SIP_OUTGOING)) {
					transmit_response_with_sdp(p, "183 Session Progress", &p->initreq, 0, FALSE);
					ast_set_flag(p, SIP_PROGRESS_SENT);	
				}
				time(&p->lastrtptx);
				res =  ast_rtp_write(p->vrtp, frame);
			}
			ast_mutex_unlock(&p->lock);
		}
		break;
	case AST_FRAME_IMAGE:
		return 0;
		break;
#if defined(T38_SUPPORT)
	case AST_FRAME_MODEM:
		if (p) {
			ast_mutex_lock(&p->lock);
			if (p->udptl) {
				if ((ast->_state != AST_STATE_UP) && !ast_test_flag(p, SIP_PROGRESS_SENT) && !ast_test_flag(p, SIP_OUTGOING)) {
					transmit_response_with_t38_sdp(p, "183 Session Progress", &p->initreq, 0, FALSE);
					ast_set_flag(p, SIP_PROGRESS_SENT);	
				}
				res = ast_udptl_write(p->udptl, frame);
			}
			ast_mutex_unlock(&p->lock);
		}
		break;
#endif
	default: 
		ast_log(LOG_WARNING, "Can't send %d type frames with SIP write\n", frame->frametype);
		return 0;
	}

	return res;
}

/*! \brief  sip_fixup: Fix up a channel:  If a channel is consumed, this is called.
        Basically update any ->owner links ----*/
static int sip_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct sip_pvt *p = newchan->tech_pvt;
	ast_mutex_lock(&p->lock);
	if (p->owner != oldchan) {
		ast_log(LOG_WARNING, "old channel wasn't %p but was %p\n", oldchan, p->owner);
		ast_mutex_unlock(&p->lock);
		return -1;
	}
	p->owner = newchan;
	if (p->ptime)
	{
		p->owner->ptime = p->ptime;
		p->ptime = 0;
	}
	ast_mutex_unlock(&p->lock);
	return 0;
}

/*! \brief  sip_senddigit: Send DTMF character on SIP channel */
/*    within one call, we're able to transmit in many methods simultaneously */
static int sip_senddigit_begin(struct ast_channel *ast, char digit)
{
	struct sip_pvt *p = ast->tech_pvt;
	int res = 0;
	ast_mutex_lock(&p->lock);
	switch (ast_test_flag(p, SIP_DTMF)) {
	case SIP_DTMF_RFC2833:
		if (p->rtp)
			ast_rtp_senddigit_begin(p->rtp, digit);
		break;
	case SIP_DTMF_INBAND:
		res = -1; /* Tell Asterisk to generate inband indications */
		break;
	default:
		break;
	}
	ast_mutex_unlock(&p->lock);
	return res;
}

/*! \brief Send DTMF character on SIP channel
	within one call, we're able to transmit in many methods simultaneously */
static int sip_senddigit_end(struct ast_channel *ast, char digit)
{
	struct sip_pvt *p = ast->tech_pvt;
	int res = 0;
	ast_mutex_lock(&p->lock);
	switch (ast_test_flag(p, SIP_DTMF)) {
	case SIP_DTMF_INFO:
		transmit_info_with_digit(p, digit);
		break;
	case SIP_DTMF_RFC2833:
		if (p->rtp)
			ast_rtp_senddigit_end(p->rtp, digit);
		break;
	case SIP_DTMF_INBAND:
		res = -1; /* Tell Asterisk to stop inband indications */
		break;
	}
	ast_mutex_unlock(&p->lock);
	return res;
}

/*! \brief  sip_transfer: Transfer SIP call */
static int sip_transfer(struct ast_channel *ast, const char *dest)
{
	struct sip_pvt *p = ast->tech_pvt;
	int res;

	ast_mutex_lock(&p->lock);
	if (ast->_state == AST_STATE_RING)
		res = sip_sipredirect(p, dest);
	else
		res = transmit_refer(p, NULL, dest);
	ast_mutex_unlock(&p->lock);
	return res;
}

/*! \brief  sip_attendedtransfer: Transfer SIP call */
static int sip_attendedtransfer(struct ast_channel *ast, struct ast_channel *target)
{
	struct sip_pvt *p = ast->tech_pvt;
	struct sip_pvt *target_pvt = target->tech_pvt;
	int res;

	ast_mutex_lock(&p->lock);
	res = transmit_refer(p, target_pvt, target_pvt->username);
	ast_mutex_unlock(&p->lock);
	return res;
}

static int sip_do_reinvite_t38(void *data)
{
	struct sip_pvt *p = (struct sip_pvt *) data;
	p->t38state = T38_LOCAL_REINVITE;
	ast_log(LOG_DEBUG,"Sending t38 reinvite\n");
	return transmit_reinvite_with_t38_sdp(p, FALSE);
}

static int sip_do_reinvite_fax(void *data)
{
	struct sip_pvt *p = (struct sip_pvt *) data;
	int codec_bit = ffs(p->faxtxcodecs);

	/* We take one codec from our list as passthrough codec */
	if (!codec_bit)
	{
	    ast_log(LOG_ERROR, "REINVITE: faxtxcodecs not set\n");
	    return -1;
	}

	ast_codec_pref_set2(&p->formats, (1 << (codec_bit - 1)));
	return transmit_reinvite_with_sdp(p, FALSE);
}

static int sip_do_reinvite_modem(void *data)
{
	struct sip_pvt *p = (struct sip_pvt *) data;
	int codec_bit = ffs(p->modemtxcodecs);

	/* We take one codec from our list as passthrough codec */
	if (!codec_bit)
	{
	    ast_log(LOG_ERROR, "REINVITE: modemtxcodecs not set\n");
	    return -1;
	}

	ast_codec_pref_set2(&p->formats, (1 << (codec_bit - 1)));
	return transmit_reinvite_with_sdp(p, FALSE);
}

/*! \brief  sip_indicate: Play indication to user 
 * With SIP a lot of indications is sent as messages, letting the device play
   the indication - busy signal, congestion etc */
static int sip_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen)
{
	struct sip_pvt *p = ast->tech_pvt;
	int res = 0;
	int delay;

	ast_mutex_lock(&p->lock);
	switch(condition) {
	case AST_CONTROL_RINGING:
		if (ast->_state == AST_STATE_RING) {
			if (!ast_test_flag(p, SIP_PROGRESS_SENT) ||
			    (ast_test_flag(p, SIP_PROG_INBAND) == SIP_PROG_INBAND_NEVER)) {
				/* Send 180 ringing if out-of-band seems reasonable */
				transmit_response(p, "180 Ringing", &p->initreq);
				ast_set_flag(p, SIP_RINGING);
				if (ast_test_flag(p, SIP_PROG_INBAND) != SIP_PROG_INBAND_YES)
					break;
			} else {
				/* Well, if it's not reasonable, just send in-band */
			}
		}
		res = -1;
		break;
	case AST_CONTROL_BUSY:
		if (ast->_state != AST_STATE_UP) {
			transmit_response(p, "486 Busy Here", &p->initreq);
			ast_set_flag(p, SIP_ALREADYGONE);	
			ast_softhangup_nolock(ast, AST_SOFTHANGUP_DEV);
			break;
		}
		res = -1;
		break;
	case AST_CONTROL_CONGESTION:
		if (ast->_state != AST_STATE_UP) {
			if (p->transparent_response) {
				ast_log(LOG_DEBUG, "Has a transparent response (%s) waiting\n",
					p->transparent_response->rlPart2);

				transmit_response(p, p->transparent_response->rlPart2, &p->initreq);
				p->transparent_response = NULL;
			}
			else
				transmit_response(p, "480 Temporarily Unavailable", &p->initreq);
			ast_set_flag(p, SIP_ALREADYGONE);	
			ast_softhangup_nolock(ast, AST_SOFTHANGUP_DEV);
			break;
		}
		res = -1;
		break;
	case AST_CONTROL_UNALLOCATED:
		if (ast->_state != AST_STATE_UP) {
			transmit_response(p, "404 Not Found", &p->initreq);
			ast_set_flag(p, SIP_ALREADYGONE);	
			ast_softhangup_nolock(ast, AST_SOFTHANGUP_DEV);
			break;
		}
		res = -1;
		break;		
	case AST_CONTROL_NOANSWER:
		if (ast->_state != AST_STATE_UP) {
			transmit_response(p, "480 Temporarily Unavailable (No answer)", &p->initreq);
			ast_set_flag(p, SIP_ALREADYGONE);	
			ast_softhangup_nolock(ast, AST_SOFTHANGUP_DEV);
			break;
		}
		res = -1;
		break;
	case AST_CONTROL_CALL_LIMIT:
		if (ast->_state != AST_STATE_UP) {
			transmit_response(p, "480 Temporarily Unavailable (Call limit)", &p->initreq);
			ast_set_flag(p, SIP_ALREADYGONE);
			ast_softhangup_nolock(ast, AST_SOFTHANGUP_DEV);
			break;
		}
		res = -1;
		break;
	case AST_CONTROL_PROCEEDING:
		if ((ast->_state != AST_STATE_UP) && !ast_test_flag(p, SIP_PROGRESS_SENT) && !ast_test_flag(p, SIP_OUTGOING)) {
			transmit_response(p, "100 Trying", &p->initreq);
			break;
		}
		res = -1;
		break;
	case AST_CONTROL_PROGRESS:
		if ((ast->_state != AST_STATE_UP) && !ast_test_flag(p, SIP_PROGRESS_SENT) && !ast_test_flag(p, SIP_OUTGOING)) {
		    	if (data)
				memcpy(&p->formats, data, datalen);
			transmit_response_with_sdp(p, "183 Session Progress", &p->initreq, 0, FALSE);
			ast_set_flag(p, SIP_PROGRESS_SENT);	
			break;
		}
		res = -1;
		break;
	case AST_CONTROL_HOLD:	/* The other part of the bridge are put on hold */
		if (sipdebug)
			ast_log(LOG_DEBUG, "Bridged channel now on hold%s\n", p->callid);
		ast_set_flag((&p->flags_page2), SIP_PAGE2_PEERONHOLD);
		transmit_reinvite_with_sdp(p, FALSE);
		break;
	case AST_CONTROL_UNHOLD:	/* The other part of the bridge are back from hold */
		if (sipdebug)
			ast_log(LOG_DEBUG, "Bridged channel is back from hold, let's talk! : %s\n", p->callid);
		ast_clear_flag((&p->flags_page2), SIP_PAGE2_PEERONHOLD);
		transmit_reinvite_with_sdp(p, FALSE);
		break;
	case AST_CONTROL_VIDUPDATE:	/* Request a video frame update */
		if (p->vrtp && !ast_test_flag(p, SIP_NOVIDEO)) {
			transmit_info_with_vidupdate(p);
			res = 0;
		} else
			res = -1;
		break;
	case AST_CONTROL_FLASH:
		if (ast_test_flag(p, SIP_COMPAT) == SIP_COMPAT_BROADSOFT) {
		    /* Broadsoft expects to get flashhook events as SIP INFO
		     * even when DTMFs are sent as RFC 2833, so ignore our
		     * DTMF mode by not going through sip_senddigit() */ 
		    transmit_info_with_broadsoft_flash(p);
		} else
		    ast_senddigit(ast, 'f');
		break;
	case AST_CONTROL_FAX:
		p->vbdmode = VBD_MODE_FAX;
		/* Check if current write / read codecs are passthrough */
		if (p->peer_quality && p->peer_quality->max > 0 && !p->quality_meter)
			ast_queue_hangup(ast);
		else if (p->faxmethod == FAX_PASSTHROUGH_AUTO && (!(ast->readformat &
			p->faxtxcodecs) || !(ast->writeformat & p->faxtxcodecs)))
		{
			if (!p->pendinginvite) {
				
				delay = ast_test_flag(p, SIP_OUTGOING) ?
					modemfax_tx_reinvite_delay :
					modemfax_rx_reinvite_delay;
				
				ast_log(LOG_DEBUG, "got AST_CONTROL_FAX: schedule passthrough"
					" re-INVITE, delay - %d\n", delay);

				if(delay)
				{
					ast_sched_add(sched, delay,
						sip_do_reinvite_fax, p);
				} 
				else 					
					sip_do_reinvite_fax(p);
			}
			else 
			{
				ast_log(LOG_DEBUG, "got AST_CONTROL_FAX: pendinginvite; method passthrough\n");
				ast_set_flag(p, SIP_NEEDREINVITE);
			}
		}
		else if (p->faxmethod == FAX_T38_AUTO && p->t38state == T38_DISABLED)
		{
			if (!p->pendinginvite) {
				delay = ast_test_flag(p, SIP_OUTGOING) ? 
					modemfax_tx_reinvite_delay : modemfax_rx_reinvite_delay;
			
				ast_log(LOG_DEBUG, "got AST_CONTROL_FAX: schedule t38 re-INVITE,"
					" delay - %d\n", delay);

				if(delay)
				{
					ast_sched_add(sched, delay,
						sip_do_reinvite_t38, p);
				}
				else				
					sip_do_reinvite_t38(p);
			} 
			else
			{
				ast_log(LOG_DEBUG, "got AST_CONTROL_FAX: pendinginvite; method t38\n");
				ast_set_flag(p, SIP_NEEDREINVITE);
			}
		}
		else
		{
			ast_log(LOG_DEBUG, "ignore AST_CONTROL_FAX: readformat 0x%x; "
				"writeformat 0x%x; faxtxcodecs 0x%x\n",
				ast->readformat, ast->writeformat, p->faxtxcodecs);
		}
		break;
	case AST_CONTROL_MODEM:
		p->vbdmode = VBD_MODE_MODEM;
		/* Check if current write / read codecs are passthrough */
		if (!(ast->readformat & p->modemtxcodecs) || !(ast->writeformat &
			p->modemtxcodecs))
		{
			if (!p->pendinginvite) {
				delay = ast_test_flag(p, SIP_OUTGOING) ?
					modemfax_tx_reinvite_delay : modemfax_rx_reinvite_delay;

				ast_log(LOG_DEBUG, "got AST_CONTROL_MODEM: schedule re-INVITE,"
					" delay - %d\n", delay);
					
				if(delay)
				{
					ast_sched_add(sched, delay,
						sip_do_reinvite_modem, p);
				}
				else 
					sip_do_reinvite_modem(p);
 			}
			else
			{
				ast_log(LOG_DEBUG, "got AST_CONTROL_MODEM: pendinginvite\n");
				ast_set_flag(p, SIP_NEEDREINVITE);
			}
		}
		else
		{
			ast_log(LOG_DEBUG, "ignore AST_CONTROL_MODEM: readformat 0x%x; writeformat 0x%x; modemtxcodecs 0x%x\n",
				ast->readformat, ast->writeformat, p->modemtxcodecs);
		}
		break;
	case AST_CONTROL_PLAY_DIAGNOSTICS:
		res = incall_announcement_start_indicated(ast);
		break;
	case AST_CONTROL_TRANSPARENT_DATA:
		ast_log(LOG_DEBUG, "Got transparent frame, Data is %p datalen is %d\n",
			data, datalen);
		
		if (data) {
			ast_transparent_frame_payload trans_payload;
			trans_payload = *(const struct ast_transparent_frame_payload *)data;
			
			switch (trans_payload.code) {
			case AST_TRANSPARENT_SIP_RESPONSE:
				ast_log(LOG_DEBUG, "Got transparent error response frame\n");
				p->transparent_response = (struct sip_request *)trans_payload.data;
				break;
			default:
				res = -1;
				break;
			}
		}
		break;
	case -1:
		res = -1;
		break;
	default:
		ast_log(LOG_WARNING, "Don't know how to indicate condition %d\n", condition);
		res = -1;
		break;
	}
	ast_mutex_unlock(&p->lock);
	return res;
}

#ifdef T38_SUPPORT
static int sip_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo,struct ast_channel **rc, int timeoutms) {

     /* Because attempt to do a native RTP bridge between peers happens before T38 re-invites
        and that one time only, and at that moment neither peers have T38 enabled, this will
        lead to the native RTP bridge always (if canreinvite is set to yes). 
	Since this is not good for T38 bridging, we have to disable native bridging entirely if 
	t38 support is enabled - not good, but working. 
	XXX: It would be better to have user/peer configuration flag for t38support. :XXX
     */
     if (!t38udptlsupport) {
	    return ast_rtp_bridge(c0,c1,flags,fo,rc,timeoutms);
     } else {
	    ast_log(LOG_NOTICE, "T38 UDPTL support enabled native RTP bridging disabled\n");
	    return AST_BRIDGE_FAILED_NOWARN;
     }
}
#endif

/*! \brief  sip_new: Initiate a call in the SIP channel */
/*      called from sip_request_call (calls from the pbx ) */
static struct ast_channel *sip_new(struct sip_pvt *i, int state, char *title, int share_callid)
{
	struct ast_channel *tmp;
	struct ast_variable *v = NULL;
	struct sip_peer *tmp_peer = NULL;
	int fmt;
#ifdef OSP_SUPPORT
	char iabuf[INET_ADDRSTRLEN];
	char peer[MAXHOSTNAMELEN];
#endif	

	if (global_max_sessions && usecnt >= global_max_sessions)
	{
	    ast_log(LOG_WARNING, "Reached maximum number of sessions %d\n", global_max_sessions);
	    return NULL;
	}

	ast_mutex_unlock(&i->lock);
	/* Don't hold a sip pvt lock while we allocate a channel */
	tmp = ast_channel_alloc(1);
	ast_mutex_lock(&i->lock);
	if (!tmp) {
		ast_log(LOG_WARNING, "Unable to allocate SIP channel structure\n");
		return NULL;
	}
	tmp->tech = &sip_tech;

	if (((tmp_peer = find_peer(NULL, &i->recv, 1)) || (!ast_strlen_zero(i->exten) && (tmp_peer = find_peer(i->exten, NULL, 1)))) &&
	    !(ast_test_flag(tmp_peer, SIP_DYNAMIC)))
	{
		sip_alloc_quality(i);
	}
	else if (tmp_peer && ast_test_flag(tmp_peer, SIP_DYNAMIC))
	{
	    	struct ast_codec_pref empty_prefs = {};

		memcpy(&i->formats, memcmp(&global_prefs, &empty_prefs, sizeof(global_prefs)) ? &global_prefs : &tmp_peer->prefs, sizeof(global_prefs));
	}

	if (tmp_peer)
		ASTOBJ_UNREF(tmp_peer, sip_destroy_peer);

	/* Select our native format based on codec preference until we receive
	   something from another device to the contrary. */
	memcpy(&tmp->nativeformats, &i->formats, sizeof(i->formats));
	tmp->preferred_codec = global_preferred_codec;

	ast_codec_pref_remove2(&tmp->nativeformats, ~i->usercapability);
	fmt = ast_codec_pref_index_audio(&tmp->nativeformats, 0);

	if (title)
		snprintf(tmp->name, sizeof(tmp->name), "SIP/%s-%04x", title, thread_safe_rand() & 0xffff);
	else if (strchr(i->fromdomain,':'))
		snprintf(tmp->name, sizeof(tmp->name), "SIP/%s-%08x", strchr(i->fromdomain,':')+1, (int)(long)(i));
	else
		snprintf(tmp->name, sizeof(tmp->name), "SIP/%s-%08x", i->fromdomain, (int)(long)(i));

	tmp->type = channeltype;
	if (ast_test_flag(i, SIP_DTMF) ==  SIP_DTMF_INBAND && inbandtonedetect) {
		i->vad = ast_dsp_new();
#if defined(T38_SUPPORT)
		ast_dsp_set_features(i->vad, DSP_FEATURE_DTMF_DETECT | DSP_FEATURE_FAX_DETECT);
#else
		ast_dsp_set_features(i->vad, DSP_FEATURE_DTMF_DETECT);
#endif
		if (relaxdtmf)
			ast_dsp_digitmode(i->vad, DSP_DIGITMODE_DTMF | DSP_DIGITMODE_RELAXDTMF);
	}
	if (i->rtp) {
		tmp->fds[0] = ast_rtp_fd(i->rtp);
		tmp->fds[1] = ast_rtcp_fd(i->rtp);
	}
	if (i->vrtp) {
		tmp->fds[2] = ast_rtp_fd(i->vrtp);
		tmp->fds[3] = ast_rtcp_fd(i->vrtp);
	}
#if defined(T38_SUPPORT)
	if (i->udptl) {
    		tmp->fds[4] = ast_udptl_fd(i->udptl);
	}
#endif
	if (state == AST_STATE_RING)
		tmp->rings = 1;
	tmp->adsicpe = AST_ADSI_UNAVAILABLE;
	tmp->writeformat = fmt;
	tmp->rawwriteformat = fmt;
	tmp->readformat = fmt;
	tmp->rawreadformat = fmt;
	tmp->ptime = ast_get_ptime_by_format(&i->userprefs, fmt);
	tmp->tech_pvt = i;

	tmp->callgroup = i->callgroup;
	tmp->pickupgroup = i->pickupgroup;
	tmp->cid.cid_pres = i->callingpres;
	if (!ast_strlen_zero(i->accountcode))
		ast_copy_string(tmp->accountcode, i->accountcode, sizeof(tmp->accountcode));
	if (i->amaflags)
		tmp->amaflags = i->amaflags;
	if (!ast_strlen_zero(i->language))
		ast_copy_string(tmp->language, i->language, sizeof(tmp->language));
	if (!ast_strlen_zero(i->musicclass))
		ast_copy_string(tmp->musicclass, i->musicclass, sizeof(tmp->musicclass));
	i->owner = tmp;
	if (i->ptime)
	{
		i->owner->ptime = i->ptime;
		i->ptime = 0;
	}
	ast_mutex_lock(&usecnt_lock);
	usecnt++;
	ast_mutex_unlock(&usecnt_lock);
	ast_copy_string(tmp->context, i->context, sizeof(tmp->context));
	ast_copy_string(tmp->exten, i->exten, sizeof(tmp->exten));
	if (!ast_strlen_zero(i->cid_num)) 
		tmp->cid.cid_num = strdup(i->cid_num);
	if (!ast_strlen_zero(i->cid_name))
		tmp->cid.cid_name = strdup(i->cid_name);
	if (!ast_strlen_zero(i->rdnis))
		tmp->cid.cid_rdnis = strdup(i->rdnis);
	if (!ast_strlen_zero(i->exten) && strcmp(i->exten, "s"))
		tmp->cid.cid_dnid = strdup(i->exten);
	tmp->priority = 1;
	if (!ast_strlen_zero(i->uri)) {
		pbx_builtin_setvar_helper(tmp, "SIPURI", i->uri);
	}
	if (!ast_strlen_zero(i->domain)) {
		pbx_builtin_setvar_helper(tmp, "SIPDOMAIN", i->domain);
	}
	if (!ast_strlen_zero(i->useragent)) {
		pbx_builtin_setvar_helper(tmp, "SIPUSERAGENT", i->useragent);
	}
	if (!ast_strlen_zero(i->callid)) {
		pbx_builtin_setvar_helper(tmp, "SIPCALLID", i->callid);
	}
#ifdef OSP_SUPPORT
	snprintf(peer, sizeof(peer), "[%s]:%d", ast_inet_ntoa(iabuf, sizeof(iabuf), i->sa.sin_addr), ntohs(i->sa.sin_port));
	pbx_builtin_setvar_helper(tmp, "OSPPEER", peer);
#endif
	ast_setstate(tmp, state);
	if (state != AST_STATE_DOWN) {
		if (ast_pbx_start(tmp)) {
			ast_log(LOG_WARNING, "Unable to start PBX on %s\n", tmp->name);
			ast_hangup(tmp);
			tmp = NULL;
		}
	}
	/* Set channel variables for this call from configuration */
	for (v = i->chanvars ; v ; v = v->next)
		pbx_builtin_setvar_helper(tmp,v->name,v->value);

	/*TODO: copy the callid only when the call comes from trunk */
	if (share_callid)
	    ast_copy_string(tmp->shared_callid, i->callid, strlen(i->callid));
				
	return tmp;
}

/*! \brief  get_sdp_by_line: Reads one line of SIP message body */
static char* get_sdp_by_line(char* line, char *name, int nameLen, char delim)
{
	if (strncasecmp(line, name, nameLen) == 0 && line[nameLen] == delim) {
		return ast_skip_blanks(line + nameLen + 1);
	}
	return "";
}

/*! \brief Reads one line of SIP message body */
static char *get_body_by_line(char *line, const char *name, int nameLen)
{
	if (strncasecmp(line, name, nameLen) == 0 && (line[nameLen] == '=' || line[nameLen] == ':'))
		return ast_skip_blanks(line + nameLen + 1);

	return "";
}

/*! \brief  get_sdp: Gets all kind of SIP message bodies, including SDP,
   but the name wrongly applies _only_ sdp */
static char *get_sdp(struct sip_request *req, char *name, char delim) 
{
	int x;
	int len = strlen(name);
	char *r;

	for (x=0; x<req->lines; x++) {
		r = get_sdp_by_line(req->line[x], name, len, delim);
		if (r[0] != '\0')
			return r;
	}
	return "";
}

/*! \brief Get a specific line from the message body */
static char *get_body(struct sip_request *req, char *name) 
{
	int x;
	int len = strlen(name);
	char *r;

	for (x = 0; x < req->lines; x++) {
		r = get_body_by_line(req->line[x], name, len);
		if (r[0] != '\0')
			return r;
	}

	return "";
}

static void sdpLineNum_iterator_init(int* iterator) 
{
	*iterator = 0;
}

static char* get_sdp_iterate(int* iterator,
			     struct sip_request *req, char *name)
{
	int len = strlen(name);
	char *r;

	while (*iterator < req->lines) {
		r = get_sdp_by_line(req->line[(*iterator)++], name, len, '=');
		if (r[0] != '\0')
			return r;
	}
	return "";
}

static char *find_alias(const char *name, char *_default)
{
	int x;
	for (x=0;x<sizeof(aliases) / sizeof(aliases[0]); x++) 
		if (!strcasecmp(aliases[x].fullname, name))
			return aliases[x].shortname;
	return _default;
}

static char *__get_header(struct sip_request *req, char *name, int *start)
{
	int pass;

	/*
	 * Technically you can place arbitrary whitespace both before and after the ':' in
	 * a header, although RFC3261 clearly says you shouldn't before, and place just
	 * one afterwards.  If you shouldn't do it, what absolute idiot decided it was 
	 * a good idea to say you can do it, and if you can do it, why in the hell would.
	 * you say you shouldn't.
	 * Anyways, pedanticsipchecking controls whether we allow spaces before ':',
	 * and we always allow spaces after that for compatibility.
	 */
	for (pass = 0; name && pass < 2;pass++) {
		int x, len = strlen(name);
		for (x=*start; x<req->headers; x++) {
			if (!strncasecmp(req->header[x], name, len)) {
				char *r = req->header[x] + len;	/* skip name */
				if (pedanticsipchecking)
					r = ast_skip_blanks(r);

				if (*r == ':') {
					*start = x+1;
					return ast_skip_blanks(r+1);
				}
			}
		}
		if (pass == 0) /* Try aliases */
			name = find_alias(name, NULL);
	}

	/* Don't return NULL, so get_header is always a valid pointer */
	return "";
}

/*! \brief  get_header: Get header from SIP request ---*/
static char *get_header(struct sip_request *req, char *name)
{
	int start = 0;
	return __get_header(req, name, &start);
}

/*! \brief  sip_rtp_read: Read RTP from network ---*/
#if defined(T38_SUPPORT)
static struct ast_frame *sip_rtp_read(struct ast_channel *ast, struct sip_pvt *p, int *faxdetect)
#else
static struct ast_frame *sip_rtp_read(struct ast_channel *ast, struct sip_pvt *p)
#endif
{
	/* Retrieve audio/etc from channel.  Assumes p->lock is already held. */
	struct ast_frame *f;
	static struct ast_frame null_frame = { AST_FRAME_NULL, };
	
	if (!p->rtp) {
		/* We have no RTP allocated for this channel */
		return &null_frame;
	}

	switch(ast->fdno) {
	case 0:
		f = ast_rtp_read(p->rtp);	/* RTP Audio */
		break;
	case 1:
		f = ast_rtcp_read(p->rtp);	/* RTCP Control Channel */
		break;
	case 2:
		f = ast_rtp_read(p->vrtp);	/* RTP Video */
		break;
	case 3:
		f = ast_rtcp_read(p->vrtp);	/* RTCP Control Channel for video */
		break;
#if defined(T38_SUPPORT)
	case 4:
		f = ast_udptl_read(p->udptl);	/* UDPTL for T.38 */
		break;
#endif
	default:
		f = &null_frame;
	}
	/* Don't forward RFC2833 if we're not supposed to */
	if (f && (f->frametype == AST_FRAME_DTMF) && (ast_test_flag(p, SIP_DTMF) != SIP_DTMF_RFC2833))
		return &null_frame;
	if (p->owner) {
		/* We already hold the channel lock */
		if (f && f->frametype == AST_FRAME_VOICE) {
                       if (!(f->subclass & p->owner->nativeformats.audio_bits)) {
                               if (!(f->subclass & p->formats.audio_bits)) {
					ast_log(LOG_DEBUG, "Bogus frame of format '%s' received from '%s'!\n",
						ast_getformatname(f->subclass), p->owner->name);
					return &null_frame;
				}
				ast_log(LOG_DEBUG, "Oooh, format changed to %d\n", f->subclass);
				ast_codec_pref_append(&p->owner->nativeformats, f->subclass);
				ast_set_read_format(p->owner, p->owner->readformat);
				ast_set_write_format(p->owner, p->owner->writeformat);
			}
			if ((ast_test_flag(p, SIP_DTMF) == SIP_DTMF_INBAND) && p->vad) {
				f = ast_dsp_process(p->owner, p->vad, f);
				if (f && (f->frametype == AST_FRAME_DTMF)) {
#if defined(T38_SUPPORT)
					if (t38udptlsupport && f->subclass == 'f')  {
						/* Fax tone */
						if (option_debug)
							ast_log(LOG_DEBUG, "Fax CNG detected on %s\n", ast->name);

						*faxdetect = 1;
					}
#endif
					if (option_debug)
						ast_log(LOG_DEBUG, "* Detected inband DTMF '%c'\n", f->subclass);
				}
			}
		}
	}
	return f;
}

/*! \brief  sip_read: Read SIP RTP from channel */
static struct ast_frame *sip_read(struct ast_channel *ast)
{
	struct ast_frame *fr;
	struct sip_pvt *p = ast->tech_pvt;
#if defined(T38_SUPPORT)
	int faxdetected = 0;
#endif
	ast_mutex_lock(&p->lock);
#if defined(T38_SUPPORT)
	fr = sip_rtp_read(ast, p, &faxdetected);
#else
	fr = sip_rtp_read(ast, p);
#endif
	time(&p->lastrtprx);
	ast_mutex_unlock(&p->lock);
#if defined(T38_SUPPORT)
	/* If we are NOT bridged to another channel, and we have detected fax tone we issue T38 re-invite to a peer */
	/* If we are bridged than it is responsibility of the SIP device to issue T38 re-invite if it detects CNG or fax preabmle */
	if (faxdetected  && t38udptlsupport && (p->t38state == T38_DISABLED) && !(ast_bridged_channel(ast))) {
		if (!ast_test_flag(p, SIP_GOTREFER)) {
			if (!p->pendinginvite) {
				if (option_debug > 2)
					ast_log(LOG_DEBUG, "Sending reinvite on SIP (%s) for T.38 negotiation.\n",ast->name);
				p->t38state = T38_LOCAL_REINVITE;
				transmit_reinvite_with_t38_sdp(p, FALSE);
				ast_log(LOG_DEBUG, "T38 state changed to %d on channel %s",p->t38state,ast->name);
			}
		} else if (!ast_test_flag(p, SIP_PENDINGBYE)) {
				if (option_debug > 2)
					ast_log(LOG_DEBUG, "Deferring reinvite on SIP (%s) - it will be re-negotiated for T.38\n",ast->name);
				ast_set_flag(p, SIP_NEEDREINVITE);
			}	
	}
#endif
	return fr;
}

/*! \brief  build_callid: Build SIP CALLID header ---*/
static void build_callid(char *callid, int len, struct in_addr ourip, char *fromdomain)
{
	int res;
	int val;
	int x;
	char iabuf[INET_ADDRSTRLEN];
	for (x=0; x<4; x++) {
		val = thread_safe_rand();
		res = snprintf(callid, len, "%08x", val);
		len -= res;
		callid += res;
	}
	if (!ast_strlen_zero(fromdomain))
		snprintf(callid, len, "@%s", fromdomain);
	else
	/* It's not important that we really use our right IP here... */
		snprintf(callid, len, "@%s", ast_inet_ntoa(iabuf, sizeof(iabuf), ourip));
}

static void make_our_tag(char *tagbuf, size_t len)
{
	snprintf(tagbuf, len, "as%08x", thread_safe_rand());
}

/*! \brief Allocate Session-Timers struct w/in dialog */
static struct sip_st_dlg* sip_st_alloc(struct sip_pvt *const p)
{
	struct sip_st_dlg *stp;

	if (p->stimer) {
		ast_log(LOG_ERROR, "Session-Timer struct already allocated\n");
		return p->stimer;
	}

	if (!(stp = calloc(1, sizeof(struct sip_st_dlg))))
		return NULL;

	p->stimer = stp;

	p->stimer->st_active = FALSE;
	stp->st_schedid = -1;           /* Session-Timers ast_sched scheduler id */

	return p->stimer;
}

/*! \brief  sip_alloc: Allocate SIP_PVT structure and set defaults ---*/
static struct sip_pvt *sip_alloc(char *callid, struct sockaddr_in *sin, int useglobal_nat, const int intended_method)
{
	struct sip_pvt *p;

	if (!(p = calloc(1, sizeof(*p))))
		return NULL;

	ast_mutex_init(&p->lock);

	p->DTMFschedid = -1;
	p->method = intended_method;
	p->initid = -1;
	p->waitid = -1;
	p->autokillid = -1;
	p->subscribed = NONE;
	p->stateid = -1;
	p->sessionversion_remote = -1;
	p->session_modify = TRUE;
	p->stimer = NULL;

	p->userprefs = global_prefs;
	p->usercapability = global_capability;
	p->ptime = 0;

	if (intended_method != SIP_OPTIONS)	/* Peerpoke has it's own system */
		p->timer_t1 = 500;	/* Default SIP retransmission timer T1 (RFC 3261) */
#ifdef OSP_SUPPORT
	p->osphandle = -1;
	p->osptimelimit = 0;
#endif	
	if (sin) {
		memcpy(&p->sa, sin, sizeof(p->sa));
		if (ast_sip_ouraddrfor(&p->sa.sin_addr,&p->ourip))
			memcpy(&p->ourip, &__ourip, sizeof(p->ourip));
	} else {
		memcpy(&p->ourip, &__ourip, sizeof(p->ourip));
	}

	p->branch = thread_safe_rand();	
	make_our_tag(p->tag, sizeof(p->tag));
	/* Start with 101 instead of 1 */
	p->ocseq = 101;

	if (sip_methods[intended_method].need_rtp) {
		p->rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, bindaddr.sin_addr);
		if (videosupport)
			p->vrtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, bindaddr.sin_addr);
#if defined(T38_SUPPORT)
		if (t38udptlsupport)
			p->udptl = ast_udptl_new_with_bindaddr(sched, io, 0, bindaddr.sin_addr);
#endif
		if (!p->rtp || (videosupport && !p->vrtp)) {
			ast_log(LOG_WARNING, "Unable to create RTP audio %s session: %s\n", videosupport ? "and video" : "", strerror(errno));
			ast_mutex_destroy(&p->lock);
			if (p->chanvars) {
				ast_variables_destroy(p->chanvars);
				p->chanvars = NULL;
			}
			free(p);
			return NULL;
		}
		ast_rtp_settos(p->rtp, tos);
		if (p->vrtp)
			ast_rtp_settos(p->vrtp, tos);
		ast_rtp_set_so_mark(p->rtp, so_mark);
		if (p->vrtp)
			ast_rtp_set_so_mark(p->vrtp, so_mark);
#if defined(T38_SUPPORT)
		if (p->udptl)
			ast_udptl_settos(p->udptl, tos);
		if (p->udptl)
			ast_udptl_set_so_mark(p->udptl, so_mark);
#endif
		p->rtptimeout = global_rtptimeout;
		p->rtpholdtimeout = global_rtpholdtimeout;
		p->rtpkeepalive = global_rtpkeepalive;
	}

	if (useglobal_nat && sin) {
		/* Setup NAT structure according to global settings if we have an address */
		ast_copy_flags(p, &global_flags, SIP_NAT);
		memcpy(&p->recv, sin, sizeof(p->recv));
		if (p->rtp)
			ast_rtp_setnat(p->rtp, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
		if (p->vrtp)
			ast_rtp_setnat(p->vrtp, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
#if defined(T38_SUPPORT)
		if (p->udptl)
			ast_udptl_setnat(p->udptl, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
#endif
	}

	if (p->method != SIP_REGISTER)
		ast_copy_string(p->fromdomain, default_fromdomain, sizeof(p->fromdomain));
	build_via(p, p->via, sizeof(p->via));
	if (!callid)
		build_callid(p->callid, sizeof(p->callid), p->ourip, p->fromdomain);
	else
		ast_copy_string(p->callid, callid, sizeof(p->callid));
	ast_copy_flags(p, &global_flags, SIP_FLAGS_TO_COPY);
	/* Assign default music on hold class */
	strcpy(p->musicclass, global_musicclass);
	if ((ast_test_flag(p, SIP_DTMF) == SIP_DTMF_RFC2833) || (ast_test_flag(p, SIP_DTMF) == SIP_DTMF_AUTO))
		p->noncodeccapability |= AST_RTP_DTMF;
#if defined(T38_SUPPORT)
	if (p->udptl) {
	    p->t38capability = global_t38_capability;
	    if (ast_udptl_get_error_correction_scheme(p->udptl) == UDPTL_ERROR_CORRECTION_REDUNDANCY)
		    p->t38capability |= T38FAX_UDP_EC_REDUNDANCY;
	    else if (ast_udptl_get_error_correction_scheme(p->udptl) == UDPTL_ERROR_CORRECTION_FEC)
		    p->t38capability |= T38FAX_UDP_EC_FEC;
	    else if (ast_udptl_get_error_correction_scheme(p->udptl) == UDPTL_ERROR_CORRECTION_NONE)
		    p->t38capability |= T38FAX_UDP_EC_NONE;
	    p->t38capability |= T38FAX_RATE_MANAGEMENT_TRANSFERED_TCF;
	    p->t38jointcapability = p->t38capability;
	}	
#endif

	p->used_upstream = 0;
	strcpy(p->context, default_context);
	p->maxtime = default_qualify; 

	p->prack_level = PRACK_LEVEL_NONE;
	p->prack_rseq = 0;		/* Invalid starting value - indicates no RSeq sent */
	p->prack_rack = 0;		/* Invalid starting value - indicates no RAck sent */
	p->prack_expected_rseq = 0;	/* Invalid starting value - indicates no RSeq received */
	p->prack_expected_rack = 0;	/* Invalid starting value - indicates no RAck received */
	p->instant_dial_added = 0;
	p->clir = 0;
	p->transparent_response = NULL;

	/* reset offer media order */
	memset(p->offer_m_order, 0, sizeof(p->offer_m_order));
	memset(p->last_offer_m_order, 0, sizeof(p->offer_m_order));
	
	/* Add to active dialog list */
	ast_mutex_lock(&iflock);
	p->next = iflist;
	iflist = p;
	ast_mutex_unlock(&iflock);
	if (option_debug)
		ast_log(LOG_DEBUG, "Allocating new SIP dialog for %s - %s (%s)\n", callid ? callid : "(No Call-ID)", sip_methods[intended_method].text, p->rtp ? "With RTP" : "No RTP");
	return p;
}

static int is_callid_duplicated(struct sip_pvt* p, char *callid)
{
	struct sip_pvt *tmp = p;
	while (tmp)
	{
		if (!strncmp(tmp->callid, callid, sizeof(tmp->callid)))
			return 1;
		tmp = tmp->next;	
	}

	return 0;
}

/*! \brief  find_call: Connect incoming SIP message to current dialog or create new dialog structure */
/*               Called by handle_request, sipsock_read */
static struct sip_pvt *find_call(struct sip_request *req, struct sockaddr_in *sin, const int intended_method)
{
	struct sip_pvt *p;
	char *callid;
	char *tag = "";
	char totag[128];
	char fromtag[128];

	callid = get_header(req, "Call-ID");

	if (pedanticsipchecking) {
		/* In principle Call-ID's uniquely identify a call, but with a forking SIP proxy
		   we need more to identify a branch - so we have to check branch, from
		   and to tags to identify a call leg.
		   For Asterisk to behave correctly, you need to turn on pedanticsipchecking
		   in sip.conf
		   */
		if (gettag(req, "To", totag, sizeof(totag)))
			ast_set_flag(req, SIP_PKT_WITH_TOTAG);	/* Used in handle_request/response */
		gettag(req, "From", fromtag, sizeof(fromtag));

		if (req->method == SIP_RESPONSE)
			tag = totag;
		else
			tag = fromtag;

		if (option_debug > 4 )
			ast_log(LOG_DEBUG, "= Looking for  Call ID: %s (Checking %s) --From tag %s --To-tag %s  \n", callid, req->method==SIP_RESPONSE ? "To" : "From", fromtag, totag);
	}

	ast_mutex_lock(&iflock);
	p = iflist;
	while(p) {	/* In pedantic, we do not want packets with bad syntax to be connected to a PVT */
		int found = 0;
		if (req->method == SIP_REGISTER)
			found = (!strcmp(p->callid, callid));
		else {
			/* Before dialog initalization, an outgoing call can be forked 
			   and responded with numerous 1XX messages from various clients,
			   each with a different tag. Therefore, we must not check the tags
			   until the dialog starts. 
			   On the otherhand, in case both call ends exists on the same CPE,
			   this check causes a faulty scenario and must be ignored. */
			found = !strcmp(p->callid, callid) && 
			(!pedanticsipchecking || !tag || ast_strlen_zero(p->theirtag) ||
			(!ast_test_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED) &&
			!is_callid_duplicated(p->next, p->callid)) || !strcmp(p->theirtag, tag));

			/* Workaround for a special case of a SIP call between 2
			 * local lines:
			 * 1. Line A sends an INVITE to line B and
			 *    creates a sip_pvt context for this call, with an
			 *    empty theirtag.
			 * 2. We get the INVITE here in line B's context and
			 *    try to match it to an existing sip_pvt.
			 * 3. The condition above goes over all sip_pvt's and
			 *    reaches line A's sip_pvt. Since this sip_pvt
			 *    doesn't have theirtag set, it tries to match
			 *    it by only using the call ID, so there is a match.
			 * 4. As a result Asterisk thinks that the INVITE was
			 *    intended to line A and reports a loopback. */
			if (found && pedanticsipchecking &&
			    	ast_strlen_zero(p->theirtag) &&
			    	req->method == SIP_INVITE)
			{
			    	found = 0;
			}
		}

		if (option_debug > 4)
			ast_log(LOG_DEBUG, "= %s Their Call ID: %s Their Tag %s Our tag: %s\n", found ? "Found" : "No match", p->callid, p->theirtag, p->tag);

		/* If we get a new request within an existing to-tag - check the to tag as well */
		if (pedanticsipchecking && found  && req->method != SIP_RESPONSE) {	/* SIP Request */
			if (p->tag[0] == '\0' && totag[0]) {
				/* We have no to tag, but they have. Wrong dialog */
				found = 0;
			} else if (totag[0]) {			/* Both have tags, compare them */
				if (strcmp(totag, p->tag)) {
					found = 0;		/* This is not our packet */
				}
			}
			if (!found && option_debug > 4)
				ast_log(LOG_DEBUG, "= Being pedantic: This is not our match on request: Call ID: %s Ourtag <null> Totag %s Method %s\n", p->callid, totag, sip_methods[req->method].text);
		}


		if (found) {
			/* Found the call */
			ast_mutex_lock(&p->lock);
			ast_mutex_unlock(&iflock);
			return p;
		}
		p = p->next;
	}
	ast_mutex_unlock(&iflock);
	p = sip_alloc(callid, sin, 1, intended_method);
	if (p)
		ast_mutex_lock(&p->lock);
	return p;
}

/*--- sip_subscribe: Parse subscribe=> line in sip.conf and add to subscriptions */
static int sip_subscribe(char *value, int lineno)
{
	struct sip_subscription *sub;
	char copy[256] = "";
	char *username=NULL, *hostname=NULL, *secret=NULL, *authuser=NULL;
	char *porta=NULL;
	char *contact=NULL;
	char *obproxy=NULL, *obproxyport=NULL;
	char *stringp=NULL;
	
	if (!value)
		return -1;
	ast_copy_string(copy, value, sizeof(copy));
	stringp=copy;
	username = stringp;
	hostname = strrchr(stringp, '@');
	if (hostname) {
		*hostname = '\0';
		hostname++;
	}
	if (!username || ast_strlen_zero(username) || !hostname || ast_strlen_zero(hostname)) {
		ast_log(LOG_WARNING, "Format for subscription is user[:secret[:authuser]]@host[:port][|outbound_proxy[:port]][/contact] at line %d\n", lineno);
		return -1;
	}
	stringp=username;
	username = strsep(&stringp, ":");
	if (username) {
		secret = strsep(&stringp, ":");
		if (secret) 
			authuser = strsep(&stringp, ":");
	}
	stringp = hostname;
	hostname = strsep(&stringp, "/");
	if (hostname) 
		contact = strsep(&stringp, "/");
	if (!contact || ast_strlen_zero(contact))
		contact = "s";

	stringp=hostname;
	hostname = strsep(&stringp, "|");
	obproxy = strsep(&stringp, "|");

	stringp=hostname;
	hostname = strsep(&stringp, ":");
	porta = strsep(&stringp, ":");
	
	if (porta && !atoi(porta)) {
		ast_log(LOG_WARNING, "%s is not a valid port number at line %d\n", porta, lineno);
		return -1;
	}
	if (obproxy)
	{
	    stringp = obproxy;
	    obproxy = strsep(&stringp, ":");
	    obproxyport = strsep(&stringp, ":");

	    if (obproxyport && !atoi(obproxyport)) {
			ast_log(LOG_WARNING, "%s is not a valid port number at line %d\n", porta, lineno);
			return -1;
	    }
	}
	sub = malloc(sizeof(struct sip_subscription));
	if (!sub) {
		ast_log(LOG_ERROR, "Out of memory. Can't allocate SIP registry entry\n");
		return -1;
	}
	memset(sub, 0, sizeof(struct sip_subscription));
	subobjs++;
	ASTOBJ_INIT(sub);
	ast_copy_string(sub->contact, contact, sizeof(sub->contact));
	if (username)
		ast_copy_string(sub->username, username, sizeof(sub->username));
	if (hostname)
		ast_copy_string(sub->hostname, hostname, sizeof(sub->hostname));
	if (obproxy)
		ast_copy_string(sub->obproxy, obproxy, sizeof(sub->obproxy));
	if (authuser)
		ast_copy_string(sub->authuser, authuser, sizeof(sub->authuser));
	if (secret)
		ast_copy_string(sub->secret, secret, sizeof(sub->secret));
	if (!ast_strlen_zero(default_domain))
		ast_copy_string(sub->subdomain, default_domain, sizeof(sub->subdomain));
	sub->expire = -1;
	sub->timeout =  -1;
	sub->refresh = default_subscription_expiry;
	sub->portno = porta ? atoi(porta) : 0;
	sub->obproxyport = obproxyport ? atoi(obproxyport) : 0;
	sub->callid_valid = 0;
	sub->ocseq = 1;
	ASTOBJ_CONTAINER_LINK(&subl, sub);
	ASTOBJ_UNREF(sub,sip_subscription_destroy);
	return 0;
}
	
static struct sip_registry *new_reg(char *contact, char *username,
	char *hostname, char *secret, char *authuser, char *porta, char *obproxy,
	char *obproxyport, char *domain, int backup)
{
	struct sip_registry *reg;

	if (!(reg=malloc(sizeof(struct sip_registry)))) {
		ast_log(LOG_ERROR, "Out of memory. Can't allocate SIP registry "
			"entry\n");
		return NULL;
	}
	memset(reg, 0, sizeof(struct sip_registry));
	regobjs++;
	ASTOBJ_INIT(reg);
	ast_copy_string(reg->contact, contact, sizeof(reg->contact));
	if (username)
		ast_copy_string(reg->username, username, sizeof(reg->username));
	if (hostname)
		ast_copy_string(reg->hostname, hostname, sizeof(reg->hostname));
	reg->uas_srv.healthy = 0;
	if (obproxy)
	{
		struct in_addr in;
		ast_copy_string(reg->obproxy, obproxy, sizeof(reg->obproxy));
		reg->srv_failover = srv_failover_enabled && strlen(obproxy) &&
			!inet_aton(obproxy, &in);
		/* Resolve the UAS address early so it can be used by a relevant peer */
		if (reg->srv_failover) {
			char service[MAXHOSTNAMELEN];
			snprintf(service, sizeof(service), "_sip._udp.%s", obproxy);
			if (ast_get_next_srv_ip(&reg->uas_srv.context,
				&reg->uas_srv.addr, service, min_srv_ttl))
			{
				ast_log(LOG_WARNING, "Could not retrieve outbound proxy "
					"address from DNS server.\n");
			}
			else
				reg->uas_srv.healthy = 1;
		}
	}
	if (authuser)
		ast_copy_string(reg->authuser, authuser, sizeof(reg->authuser));
	if (secret)
		ast_copy_string(reg->secret, secret, sizeof(reg->secret));
	if (domain)
		ast_copy_string(reg->regdomain, domain, sizeof(reg->regdomain));
	reg->expire = -1;
	reg->timeout =  -1;
	reg->refresh = default_expiry;
	reg->expiry = default_expiry;
	reg->portno = porta ? atoi(porta) : 0;
	reg->obproxyport = obproxyport ? atoi(obproxyport) : 0;
	reg->callid_valid = 0;
	reg->ocseq = 101;
	reg->backup = backup;
	reg->is_backup_active = 0;
	reg->reg_primary = NULL;
	reg->reg_backup = NULL;
	reg->sched_recover_primary = -1;
	reg->sched_failover_delay = -1;
	reg->fetch_state = unregister_existing_bindings ? 
		FETCH_BINDING_STATE_QUERY : FETCH_BINDING_STATE_UNBOUND;
	reg->ip_changed = ip_changed;
	snprintf(reg->name, sizeof(reg->name), "%s-%d-%s-%s", reg->hostname,
		reg->portno, reg->contact, reg->backup ? "BKP" : "PRI");
	manager_event_sip_registry();
	return reg;
}

static struct sip_registry *find_reg_by_contact(char *contact, int backup);

/*! \brief  sip_register: Parse register=> line in sip.conf and add to registry */
static int sip_register(char *value, int lineno)
{
	struct sip_registry *reg, *reg_pair;
	char copy[256];
	char *username=NULL, *hostname=NULL, *secret=NULL, *authuser=NULL;
	char *porta=NULL;
	char *contact=NULL;
	char *obproxy=NULL, *obproxyport=NULL;
	char *stringp=NULL;
	int  backup=0;
	
	if (!value)
		return -1;
	ast_copy_string(copy, value, sizeof(copy));
	stringp = copy;
	if (strstr(stringp, register_backup_prefix) == stringp)
	{
	    backup = 1;
	    stringp += strlen(register_backup_prefix);
	}
	username = stringp;
	hostname = strrchr(stringp, '@');
	if (hostname) {
		*hostname = '\0';
		hostname++;
	}
	if (ast_strlen_zero(username) || ast_strlen_zero(hostname)) {
		ast_log(LOG_WARNING, "Format for registration is [BKP-]user"
		    "[:secret[:authuser]]@host[:port][|outbound_proxy[:port]]"
                    "[/contact] at line %d\n", lineno);
		return -1;
	}
	stringp=username;
	username = strsep(&stringp, ":");
	if (username) {
		secret = strsep(&stringp, ":");
		if (secret) 
			authuser = strsep(&stringp, ":");
	}
	stringp = hostname;
	hostname = strsep(&stringp, "/");
	if (hostname) 
		contact = strsep(&stringp, "/");
	if (ast_strlen_zero(contact))
		contact = "s";

	stringp=hostname;
	hostname = strsep(&stringp, "|");
	obproxy = strsep(&stringp, "|");

	stringp=hostname;
	hostname = strsep(&stringp, ":");
	porta = strsep(&stringp, ":");
	
	if (porta && !atoi(porta)) {
		ast_log(LOG_WARNING, "%s is not a valid port number at line %d\n", porta, lineno);
		return -1;
	}
	if (obproxy)
	{
	    stringp = obproxy;
	    obproxy = strsep(&stringp, ":");
	    obproxyport = strsep(&stringp, ":");

		if (obproxyport && !ast_strlen_zero(obproxyport) && !atoi(obproxyport)) {
			ast_log(LOG_WARNING, "%s is not a valid port number at line %d\n",
				obproxyport, lineno);
			return -1;
	    }
	}
	if (!(reg = new_reg(contact, username, hostname, secret, authuser, porta, 
		obproxy, obproxyport, default_domain, backup)))
	{
		ast_log(LOG_WARNING, "No memory to allocate registry\n");
		return -1;
	}
	ast_log(LOG_DEBUG, "Adding REG %s@%s (%s).\n", reg->username,
		reg->hostname, reg->backup?"BACKUP":"PRIMARY");

	if ((reg_pair = find_reg_by_contact(reg->contact, reg->backup ? 0 : 1)))
	{
		if (reg->backup && !reg_pair->reg_backup)
		{
			ast_log(LOG_DEBUG, "Attaching to a Primary:'%s@%s' to "
				"backup.\n", reg_pair->username, reg_pair->hostname);
			reg_pair->reg_backup = ASTOBJ_REF(reg);
			reg->reg_primary = reg_pair; /*the reg_primary pointer is 
										   not counted (REF) */
		}
		else if (!reg->backup && !reg_pair->reg_primary)
		{
			ast_log(LOG_DEBUG, "Attaching to a backup:'%s@%s' to "
				"Primary.\n", reg_pair->username, reg_pair->hostname);
			reg->reg_backup = ASTOBJ_REF(reg_pair);
			reg_pair->reg_primary = reg; /* the reg_primary pointer is
											not counted (REF) */
		}
		else
		{
			ast_log(LOG_WARNING, "REG '%s@%s'(%s) - Multiple primary to "
				"backup or backup to primary.\n", reg->username,
				reg->hostname, reg->backup?"BACKUP":"PRIMARY");
			ASTOBJ_UNREF(reg_pair, sip_registry_destroy);
			ASTOBJ_UNREF(reg, sip_registry_destroy);
			return -1;
		}
		ASTOBJ_UNREF(reg_pair, sip_registry_destroy);
	}

	ASTOBJ_CONTAINER_LINK(&regl, reg);
	ASTOBJ_UNREF(reg,sip_registry_destroy);
	return 0;
}

static void mark_method_allowed(unsigned int *allowed_methods, int sipmethod)
{
	(*allowed_methods) |= (1 << sipmethod);
}

/*! \brief Check if method is allowed for a device or a dialog */
static int is_method_allowed(unsigned int *allowed_methods, int sipmethod)
{
	return ((*allowed_methods) >> sipmethod) & 1;
}

static void mark_parsed_methods(unsigned int *methods, char *methods_str)
{
	char *method;
	for (method = strsep(&methods_str, ","); !ast_strlen_zero(method); method = strsep(&methods_str, ",")) {
		int id = find_sip_method(ast_skip_blanks(method));
		if (id == SIP_UNKNOWN) {
			continue;
		}
		mark_method_allowed(methods, id);
	}
}
/*!
 * \brief parse the Allow header to see what methods the endpoint we
 * are communicating with allows.
 *
 * We parse the allow header on incoming Registrations and save the
 * result to the SIP peer that is registering. When the registration
 * expires, we clear what we know about the peer's allowed methods.
 * When the peer re-registers, we once again parse to see if the
 * list of allowed methods has changed.
 *
 * For peers that do not register, we parse the first message we receive
 * during a call to see what is allowed, and save the information
 * for the duration of the call.
 * \param req The SIP request we are parsing
 * \retval The methods allowed
 */
static unsigned int parse_allowed_methods(struct sip_request *req)
{
	char *allow = ast_strdupa(get_header(req, "Allow"));
	unsigned int allowed_methods = SIP_UNKNOWN;

	if (ast_strlen_zero(allow)) {
		/* I have witnessed that REGISTER requests from Polycom phones do not
		 * place the phone's allowed methods in an Allow header. Instead, they place the
		 * allowed methods in a methods= parameter in the Contact header.
		 */
		char *contact = ast_strdupa(get_header(req, "Contact"));
		char *methods = strstr(contact, ";methods=");

		if (ast_strlen_zero(methods)) {
			/* RFC 3261 states:
			 *
			 * "The absence of an Allow header field MUST NOT be
			 * interpreted to mean that the UA sending the message supports no
			 * methods.   Rather, it implies that the UA is not providing any
			 * information on what methods it supports."
			 *
			 * For simplicity, we'll assume that the peer allows all known
			 * SIP methods if they have no Allow header. We can then clear out the necessary
			 * bits if the peer lets us know that we have sent an unsupported method.
			 */
			return UINT_MAX;
		}
		allow = ast_strip_quoted(methods + 9, "\"", "\"");
	}
	mark_parsed_methods(&allowed_methods, allow);
	return allowed_methods;
}

/*! A wrapper for parse_allowed_methods geared toward sip_pvts
 * \param pvt The sip_pvt we are setting the allowed_methods for
 * \param req The request which we are parsing
 * \retval The methods alloweded by the sip_pvt
 */
static unsigned int set_pvt_allowed_methods(struct sip_pvt *pvt, struct sip_request *req)
{
	pvt->allowed_methods = parse_allowed_methods(req);	
	if (option_debug > 3)
	{
		ast_verbose("Allowed methods:UPDATE=%d, SUBSCRIBE=%d, "
				"NOTIFY=%d, INFO=%d, PRACK=%d\n",
				is_method_allowed(&pvt->allowed_methods, SIP_UPDATE),
				is_method_allowed(&pvt->allowed_methods, SIP_SUBSCRIBE),
				is_method_allowed(&pvt->allowed_methods, SIP_NOTIFY),
				is_method_allowed(&pvt->allowed_methods, SIP_INFO),
				is_method_allowed(&pvt->allowed_methods, SIP_PRACK));
	}
	return pvt->allowed_methods;
}

/*! \brief  lws2sws: Parse multiline SIP headers into one header */
/* This is enabled if pedanticsipchecking is enabled */
static int lws2sws(char *msgbuf, int len) 
{ 
	int h = 0, t = 0; 
	int lws = 0; 
	int newline = 0;

	for (; h < len;) { 
		/* Eliminate all CRs */ 
		if (msgbuf[h] == '\r') { 
			h++; 
			continue; 
		} 
		/* Check for end-of-line */ 
		if (msgbuf[h] == '\n') { 
		    /* Empty line marks end of headers */
		    if (newline) {
			    break;
			}
		    newline = 1;
			/* Check for end-of-message */ 
			if (h + 1 == len) 
				break; 
			/* Check for a continuation line */ 
			if (msgbuf[h + 1] == ' ' || msgbuf[h + 1] == '\t') { 
				/* Merge continuation line */ 
				h++; 
				continue; 
			} 
			/* Propagate LF and start new line */ 
			msgbuf[t++] = msgbuf[h++]; 
			lws = 0;
			continue; 
		} 
		newline = 0;
		if (msgbuf[h] == ' ' || msgbuf[h] == '\t') { 
			if (lws) { 
				h++; 
				continue; 
			} 
			msgbuf[t++] = msgbuf[h++]; 
			lws = 1; 
			continue; 
		} 
		msgbuf[t++] = msgbuf[h++]; 
		if (lws) 
			lws = 0; 
	} 
	/* Continue shifting body */
	for (; h < len;) {
		msgbuf[t++] = msgbuf[h++]; 
	}
	msgbuf[t] = '\0'; 
	return t; 
}

/*! \brief  parse_request: Parse a SIP message ----*/
static void parse_request(struct sip_request *req)
{
	/* Divide fields by NULL's */
	char *c;
	int f = 0;

	c = req->data;

	/* First header starts immediately */
	req->header[f] = c;
	while(*c) {
		if (*c == '\n') {
			/* We've got a new header */
			*c = 0;

			if (sipdebug && option_debug > 3)
				ast_log(LOG_DEBUG, "Header %d: %s (%d)\n", f, req->header[f], (int) strlen(req->header[f]));
			if (ast_strlen_zero(req->header[f])) {
				/* Line by itself means we're now in content */
				c++;
				break;
			}
			if (f >= SIP_MAX_HEADERS - 1) {
				ast_log(LOG_WARNING, "Too many SIP headers. Ignoring.\n");
			} else
				f++;
			req->header[f] = c + 1;
		} else if (*c == '\r') {
			/* Ignore but eliminate \r's */
			*c = 0;
		}
		c++;
	}
	/* Check for last header */
	if (!ast_strlen_zero(req->header[f])) {
		if (sipdebug && option_debug > 3)
			ast_log(LOG_DEBUG, "Header %d: %s (%d)\n", f, req->header[f], (int) strlen(req->header[f]));
		f++;
	}
	req->headers = f;
	/* Now we process any mime content */
	f = 0;
	req->line[f] = c;
	req->body = c;
	if (!is_binary_content_type(get_header(req, "Content-Type")))
	{
		while(*c) {
			if (*c == '\n') {
				/* We've got a new line */
				*c = 0;
				if (sipdebug && option_debug > 3)
					ast_log(LOG_DEBUG, "Line: %s (%d)\n", req->line[f], (int) strlen(req->line[f]));
				if (f >= SIP_MAX_LINES - 1) {
					ast_log(LOG_WARNING, "Too many SDP lines. Ignoring.\n");
				} else
					f++;
				req->line[f] = c + 1;
			} else if (*c == '\r') {
				/* Ignore and eliminate \r's */
				*c = 0;
			}
			c++;
		}
	}
	req->body_len = req->len - (req->body - req->data);
	/* Check for last line */
	if (!ast_strlen_zero(req->line[f])) 
		f++;
	req->lines = f;
	if (*c) 
		ast_log(LOG_WARNING, "Odd content, extra stuff left over ('%s')\n", c);
	/* Split up the first line parts */
	determine_firstline_parts(req);
}

static int sip_show_registry_output(char** output, int fd);

static void manager_event_sip_registry(void)
{
    char *s = NULL;
    
    sip_show_registry_output(&s, -1);
    if (!s)
        return;
    manager_event(EVENT_FLAG_SYSTEM, "FullRegistryUpdate", s);
    free(s);
}

static void manager_event_message(char *peer, char *content_type, char *ip_sm_gw, char *buf, int len)
{
	char *hex = malloc(len*2+1);

	ast_hexencode(buf, hex, len);
	manager_event(EVENT_FLAG_CALL, "SIPrecvmessage", "Peer: %s\r\n"
		"IP-SM-GW: %s\r\n"
		"Content-Type: %s\r\n"
		"Content: %s\r\n", peer, ip_sm_gw, content_type, hex);
	free(hex);
}

static int is_t38_allowed(struct sip_pvt *p) {
	return p->udptl && t38udptlsupport && p->faxmethod == FAX_T38_AUTO;
}

static int has_sdp(struct sip_request *req)
{
	return !strcasecmp(get_header(req, "Content-Type"), "application/sdp");
}

static void append_codecs(struct sip_pvt *p, int index, int *codec_arr)
{
	int i;

	for (i = 0; i < index; ++i)
	{
		struct rtpPayloadType pt;

		pt = ast_rtp_lookup_pt(p->rtp, codec_arr[i]);
		if (!pt.isAstFormat && !pt.code && p->vrtp)
			pt = ast_rtp_lookup_pt(p->vrtp, codec_arr[i]);

		if (!pt.isAstFormat || !(pt.code & p->usercapability))
			continue;

		ast_codec_pref_append_ex(&p->formats, pt.code, NULL, 0);
	}
}

/*! \brief  process_sdp: Process SIP SDP and activate RTP channels---*/
static int process_sdp(struct sip_pvt *p, struct sip_request *req)
{
	char *m;
	char *c;
	char *a;
	const char *o;		/* Pointer to o= line */
	char *o_copy;		/* Copy of o= line */
	char *token;
	char host[258];
	char iabuf[INET_ADDRSTRLEN];
	int len = -1;
	int portno = -1;
	int vportno = -1;
#if defined(T38_SUPPORT)
	int udptlportno = -1;
	int peert38capability = 0;
	char s[256];
	int old = 0;
#endif        
	int peercapability = 0, peernoncodeccapability;
	int vpeercapability=0, vpeernoncodeccapability=0;
	struct sockaddr_in sin;
	char *codecs;
	struct hostent *hp;
	struct ast_hostent ahp;
	int codec;
	int destiterator = 0;
	int iterator;
	int sendonly = 0;
	int x,y;
	int debug=sip_debug_test_pvt(p);
	struct ast_channel *bridgepeer = NULL;
	int codec_index = 0;
	int codec_pt_order[256];
	int ptime = -1;
	int rua_version;
	int ret_val = 0;
	struct sip_peer *peer, *tmp_peer = NULL;
	int appended_codecs = 0;
	int oldnoncodeccapability;

	if (!p->rtp) {
		ast_log(LOG_ERROR, "Got SDP but have no RTP session allocated.\n");
		return -1;
	}

	/* Update our last rtprx when we receive an SDP, too */
	time(&p->lastrtprx);
	time(&p->lastrtptx);

	/* Get codec and RTP info from SDP */
	if (strcasecmp(get_header(req, "Content-Type"), "application/sdp")) {
		ast_log(LOG_NOTICE, "Content is '%s', not 'application/sdp'\n", get_header(req, "Content-Type"));
		return -1;
	}

	/* Store the SDP version number of remote UA. This will allow us to 
	distinguish between session modifications and session refreshes. If 
	the remote UA does not send an incremented SDP version number in a 
	subsequent RE-INVITE then that means its not changing media session. 
	The RE-INVITE may have been sent to update connected party, remote  
	target or to refresh the session (Session-Timers).  Asterisk must not 
	change media session and increment its own version number in answer 
	SDP in this case. */ 
	
	o = get_sdp(req, "o", '=');
	if (ast_strlen_zero(o)) {
		ast_log(LOG_WARNING, "SDP sytax error. SDP without an o= line\n");
		return -1;
	}

	o_copy = ast_strdupa(o);
	token = strsep(&o_copy, " ");  /* Skip username   */
	if (!o_copy) { 
		ast_log(LOG_WARNING, "SDP sytax error in o= line username\n");
		return -1;
	}
	token = strsep(&o_copy, " ");  /* Skip session-id */
	if (!o_copy) { 
		ast_log(LOG_WARNING, "SDP sytax error in o= line session-id\n");
		return -1;
	}
	token = strsep(&o_copy, " ");  /* Version         */
	if (!o_copy) { 
		ast_log(LOG_WARNING, "SDP sytax error in o= line\n");
		return -1;
	}
	if (!sscanf(token, "%d", &rua_version)) {
		ast_log(LOG_WARNING, "SDP sytax error in o= line version\n");
		return -1;
	}

	if (p->sessionversion_remote < 0 || p->sessionversion_remote != rua_version) {
 		p->sessionversion_remote = rua_version;
		p->session_modify = TRUE;
	} else if (p->sessionversion_remote == rua_version) {
		p->session_modify = FALSE;
		ast_log(LOG_DEBUG, "SDP version number same as previous SDP\n");
		return 0;
	} 

	m = get_sdp(req, "m", '=');
	sdpLineNum_iterator_init(&destiterator);
	c = get_sdp_iterate(&destiterator, req, "c");
	if (ast_strlen_zero(m) || ast_strlen_zero(c)) {
		/* An INVITE request with no media, means delayed codec negotiation. */
		ast_log(LOG_WARNING, "Insufficient information for SDP (m = '%s', c = '%s')\n", m, c);
		return -1;
	}
	if (sscanf(c, "IN IP4 %256s", host) != 1) {
		ast_log(LOG_WARNING, "Invalid host in c= line, '%s'\n", c);
		return -1;
	}
	/* XXX This could block for a long time, and block the main thread! XXX */
	hp = ast_gethostbyname(host, &ahp);
	if (!hp) {
		ast_log(LOG_WARNING, "Unable to lookup host in c= line, '%s'\n", c);
		return -1;
	}
	sdpLineNum_iterator_init(&iterator);
	ast_set_flag(p, SIP_NOVIDEO);	
        /*
         * We have to scan m= line first, remember codec order then scan a= lines
         * and only then build prefs. This is because there is no way to identify dynamic
         * payload types before processing a= lines. At the same time we cannot rely
         * on a= lines completely since UA may omit them for static payloads.
         */
	while ((m = get_sdp_iterate(&iterator, req, "m"))[0] != '\0') {
		int found = 0;
		if ((sscanf(m, "audio %d/%d RTP/AVP %n", &x, &y, &len) == 2) ||
		    (sscanf(m, "audio %d RTP/AVP %n", &x, &len) == 1)) {
			found = 1;
			if (req->method != SIP_RESPONSE)
				add_sdp_media_to_order(p, SIP_MEDIA_AUDIO);
			portno = x;
			/* Scan through the RTP payload types specified in a "m=" line: */
			ast_rtp_pt_clear(p->rtp);
			codecs = m + len;
			while(!ast_strlen_zero(codecs)) {
				if (sscanf(codecs, "%d%n", &codec, &len) != 1) {
					ast_log(LOG_WARNING, "Error in codec string '%s'\n", codecs);
					ret_val = -1;
					goto Exit;
				}
				if (debug)
					ast_verbose("Found RTP audio format %d\n", codec);
				ast_rtp_set_m_type(p->rtp, codec);
				codec_pt_order[codec_index++] = codec;
				codecs = ast_skip_blanks(codecs + len);
			}
		}
#if defined(T38_SUPPORT)
		if (sscanf(m, "image %d udptl t38 %n", &x, &len) == 1 && x > 0) 
		{
			if (debug)
				ast_verbose("Got T.38 offer in SDP\n");

			if (req->method != SIP_RESPONSE)
				add_sdp_media_to_order(p, SIP_MEDIA_T38);
			
			if (is_t38_allowed(p))
			{
				found = 1;
				udptlportno = x;

				if (p->owner && p->lastinvite) {
					if (p->t38state != T38_LOCAL_REINVITE) {
						p->t38state = T38_PEER_REINVITE; /* T38 Offered in re-invite from remote party */
						ast_log(LOG_DEBUG, "T38 state changed to %d on channel %s\n",p->t38state,p->owner ? p->owner->name : "<none>" );
					}
				} else {
					p->t38state = T38_PEER_DIRECT; /* T38 Offered directly from peer in first invite */
					ast_log(LOG_DEBUG, "T38 state changed to %d on channel %s\n",p->t38state,p->owner ? p->owner->name : "<none>");
				}
			}
			else
				ast_log(LOG_DEBUG, "Not supporting T38\n");
		}
#endif
		if (p->vrtp)
			ast_rtp_pt_clear(p->vrtp);  /* Must be cleared in case no m=video line exists */

		if (p->vrtp && (sscanf(m, "video %d RTP/AVP %n", &x, &len) == 1)) {
			found = 1;
			ast_clear_flag(p, SIP_NOVIDEO);
			if (req->method != SIP_RESPONSE)
				add_sdp_media_to_order(p, SIP_MEDIA_VIDEO);
			vportno = x;
			/* Scan through the RTP payload types specified in a "m=" line: */
			codecs = m + len;
			while(!ast_strlen_zero(codecs)) {
				if (sscanf(codecs, "%d%n", &codec, &len) != 1) {
					ast_log(LOG_WARNING, "Error in codec string '%s'\n", codecs);
					ret_val = -1;
					goto Exit;
				}
				if (debug)
					ast_verbose("Found RTP video format %d\n", codec);
				ast_rtp_set_m_type(p->vrtp, codec);
				codec_pt_order[codec_index++] = codec;
				codecs = ast_skip_blanks(codecs + len);
			}
		}
		if (!found )
			ast_log(LOG_WARNING, "Unknown SDP media type in offer: %s\n", m);
	}
#if defined(T38_SUPPORT)
	if (portno == -1 && vportno == -1 && udptlportno == -1) {
#else
	if (portno == -1 && vportno == -1) {
#endif
		/* No acceptable offer found in SDP */
		ret_val = -2;
		goto Exit;
	}
	/* Check for Media-description-level-address for audio */
	if (pedanticsipchecking) {
		c = get_sdp_iterate(&destiterator, req, "c");
		if (!ast_strlen_zero(c)) {
			if (sscanf(c, "IN IP4 %256s", host) != 1) {
				ast_log(LOG_WARNING, "Invalid secondary host in c= line, '%s'\n", c);
			} else {
				/* XXX This could block for a long time, and block the main thread! XXX */
				hp = ast_gethostbyname(host, &ahp);
				if (!hp) {
					ast_log(LOG_WARNING, "Unable to lookup host in secondary c= line, '%s'\n", c);
					ret_val = -1;
					goto Exit;
				}
			}
		}
	}
	/* RTP addresses and ports for audio and video */
	sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));

	/* Setup audio port number */
	sin.sin_port = htons(portno);
	if (p->rtp && sin.sin_port) {
		ast_rtp_set_peer(p->rtp, &sin, RTP_SENDRECEIVE);
		if (debug) {
			ast_verbose("Peer audio RTP is at port %s:%d\n", ast_inet_ntoa(iabuf,sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
			ast_log(LOG_DEBUG,"Peer audio RTP is at port %s:%d\n",ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
		}
	}
	/* Check for Media-description-level-address for video */
	if (pedanticsipchecking) {
		c = get_sdp_iterate(&destiterator, req, "c");
		if (!ast_strlen_zero(c)) {
			if (sscanf(c, "IN IP4 %256s", host) != 1) {
				ast_log(LOG_WARNING, "Invalid secondary host in c= line, '%s'\n", c);
			} else {
				/* XXX This could block for a long time, and block the main thread! XXX */
				hp = ast_gethostbyname(host, &ahp);
				if (!hp) {
					ast_log(LOG_WARNING, "Unable to lookup host in secondary c= line, '%s'\n", c);
					ret_val = -1;
					goto Exit;
				}
			}
		}
	}
	/* Setup video port number */
	sin.sin_port = htons(vportno);
	if (p->vrtp && sin.sin_port) {
		ast_rtp_set_peer(p->vrtp, &sin, RTP_SENDRECEIVE);
		if (debug) {
			ast_verbose("Peer video RTP is at port %s:%d\n", ast_inet_ntoa(iabuf,sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
			ast_log(LOG_DEBUG,"Peer video RTP is at port %s:%d\n",ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
		}
	}
#if defined(T38_SUPPORT)
	/* Setup UDPTL port number */
	if (udptlportno > 0)
	{
		sin.sin_port = htons(udptlportno);
		if (p->udptl && t38udptlsupport && sin.sin_port) {
			ast_udptl_set_peer(p->udptl, &sin);
			if (debug) {
				ast_verbose("Peer T.38 UDPTL is at port %s:%d\n", ast_inet_ntoa(iabuf,sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
				ast_log(LOG_DEBUG,"Peer T.38 UDPTL is at port %s:%d\n",ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
			}
		}
	}
#endif

	/* Next, scan through each "a=rtpmap:" line, noting each
	 * specified RTP payload type (with corresponding MIME subtype):
	 */
	sdpLineNum_iterator_init(&iterator);

	/* We need to get rid of the G729B variables to handle reinvite
	 * situations */
	if (p->owner)
	{
		pbx_builtin_setvar_helper(p->owner, "_G729_ANNEXB", NULL);
		if (ast_bridged_channel(p->owner))
			pbx_builtin_setvar_helper(ast_bridged_channel(p->owner), "_G729_ANNEXB", NULL);
	}

	while ((a = get_sdp_iterate(&iterator, req, "a"))[0] != '\0') {
		char* mimeSubtype = ast_strdupa(a); /* ensures we have enough space */

		/* for G729 (18) 'annexb=no' means NOT G729B */
		if ((sscanf(a, "fmtp: %u annexb=%s", &codec, mimeSubtype) == 2
			&& !strcasecmp(mimeSubtype,"no")) ||
			(!ast_test_flag((&p->flags_page2), SIP_PAGE2_G729_ANNEXB)))
		{
			/* if we have owner channel we store on the channel's variables */
			if (p->owner)
			{
				pbx_builtin_setvar_helper(p->owner, "_G729_ANNEXB", "NO");
			}
			/* we don't have owner, so we store on sip private variables which
			 * eventually are copied into the channel */
			else
			{
				struct ast_variable *tmpvar = ast_variable_new("_G729_ANNEXB", "NO");

				tmpvar->next =  p->chanvars;
				p->chanvars = tmpvar;
			}
		}
		if (!strcasecmp(a, "sendonly")) {
			sendonly=1;
			if (p->rtp && sin.sin_port) 
				ast_rtp_set_peer(p->rtp, &sin, RTP_RECEIVE);
			continue;
		}
		if (!strcasecmp(a, "sendrecv")) {
		  	sendonly=0;
		}
		if (!strncasecmp(a, "ptime", 5)) {
		    char *tmp = strrchr(a, ':');
		    if (tmp) {
			tmp++;
			ptime = atoi(tmp);
			if (ptime < PTIME_MIN || ptime > PTIME_MAX) {
			    ptime = -1;
			    ast_log(LOG_DEBUG, "invalid ptime read from SDP: %s\n", a);
			}
		    }
		}
		if (sscanf(a, "rtpmap: %u %[^/]/", &codec, mimeSubtype) != 2) continue;
		if (debug)
			ast_verbose("Found description format %s\n", mimeSubtype);
		/* Note: should really look at the 'freq' and '#chans' params too */
		ast_rtp_set_rtpmap_type(p->rtp, codec, "audio", mimeSubtype);
		if (p->vrtp)
			ast_rtp_set_rtpmap_type(p->vrtp, codec, "video", mimeSubtype);
	}
#if defined(T38_SUPPORT)
	if (udptlportno != -1) {
		int found = 0;
		/* Scan trough the a= lines for T38 attributes and set apropriate fileds */
		sdpLineNum_iterator_init(&iterator);
		old = 0;
		while ((a = get_sdp_iterate(&iterator, req, "a"))[0] != '\0') {
			if (old && (iterator-old != 1))
			    break;
			old = iterator;
			
			if ((sscanf(a, "T38FaxMaxBuffer:%d", &x) == 1)) {
				found = 1;
				ast_log(LOG_DEBUG,"MaxBufferSize:%d\n",x);
			}
			if ((sscanf(a, "T38MaxBitRate:%d", &x) == 1)) {
				found = 1;
				ast_log(LOG_DEBUG,"T38MaxBitRate: %d\n",x);
				ast_udptl_set_max_bitrate(p->udptl, x);
				switch (x) {
				    case 14400:
					peert38capability |= T38FAX_RATE_14400 | T38FAX_RATE_12000 | T38FAX_RATE_9600 | T38FAX_RATE_7200 | T38FAX_RATE_4800 | T38FAX_RATE_2400;
					break;
				    case 12000:
					peert38capability |= T38FAX_RATE_12000 | T38FAX_RATE_9600 | T38FAX_RATE_7200 | T38FAX_RATE_4800 | T38FAX_RATE_2400;
					break;
				    case 9600:
					peert38capability |= T38FAX_RATE_9600 | T38FAX_RATE_7200 | T38FAX_RATE_4800 | T38FAX_RATE_2400;
					break;
				    case 7200:
					peert38capability |= T38FAX_RATE_7200 | T38FAX_RATE_4800 | T38FAX_RATE_2400;
					break;
				    case 4800:
					peert38capability |= T38FAX_RATE_4800 | T38FAX_RATE_2400;
					break;
				    case 2400:
					peert38capability |= T38FAX_RATE_2400;
					break;
				}
			}
			if ((sscanf(a, "T38FaxVersion:%d", &x) == 1)) {
				found = 1;
				ast_log(LOG_DEBUG,"FaxVersion: %d\n",x);
				if (x == 0)
					peert38capability |= T38FAX_VERSION_0;
				else if (x == 1)
					peert38capability |= T38FAX_VERSION_1;
				ast_udptl_set_t38_version(p->udptl, x);
			}
			if ((sscanf(a, "T38FaxMaxDatagram:%d", &x) == 1)) {
				found = 1;
				ast_log(LOG_DEBUG,"FaxMaxDatagram: %d\n",x);
				ast_udptl_set_far_max_datagram(p->udptl, x);
				ast_udptl_set_local_max_datagram(p->udptl, x);
			}
			if ((strncmp(a, "T38FaxFillBitRemoval", 20) == 0)) {
				found = 1;
				if ((sscanf(a, "T38FaxFillBitRemoval:%d", &x) == 1)) {
					ast_log(LOG_DEBUG,"FillBitRemoval: %d\n",x);
					if (x == 1)
					{
						peert38capability |= T38FAX_FILL_BIT_REMOVAL;
						ast_udptl_set_conversion_option(p->udptl, UDPTL_FILL_BIT_REMOVAL);
					}
				}
				else {
					ast_log(LOG_DEBUG,"FillBitRemoval\n");
					peert38capability |= T38FAX_FILL_BIT_REMOVAL;
					ast_udptl_set_conversion_option(p->udptl, UDPTL_FILL_BIT_REMOVAL);
				}
			}
			if ((strncmp(a, "T38FaxTranscodingMMR", 20) == 0)) {
				found = 1;
				if ((sscanf(a, "T38FaxTranscodingMMR:%d", &x) == 1)) {
					ast_log(LOG_DEBUG,"Transcoding MMR: %d\n",x);
					if (x == 1)
					{
						peert38capability |= T38FAX_TRANSCODING_MMR;
						ast_udptl_set_conversion_option(p->udptl,UDPTL_TRANSCODING_MMR);
					}
				}
				else {
					ast_log(LOG_DEBUG,"Transcoding MMR\n");
					peert38capability |= T38FAX_TRANSCODING_MMR;
					ast_udptl_set_conversion_option(p->udptl, UDPTL_TRANSCODING_MMR);
				}
			}
			if ((strncmp(a, "T38FaxTranscodingJBIG", 21) == 0)) {
				found = 1;
				if ((sscanf(a, "T38FaxTranscodingJBIG:%d", &x) == 1)) {
					ast_log(LOG_DEBUG,"Transcoding JBIG: %d\n",x);
					if (x == 1)
					{
						peert38capability |= T38FAX_TRANSCODING_JBIG;
						ast_udptl_set_conversion_option(p->udptl, UDPTL_TRANSCODING_JBIG);
					}
				}
				else {
					ast_log(LOG_DEBUG,"Transcoding JBIG\n");
					peert38capability |= T38FAX_TRANSCODING_JBIG;
					ast_udptl_set_conversion_option(p->udptl, UDPTL_TRANSCODING_JBIG);
				}
			}
			if ((sscanf(a, "T38FaxRateManagement:%255s", s) == 1)) {
			    	found = 1;
				ast_log(LOG_DEBUG,"RateManagement: %s\n", s);
				if (!strcasecmp(s, "localTCF")) {
					peert38capability |= T38FAX_RATE_MANAGEMENT_LOCAL_TCF;
					ast_udptl_set_rate_management_method(p->udptl,UDPTL_LOC_TCF);
				}
				else if (!strcasecmp(s, "transferredTCF")) {
					peert38capability |= T38FAX_RATE_MANAGEMENT_TRANSFERED_TCF;
					ast_udptl_set_rate_management_method(p->udptl, UDPTL_TRANS_TCF);
				}
			}
			if ((sscanf(a, "T38FaxUdpEC:%255s", s) == 1)) {
				found = 1;
				ast_log(LOG_DEBUG,"UDP EC: %s\n", s);
				if (!strcasecmp(s, "t38UDPRedundancy")) {
					peert38capability |= T38FAX_UDP_EC_REDUNDANCY;
					ast_udptl_set_error_correction_scheme(p->udptl, UDPTL_ERROR_CORRECTION_REDUNDANCY);
				} else if (!strcasecmp(s, "t38UDPFEC")) {
					peert38capability |= T38FAX_UDP_EC_FEC;
					ast_udptl_set_error_correction_scheme(p->udptl, UDPTL_ERROR_CORRECTION_FEC);
				} else {
					peert38capability |= T38FAX_UDP_EC_NONE;
					ast_udptl_set_error_correction_scheme(p->udptl, UDPTL_ERROR_CORRECTION_NONE);
				}
			}
		}
		if (found) { /* Some cisco equipment returns nothing beside c= and m= lines in 200 OK T38 SDP */ 
			p->t38peercapability = peert38capability;
			p->t38jointcapability = (peert38capability & 255); /* Put everything beside supported speeds settings */
			peert38capability &= (T38FAX_RATE_14400 | T38FAX_RATE_12000 | T38FAX_RATE_9600 | T38FAX_RATE_7200 | T38FAX_RATE_4800 | T38FAX_RATE_2400); /* Mask speeds only */ 
			p->t38jointcapability |= (peert38capability & p->t38capability); /* Put the lower of our's and peer's speed */
		}
		if (debug)
			ast_log(LOG_DEBUG,"Our T38 capability = (%d), peer T38 capability (%d), joint T38 capability (%d)\n",
				    p->t38capability, 
				    p->t38peercapability,
				    p->t38jointcapability);
	} else {
		p->t38state = T38_DISABLED;
		ast_log(LOG_DEBUG, "T38 state changed to %d on channel %s\n",p->t38state,p->owner ? p->owner->name : "<none>");
	}
#endif
	/* Now gather all of the codecs that were asked for: */
	ast_rtp_get_current_formats(p->rtp,
		&peercapability, &peernoncodeccapability);

	if (peercapability)
		ast_codec_pref_init(&p->formats);
	ast_rtp_offered_from_local(p->rtp, 0);
	if (p->vrtp)
		ast_rtp_offered_from_local(p->vrtp, 0);

	if (req->method != SIP_RESPONSE && req->method != SIP_ACK)
	{
		ast_mutex_lock(&bw_mgt_lock);
		peer = find_peer(p->peername, NULL, 1);

		if (peer && peer->bw_mgt)
		{
			bw_mgt_free_bw(p, peer);

			p->usercapability = find_fitting_codecs(peer, p);

			if (!p->usercapability)
			{
				ret_val = -1;
				ASTOBJ_UNREF(peer, sip_destroy_peer);
				ast_mutex_unlock(&bw_mgt_lock);
				goto Exit;
			}
			
			append_codecs(p, codec_index, codec_pt_order);
			appended_codecs = 1;
			bw_mgt_alloc_bw(p, peer, get_minmax_audio_bw(p->formats.audio_bits, 1));
		}

		if (peer)
		    ASTOBJ_UNREF(peer, sip_destroy_peer);
		ast_mutex_unlock(&bw_mgt_lock);
	}

	if ((tmp_peer = find_peer(NULL, &p->recv, 1)) && (ast_test_flag(tmp_peer, SIP_DYNAMIC)))
	{
	    	struct ast_codec_pref empty_prefs = {};

		memcpy(&p->formats, memcmp(&global_prefs, &empty_prefs, sizeof(global_prefs)) ? &global_prefs : &tmp_peer->prefs, sizeof(global_prefs));

	    	if (peercapability)
			ast_codec_pref_remove2(&p->formats, ~peercapability);
	}
	else if (!appended_codecs && peercapability)	 
		append_codecs(p, codec_index, codec_pt_order);

	if (tmp_peer)
		ASTOBJ_UNREF(tmp_peer, sip_destroy_peer);

	if (p->vrtp)
		ast_rtp_get_current_formats(p->vrtp,
				&vpeercapability, &vpeernoncodeccapability);
	oldnoncodeccapability = p->noncodeccapability;
	p->noncodeccapability = p->noncodeccapability & peernoncodeccapability;
	
	if (ast_test_flag(p, SIP_DTMF) == SIP_DTMF_AUTO) {
		ast_clear_flag(p, SIP_DTMF);
		if (p->noncodeccapability & AST_RTP_DTMF) {
			/* XXX Would it be reasonable to drop the DSP at this point? XXX */
			ast_set_flag(p, SIP_DTMF_RFC2833);
		} else {
			ast_set_flag(p, SIP_DTMF_INBAND);
		}
	}

	ast_rtp_set_inband_dtmf(p->rtp,
	    ast_test_flag(p, SIP_DTMF) == SIP_DTMF_INBAND);
	
	if (debug) {
		/* shame on whoever coded this.... */
		const unsigned slen=512;
		char s1[slen], s2[slen], s3[slen], s4[slen];

		ast_verbose("Capabilities: us - %s, peer - audio=%s/video=%s, combined - %s\n",
			ast_getformatname_multiple(s1, slen, p->usercapability),
			ast_getformatname_multiple(s2, slen, peercapability),
			ast_getformatname_multiple(s3, slen, vpeercapability),
			ast_codec_pref_dump(s4, slen, &p->formats));

		ast_verbose("Non-codec capabilities: us - %s, peer - %s, combined - %s\n",
			ast_rtp_lookup_mime_multiple(s1, slen, oldnoncodeccapability, 0),
			ast_rtp_lookup_mime_multiple(s2, slen, peernoncodeccapability, 0),
			ast_rtp_lookup_mime_multiple(s3, slen, p->noncodeccapability, 0));
	}

	if (req->method == SIP_RESPONSE || req->method == SIP_ACK)
	    ast_clear_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);
	else
	    ast_set_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);

	if (!ast_codec_pref_bits(&p->formats)
#if defined(T38_SUPPORT)
	    && ((p->udptl && !p->t38jointcapability) || p->t38state == T38_DISABLED)
#endif
	    ) {
		ast_log(LOG_NOTICE, "No compatible codecs!\n");
		ret_val = -1;
		goto Exit;
	}

	if (!p->owner) 	/* There's no open channel owning us */
	{
		if (ptime != -1)
			p->ptime = ptime;
		return 0;
	}

	memcpy(&p->owner->nativeformats, &p->formats, sizeof(p->formats));
	if (ptime != -1)
	    p->owner->ptime = ptime;

	if ((bridgepeer=ast_bridged_channel(p->owner))) {
		/* We have a bridge */
		/* Turn on/off music on hold if we are holding/unholding */
		struct ast_frame af = { AST_FRAME_NULL, };
		if (sin.sin_addr.s_addr && !sendonly) {
			ast_moh_stop(bridgepeer);
		
			/* Activate a re-invite */
			ast_queue_frame(p->owner, &af);
		} else {
			/* No address for RTP, we're on hold */
			
			ast_moh_start(bridgepeer, NULL);
			if (sendonly)
				ast_rtp_stop(p->rtp);
			/* Activate a re-invite */
			ast_queue_frame(p->owner, &af);
		}
	}

	/* Manager Hold and Unhold events must be generated, if necessary */
	if (sin.sin_addr.s_addr && !sendonly) {	        
	        append_history(p, "Unhold", req->data);

		if (callevents && ast_test_flag(p, SIP_CALL_ONHOLD)) {
			manager_event(EVENT_FLAG_CALL, "Unhold",
				"Channel: %s\r\n"
				"Uniqueid: %s\r\n",
				p->owner->name, 
				p->owner->uniqueid);

       		}
		if (ast_test_flag(p, SIP_CALL_ONHOLD))
			ast_queue_control(p->owner, AST_CONTROL_UNHOLD);
		ast_clear_flag(p, SIP_CALL_ONHOLD);
		ast_clear_flag(p->owner, AST_FLAG_CALL_ONHOLD);
	} else {	        
		/* No address for RTP, we're on hold */
	        append_history(p, "Hold", req->data);

	        if (callevents && !ast_test_flag(p, SIP_CALL_ONHOLD)) {
			manager_event(EVENT_FLAG_CALL, "Hold",
				"Channel: %s\r\n"
		   	    	"Uniqueid: %s\r\n",
				p->owner->name, 
				p->owner->uniqueid);
		}
		if (!ast_test_flag(p, SIP_CALL_ONHOLD))
			ast_queue_control(p->owner, AST_CONTROL_HOLD);
		ast_set_flag(p, SIP_CALL_ONHOLD);
		ast_set_flag(p->owner, AST_FLAG_CALL_ONHOLD);
	}

Exit:
	if (ret_val)
	{
		/* No acceptable media type/codec found (or codec string error).
		 * Rejecting the offer with 488 will keep any changes made to 
		 * offer_m_order during parsing the offer.
		 * Reset it here to avoid tainting the next offer.
		 */
		if (req->method != SIP_RESPONSE)
			memset(p->offer_m_order, 0, sizeof(p->offer_m_order));
	}

	return ret_val;
}

/*! \brief Add "Supported" header to sip message.  Since some options may
 *  be disabled in the config, the sip_pvt must be inspected to determine what
 *  is supported for this dialog. */
static int add_supported_header(struct sip_pvt *pvt, struct sip_request *req)
{
	int res = 0;
	char supported[256];
	char *next = supported;
	size_t left = sizeof(supported);

	if (st_get_mode(pvt, 0) != SESSION_TIMER_MODE_REFUSE) {
		ast_build_string(&next, &left, "timer");
	}
	if (pvt->prack_level == PRACK_LEVEL_SUPPORTED) {
		ast_build_string(&next, &left, "%s100rel", next == supported ? "" : ", ");
	}

	if (next == supported)
		return 0;

	res = add_header(req, "Supported", supported);
	if (sipdebug)
		ast_log(LOG_DEBUG, "Adding SIP Header \"Supported\" with content: %s\n", supported);

	return res;
}


/*! \brief  add_header: Add header to SIP message */
static int add_header(struct sip_request *req, const char *var, const char *value)
{
	int x = 0;

	if (req->headers == SIP_MAX_HEADERS) {
		ast_log(LOG_WARNING, "Out of SIP header space\n");
		return -1;
	}

	if (req->lines) {
		ast_log(LOG_WARNING, "Can't add more headers when lines have been added\n");
		return -1;
	}

	if (req->len >= sizeof(req->data) - 4) {
		ast_log(LOG_WARNING, "Out of space, can't add anymore (%s:%s)\n", var, value);
		return -1;
	}

	req->header[req->headers] = req->data + req->len;

	if (compactheaders) {
		for (x = 0; x < (sizeof(aliases) / sizeof(aliases[0])); x++)
			if (!strcasecmp(aliases[x].fullname, var))
				var = aliases[x].shortname;
	}

	snprintf(req->header[req->headers], sizeof(req->data) - req->len - 4, "%s: %s\r\n", var, value);
	req->len += strlen(req->header[req->headers]);
	req->headers++;

	return 0;	
}

/*! \brief  add_header_contentLen: Add 'Content-Length' header to SIP message */
static int add_header_contentLength(struct sip_request *req, int len)
{
	char clen[10];

	snprintf(clen, sizeof(clen), "%d", len);
	return add_header(req, "Content-Length", clen);
}

/*! \brief  add_blank_header: Add blank header to SIP message */
static int add_blank_header(struct sip_request *req)
{
	if (req->headers == SIP_MAX_HEADERS)  {
		ast_log(LOG_WARNING, "Out of SIP header space\n");
		return -1;
	}
	if (req->lines) {
		ast_log(LOG_WARNING, "Can't add more headers when lines have been added\n");
		return -1;
	}
	if (req->len >= sizeof(req->data) - 4) {
		ast_log(LOG_WARNING, "Out of space, can't add anymore\n");
		return -1;
	}
	req->header[req->headers] = req->data + req->len;
	snprintf(req->header[req->headers], sizeof(req->data) - req->len, "\r\n");
	req->len += strlen(req->header[req->headers]);
	req->headers++;
	return 0;	
}

/* add_body: Add an arbitrary (binary) body to SIP message */
static int add_body(struct sip_request *req, const char *buf, int len)
{
	if (req->lines == SIP_MAX_LINES)  {
		ast_log(LOG_WARNING, "Out of SIP line space\n");
		return -1;
	}
	if (!req->lines) {
		/* Add extra empty return */
		snprintf(req->data + req->len, sizeof(req->data) - req->len, "\r\n");
		req->len += strlen(req->data + req->len);
	}
	if (req->len >= sizeof(req->data) - 4) {
		ast_log(LOG_WARNING, "Out of space, can't add anymore\n");
		return -1;
	}
	req->line[req->lines] = req->data + req->len;
	memcpy(req->line[req->lines], buf, MIN(sizeof(req->data) - req->len, len));
	req->len += len;
	req->lines++;
	return 0;	
}

/*! \brief  add_accept_header: Add Accept header with supported applications */
static void add_accept_header(struct sip_request *req)
{
	add_header(req, "Accept", "application/sdp, application/dtmf-relay");
}
  	
/*! \brief  add_line: Add content (not header) to SIP message */
static int add_line(struct sip_request *req, const char *line)
{
	return add_body(req, line, strlen(line));
}

/*! \brief  copy_header: Copy one header field from one request to another */
static int copy_header(struct sip_request *req, struct sip_request *orig, char *field)
{
	char *tmp;
	tmp = get_header(orig, field);
	if (!ast_strlen_zero(tmp)) {
		/* Add what we're responding to */
		return add_header(req, field, tmp);
	}
	ast_log(LOG_NOTICE, "No field '%s' present to copy\n", field);
	return -1;
}

/*! \brief  copy_all_header: Copy all headers from one request to another ---*/
static int copy_all_header(struct sip_request *req, struct sip_request *orig, char *field)
{
	char *tmp;
	int start = 0;
	int copied = 0;
	for (;;) {
		tmp = __get_header(orig, field, &start);
		if (!ast_strlen_zero(tmp)) {
			/* Add what we're responding to */
			add_header(req, field, tmp);
			copied++;
		} else
			break;
	}
	return copied ? 0 : -1;
}

/*! \brief  copy_via_headers: Copy SIP VIA Headers from the request to the response ---*/
/*	If the client indicates that it wishes to know the port we received from,
	it adds ;rport without an argument to the topmost via header. We need to
	add the port number (from our point of view) to that parameter.
	We always add ;received=<ip address> to the topmost via header.
	Received: RFC 3261, rport RFC 3581 */
static int copy_via_headers(struct sip_pvt *p, struct sip_request *req, struct sip_request *orig, char *field)
{
	char tmp[256], *oh, *end;
	int start = 0;
	int copied = 0;
	char iabuf[INET_ADDRSTRLEN];

	for (;;) {
		oh = __get_header(orig, field, &start);
		if (!ast_strlen_zero(oh)) {
			if (!copied) {	/* Only check for empty rport in topmost via header */
				char *rport;
				char new[256];

				/* Find ;rport;  (empty request) */
				rport = strstr(oh, ";rport");
				if (rport && *(rport+6) == '=') 
					rport = NULL;		/* We already have a parameter to rport */

				if (rport && (ast_test_flag(p, SIP_NAT) == SIP_NAT_ALWAYS)) {
					/* We need to add received port - rport */
					ast_copy_string(tmp, oh, sizeof(tmp));

					rport = strstr(tmp, ";rport");

					if (rport) {
						end = strchr(rport + 1, ';');
						if (end)
							memmove(rport, end, strlen(end) + 1);
						else
							*rport = '\0';
					}

					/* Add rport to first VIA header if requested */
					/* Whoo hoo!  Now we can indicate port address translation too!  Just
				   	   another RFC (RFC3581). I'll leave the original comments in for
				   	   posterity.  */
					snprintf(new, sizeof(new), "%s;received=%s;rport=%d", tmp, ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr), ntohs(p->recv.sin_port));
				} else {
					/* We should *always* add a received to the topmost via */
					snprintf(new, sizeof(new), "%s;received=%s", oh, ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr));
				}
				add_header(req, field, new);
			} else {
				/* Add the following via headers untouched */
				add_header(req, field, oh);
			}
			copied++;
		} else
			break;
	}
	if (!copied) {
		ast_log(LOG_NOTICE, "No header field '%s' present to copy\n", field);
		return -1;
	}
	return 0;
}

/*! \brief  add_route: Add route header into request per learned route ---*/
static void add_route(struct sip_request *req, struct sip_route *route)
{
	char r[256], *p;
	int n, rem = sizeof(r);

	if (!route) return;

	p = r;
	while (route) {
		n = strlen(route->hop);
		if ((n+3)>rem) break;
		if (p != r) {
			*p++ = ',';
			--rem;
		}
		*p++ = '<';
		ast_copy_string(p, route->hop, rem);  p += n;
		*p++ = '>';
		rem -= (n+2);
		route = route->next;
	}
	*p = '\0';
	add_header(req, "Route", r);
}

/*! \brief  set_destination: Set destination from SIP URI ---*/
static void set_destination(struct sip_pvt *p, char *uri)
{
	char *h, *maddr, hostname[256];
	char iabuf[INET_ADDRSTRLEN];
	int port, hn;
	struct sockaddr_in sin;
	int debug=sip_debug_test_pvt(p);

	/* Parse uri to h (host) and port - uri is already just the part inside the <> */
	/* general form we are expecting is sip[s]:username[:password]@host[:port][;...] */

	if (debug)
		ast_verbose("set_destination: Parsing <%s> for address/port to send to\n", uri);

	/* Find and parse hostname */
	h = strchr(uri, '@');
	if (h)
		++h;
	else {
		h = uri;
		if (strncmp(h, "sip:", 4) == 0)
			h += 4;
		else if (strncmp(h, "sips:", 5) == 0)
			h += 5;
	}
	hn = strcspn(h, ":;>") + 1;
	if (hn > sizeof(hostname)) 
		hn = sizeof(hostname);
	ast_copy_string(hostname, h, hn);
	h += hn - 1;

	/* Is "port" present? if not default to DEFAULT_SIP_PORT */
	if (*h == ':') {
		/* Parse port */
		++h;
		port = strtol(h, &h, 10);
	}
	else
		port = DEFAULT_SIP_PORT;

	/* Got the hostname:port - but maybe there's a "maddr=" to override address? */
	maddr = strstr(h, "maddr=");
	if (maddr) {
		maddr += 6;
		hn = strspn(maddr, "0123456789.") + 1;
		if (hn > sizeof(hostname)) hn = sizeof(hostname);
		ast_copy_string(hostname, maddr, hn);
	}
	
	if (ast_get_ip_or_srv(&sin, hostname, srvlookup ? "_sip._udp" : NULL)) {
		ast_log(LOG_WARNING, "Can't find address for host '%s'\n", hostname);
		return;
	}
	p->sa.sin_family = AF_INET;
	memcpy(&p->sa.sin_addr, &sin.sin_addr, sizeof(p->sa.sin_addr));
	p->sa.sin_port = htons(port);
	if (debug)
		ast_verbose("set_destination: set destination to %s, port %d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr), port);
}

/*! \brief  init_resp: Initialize SIP response, based on SIP request ---*/
static int init_resp(struct sip_request *req, char *resp, struct sip_request *orig)
{
	/* Initialize a response */
	if (req->headers || req->len) {
		ast_log(LOG_WARNING, "Request already initialized?!?\n");
		return -1;
	}
	req->method = SIP_RESPONSE;
	req->header[req->headers] = req->data + req->len;
	snprintf(req->header[req->headers], sizeof(req->data) - req->len, "SIP/2.0 %s\r\n", resp);
	req->len += strlen(req->header[req->headers]);
	req->headers++;
	return 0;
}

/*! \brief  init_req: Initialize SIP request ---*/
static int init_req(struct sip_request *req, int sipmethod, char *recip)
{
	/* Initialize a response */
	if (req->headers || req->len) {
		ast_log(LOG_WARNING, "Request already initialized?!?\n");
		return -1;
	}
	req->header[req->headers] = req->data + req->len;
	snprintf(req->header[req->headers], sizeof(req->data) - req->len, "%s %s SIP/2.0\r\n", sip_methods[sipmethod].text, recip);
	req->len += strlen(req->header[req->headers]);
	req->headers++;
	req->method = sipmethod;
	return 0;
}

static void add_resp_hangup_reason_header(struct sip_pvt *p, struct sip_request
		*resp, char *msg)
{
	if (pbx_builtin_getvar_helper(p->owner, "BARRED_CALL"))
	{
		p->q850_hangupcause = AST_CAUSE_CALL_REJECTED;
		add_hangup_reason_header(resp, p);
		return;
	}

	if (!strncmp(msg, "480", 3)) {
		p->q850_hangupcause = p->owner->hangupcause;
		add_hangup_reason_header(resp, p);
		return;
	}
}

/*! \brief  respprep: Prepare SIP response packet ---*/
static int respprep(struct sip_request *resp, struct sip_pvt *p, char *msg, struct sip_request *req)
{
	char newto[256], *ot;

	memset(resp, 0, sizeof(*resp));
	init_resp(resp, msg, req);
	copy_via_headers(p, resp, req, "Via");
	if (msg[0] == '2' || msg[0] == '1')
		copy_all_header(resp, req, "Record-Route");
	copy_header(resp, req, "From");
	ot = get_header(req, "To");
	if (!strcasestr(ot, "tag=") && strncmp(msg, "100", 3)) {
		/* Add the proper tag if we don't have it already.  If they have specified
		   their tag, use it.  Otherwise, use our own tag */
		if (!ast_strlen_zero(p->theirtag) && ast_test_flag(p, SIP_OUTGOING))
			snprintf(newto, sizeof(newto), "%s;tag=%s", ot, p->theirtag);
		else if (p->tag && !ast_test_flag(p, SIP_OUTGOING))
			snprintf(newto, sizeof(newto), "%s;tag=%s", ot, p->tag);
		else {
			ast_copy_string(newto, ot, sizeof(newto));
			newto[sizeof(newto) - 1] = '\0';
		}
		ot = newto;
	}
	add_header(resp, "To", ot);
	copy_header(resp, req, "Call-ID");
	copy_header(resp, req, "CSeq");
	add_header(resp, "User-Agent", default_useragent);
	add_header(resp, "Allow", p->prack_level != PRACK_LEVEL_NONE ?
	    ALLOWED_METHODS_PRACK : ALLOWED_METHODS);
	/* According to RFC3261, Supported header may only appear in 2XX
	 * responses.
	*/
	if (msg[0] == '2')
		add_supported_header(p, resp);

	/* If this is a 2XX response to INVITE or UPDATE, add Session-Timers related headers if the feature is active for this session */
	if ((req->method == SIP_INVITE || req->method == SIP_UPDATE) && msg[0] == '2' && p->stimer && p->stimer->st_active == TRUE && p->stimer->st_active_peer_ua == TRUE) {
		char se_hdr[256];
		snprintf(se_hdr, sizeof(se_hdr), "%d;refresher=%s", p->stimer->st_interval, 
			strefresher2header(p->stimer->st_ref, 1));
		/* UAS MUST place a "Require: timer" header in the response if the
		 * refresher is "uac". It SHOULD do the same if refresher is "uas" */
		add_header(resp, "Require", "timer");
		add_header(resp, "Session-Expires", se_hdr);
	}

	if (msg[0] == '2' && (p->method == SIP_SUBSCRIBE || p->method == SIP_REGISTER)) {
		/* For registration responses, we also need expiry and
		   contact info */
		char tmp[256];

		snprintf(tmp, sizeof(tmp), "%d", p->expiry);
		add_header(resp, "Expires", tmp);
		if (p->expiry) {	/* Only add contact if we have an expiry time */
			char contact[256];
			snprintf(contact, sizeof(contact), "%s;expires=%d", p->our_contact, p->expiry);
			add_header(resp, "Contact", contact);	/* Not when we unregister */
		}
	} else if (p->our_contact[0]) {
		add_header(resp, "Contact", p->our_contact);
	}
	if (check_for_unique_register && !strncmp(msg, "503", 3) && p->method == SIP_REGISTER)
 	{
             add_header(resp, "Retry-After", "1800");
 	}
	if (p->transparent_response)
	{
		char *reason = get_header(req, "Reason");
		if (!ast_strlen_zero(reason))
			add_header(resp, "Reason", reason);
	}

	add_resp_hangup_reason_header(p, resp, msg);
	return 0;
}

static void add_hangup_reason_header(struct sip_request *req, struct sip_pvt *p)
{
	char header_val[1024];

	ast_log(LOG_DEBUG, "Checking for hangup reason header fields. Sip - %d,"
		"Q850 - %d\n", p->sip_hangupcause, p->q850_hangupcause);

	if (p->sip_hangupcause)
	{
		snprintf(header_val, sizeof(header_val), 
			"SIP ;cause=%d ;text=\"%s\"", p->sip_hangupcause,
			p->sip_hanguptext);
		add_header(req, "Reason", header_val);
	}

	if (p->q850_hangupcause)
	{
		snprintf(header_val, sizeof(header_val), 
			"Q.850 ;cause=%d ;text=\"%s\"", p->q850_hangupcause,
			ast_cause2str(p->q850_hangupcause));
		add_header(req, "Reason", header_val);
	}
}

/*! \brief  reqprep: Initialize a SIP request response packet ---*/
static int reqprep(struct sip_request *req, struct sip_pvt *p, int sipmethod, int seqno, int newbranch)
{
	struct sip_request *orig = &p->initreq;
	char stripped[80];
	char tmp[80];
	char newto[256];
	char *c, *n;
	char *ot, *of;
	int is_strict = 0;	/* Strict routing flag */

	memset(req, 0, sizeof(struct sip_request));
	
	snprintf(p->lastmsg, sizeof(p->lastmsg), "Tx: %s", sip_methods[sipmethod].text);
	
	if (!seqno) {
		p->ocseq++;
		seqno = p->ocseq;
	}
	
	/* A CANCEL must have the same branch as the INVITE that it is canceling. */
	if (sipmethod == SIP_CANCEL) {
		p->branch = p->invite_branch;
		build_via(p, p->via, sizeof(p->via));
	} else if (newbranch && (sipmethod == SIP_INVITE)) {
		p->branch ^= thread_safe_rand();
		p->invite_branch = p->branch;
		build_via(p, p->via, sizeof(p->via));
	} else if (newbranch) {
		p->branch ^= thread_safe_rand();
		build_via(p, p->via, sizeof(p->via));
	}

	/* Check for strict or loose router */
	if (p->route && !ast_strlen_zero(p->route->hop) && strstr(p->route->hop,";lr") == NULL)
		is_strict = 1;

	if (sipmethod == SIP_CANCEL) {
		c = p->initreq.rlPart2;	/* Use original URI */
	} else if (sipmethod == SIP_ACK || sipmethod == SIP_PRACK) {
		/* Use URI from Contact: in 200 OK (if INVITE) 
		(we only have the contacturi on INVITEs) */
		if (!ast_strlen_zero(p->okcontacturi))
			c = is_strict ? p->route->hop : p->okcontacturi;
 		else
 			c = p->initreq.rlPart2;
	} else if (!ast_strlen_zero(p->okcontacturi)) {
			c = is_strict ? p->route->hop : p->okcontacturi; /* Use for BYE or REINVITE */
	} else if (!ast_strlen_zero(p->uri)) {
		c = p->uri;
	} else {
		/* We have no URI, use To: or From:  header as URI (depending on direction) */
		c = get_header(orig, (ast_test_flag(p, SIP_OUTGOING)) ? "To" : "From");
		ast_copy_string(stripped, c, sizeof(stripped));
		c = get_in_brackets(stripped);
		n = strchr(c, ';');
		if (n)
			*n = '\0';
	}	
	init_req(req, sipmethod, c);

	snprintf(tmp, sizeof(tmp), "%d %s", seqno, sip_methods[sipmethod].text);

	add_header(req, "Via", p->via);
	if (p->route) {
		set_destination(p, p->route->hop);
		if (is_strict)
			add_route(req, p->route->next);
		else
			add_route(req, p->route);
	}

	ot = get_header(orig, "To");
	of = get_header(orig, "From");

	/* Add tag *unless* this is a CANCEL, in which case we need to send it exactly
	   as our original request, including tag (or presumably lack thereof) */
	if (!strcasestr(ot, "tag=") && sipmethod != SIP_CANCEL) {
		/* Add the proper tag if we don't have it already.  If they have specified
		   their tag, use it.  Otherwise, use our own tag */
		if (ast_test_flag(p, SIP_OUTGOING) && !ast_strlen_zero(p->theirtag))
			snprintf(newto, sizeof(newto), "%s;tag=%s", ot, p->theirtag);
		else if (!ast_test_flag(p, SIP_OUTGOING))
			snprintf(newto, sizeof(newto), "%s;tag=%s", ot, p->tag);
		else
			snprintf(newto, sizeof(newto), "%s", ot);
		ot = newto;
	}

	if (ast_test_flag(p, SIP_OUTGOING)) {
		add_header(req, "From", of);
		add_header(req, "To", ot);
	} else {
		add_header(req, "From", ot);
		add_header(req, "To", of);
	}
	add_header(req, "Contact", p->our_contact);
	copy_header(req, orig, "Call-ID");
	add_header(req, "CSeq", tmp);

	if (sipmethod == SIP_INVITE)
		add_accept_header(req);

	add_header(req, "User-Agent", default_useragent);
	add_header(req, "Max-Forwards", DEFAULT_MAX_FORWARDS);

	if (p->rpid)
		add_header(req, "Remote-Party-ID", p->rpid);

	/* Add Session-Timers related headers if the feature is active for this session.
	   An exception to this behavior is the ACK request. Since Asterisk never requires 
	   session-timers support from a remote end-point (UAS) in an INVITE, it must 
	   not send 'Require: timer' header in the ACK request. 
	   This should only be added in the INVITE transactions, not MESSAGE or REFER or other
	   in-dialog messages.
	*/
	if (p->stimer && p->stimer->st_active == TRUE && p->stimer->st_active_peer_ua == TRUE 
	    && (sipmethod == SIP_INVITE || sipmethod == SIP_UPDATE)) {
		char se_hdr[256];
		snprintf(se_hdr, sizeof(se_hdr), "%d;refresher=%s", MAX(p->stimer->st_interval, p->stimer->st_min_se), 
			strefresher2header(p->stimer->st_ref, 0));
		add_header(req, "Session-Expires", se_hdr);
		snprintf(se_hdr, sizeof(se_hdr), "%d", p->stimer->st_min_se);
		add_header(req, "Min-SE", se_hdr);
	}

	/* Currently we only support Reason header on CANCEL and BYE */
	if (sipmethod == SIP_CANCEL || sipmethod == SIP_BYE)
		add_hangup_reason_header(req,p);

	return 0;
}

static int send_100rel_allowed(struct sip_pvt *p, char *msg)
{
	int msgno = atoi(msg);

	/* As required by RFC 3262 */
	return !(msgno > 100 && msgno <= 199 && p->prack_status == PRACK_FIRST_ACK_PENDING);
}

/*! \brief  __transmit_response: Base transmit response function */
static int __transmit_response(struct sip_pvt *p, char *msg, struct sip_request *req, int reliable)
{
	struct sip_request resp;
	int seqno = 0;

	if (reliable && (sscanf(get_header(req, "CSeq"), "%d ", &seqno) != 1)) {
		ast_log(LOG_WARNING, "Unable to determine sequence number from '%s'\n", get_header(req, "CSeq"));
		return -1;
	}

	if (!send_100rel_allowed(p, msg))
	{
	    ast_log(LOG_WARNING, "First packet sent with 100rel still waiting for PRACK; discarding this one:\n%s\n", msg);
	    return -1;
	}

	respprep(&resp, p, msg, req);

	if (p->prack_level == PRACK_LEVEL_REQUIRE)
	{
	    int msgno = atoi(msg); /* Remember - converts up to first non-digit */

	   /* PRACK requires that provisional messages (with the exception of
	    * message 100) be transmitted in a reliable manner. Convert to
	    * reliable transmission. */
	    if ((msgno > 100) && (msgno <= 199)) {
		char rseq[12]; /* large enough to represent 2^32 - 1 */
		
		if (!p->prack_rseq)
		    p->prack_rseq = ((unsigned int)rand() % 0x7FFFFFFE) + 1;
		p->prack_expected_rack = p->prack_rseq;

		sprintf(rseq, "%u", p->prack_rseq);
		add_header(&resp, "RSeq", rseq);
		add_header(&resp, "Require", "100rel");
		++p->prack_rseq;
		if (p->prack_status == PRACK_NONE)
		    p->prack_status = PRACK_FIRST_ACK_PENDING;

		if (!reliable)
		    reliable = FLAG_RESPONSE;
	    }
	}

	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, reliable, seqno);
}

/*! \brief  transmit_response: Transmit response, no retransmits */
static int transmit_response(struct sip_pvt *p, char *msg, struct sip_request *req) 
{
	/* If the message is a final message, or PRACK is not enabled for this
	 * transaction, transmit the message in an unreliable manner */
	return __transmit_response(p, msg, req, 0);
}

/*! \brief  transmit_response_with_unsupported: Transmit response, no retransmits */
static int transmit_response_with_unsupported(struct sip_pvt *p, char *msg, struct sip_request *req, char *unsupported) 
{
	struct sip_request resp;
	respprep(&resp, p, msg, req);
	append_date(&resp);
	add_header(&resp, "Unsupported", unsupported);
	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, 0, 0);
}

/*! \brief Transmit response with Warning header (SIP_UPDATE) */
static int transmit_response_with_warning(struct sip_pvt *p, char *msg, struct
		sip_request *req, char *warn_text)
{
	struct sip_request resp;
	respprep(&resp, p, msg, req);
	append_date(&resp);
	add_header(&resp, "Warning", warn_text);
	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, 0, 0);
}

/*! \brief Transmit 422 response with Min-SE header (Session-Timers)  */
static int transmit_response_with_minse(struct sip_pvt *p, char *msg, struct sip_request *req, int minse_int)
{
	struct sip_request resp;
	char minse_str[20];

	respprep(&resp, p, msg, req);
	append_date(&resp);

	snprintf(minse_str, sizeof(minse_str), "%d", minse_int);
	add_header(&resp, "Min-SE", minse_str);

	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, 0, 0);
}

/*! \brief  transmit_response_reliable: Transmit response, Make sure you get a reply */
static int transmit_response_reliable(struct sip_pvt *p, char *msg, struct sip_request *req, int fatal)
{
	return __transmit_response(p, msg, req, fatal ? 2 : 1);
}

/*! \brief  append_date: Append date to SIP message ---*/
static void append_date(struct sip_request *req)
{
	char tmpdat[256];
	struct tm tm;
	time_t t;

	time(&t);
	gmtime_r(&t, &tm);
	strftime(tmpdat, sizeof(tmpdat), "%a, %d %b %Y %T GMT", &tm);
	add_header(req, "Date", tmpdat);
}

/*! \brief  transmit_response_with_date: Append date and content length before transmitting response ---*/
static int transmit_response_with_date(struct sip_pvt *p, char *msg, struct sip_request *req)
{
	struct sip_request resp;
	respprep(&resp, p, msg, req);
	append_date(&resp);
	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, 0, 0);
}

/*! \brief  transmit_response_with_allow: Append Accept header, content length before transmitting response ---*/
static int transmit_response_with_allow(struct sip_pvt *p, char *msg, struct sip_request *req, int reliable)
{
	struct sip_request resp;
	respprep(&resp, p, msg, req);
	add_accept_header(&resp);
	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, reliable, 0);
}

/* transmit_response_with_auth: Respond with authorization request */
static int transmit_response_with_auth(struct sip_pvt *p, char *msg, struct sip_request *req, char *randdata, int reliable, char *header, int stale)
{
	struct sip_request resp;
	char tmp[256];
	int seqno = 0;

	if (reliable && (sscanf(get_header(req, "CSeq"), "%d ", &seqno) != 1)) {
		ast_log(LOG_WARNING, "Unable to determine sequence number from '%s'\n", get_header(req, "CSeq"));
		return -1;
	}
	/* Stale means that they sent us correct authentication, but 
	   based it on an old challenge (nonce) */
	snprintf(tmp, sizeof(tmp), "Digest realm=\"%s\", nonce=\"%s\"%s", global_realm, randdata, stale ? ", stale=true" : "");
	respprep(&resp, p, msg, req);
	add_header(&resp, header, tmp);
	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_response(p, &resp, reliable, seqno);
}

/* add_content: Add an arbitrary body content to the SIP message */
static int add_content(struct sip_request *req, const char *type, const char *buf, int len)
{
	add_header(req, "Content-Type", type);
	add_header_contentLength(req, len);
	add_body(req, buf, len);
	return 0;
}

/*! \brief  add_digit: add DTMF INFO tone to sip message ---*/
/* Always adds default duration 250 ms, regardless of what came in over the line */
static int add_digit(struct sip_request *req, char digit)
{
	char tmp[256];

	if (digit == 'f')
		snprintf(tmp, sizeof(tmp), "Signal=16\r\nDuration=250\r\n");
	else
		snprintf(tmp, sizeof(tmp), "Signal=%c\r\nDuration=250\r\n", digit);
	add_header(req, "Content-Type", "application/dtmf-relay");
	add_header_contentLength(req, strlen(tmp));
	add_line(req, tmp);
	return 0;
}

/*! \brief  add_vidupdate: add XML encoded media control with update ---*/
/* XML: The only way to turn 0 bits of information into a few hundred. */
static int add_vidupdate(struct sip_request *req)
{
	const char *xml_is_a_huge_waste_of_space =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n"
		" <media_control>\r\n"
		"  <vc_primitive>\r\n"
		"   <to_encoder>\r\n"
		"    <picture_fast_update>\r\n"
		"    </picture_fast_update>\r\n"
		"   </to_encoder>\r\n"
		"  </vc_primitive>\r\n"
		" </media_control>\r\n";
	add_header(req, "Content-Type", "application/media_control+xml");
	add_header_contentLength(req, strlen(xml_is_a_huge_waste_of_space));
	add_line(req, xml_is_a_huge_waste_of_space);
	return 0;
}

static void add_codec_to_sdp(const struct sip_pvt *p, int codec, int sample_rate,
			     char **m_buf, size_t *m_size, char **a_buf, size_t *a_size,
			     int debug)
{
	int rtp_code;

	if (debug)
		ast_verbose("Adding codec 0x%x (%s) to SDP\n", codec, ast_getformatname(codec));
	if ((rtp_code = ast_rtp_lookup_code(p->rtp, 1, codec)) == -1) {
		if (!p->vrtp || (rtp_code = ast_rtp_lookup_code(p->vrtp, 1, codec)) == -1)
		return;
	}

	ast_build_string(m_buf, m_size, " %d", rtp_code);
	ast_build_string(a_buf, a_size, "a=rtpmap:%d %s/%d\r\n", rtp_code,
			 ast_rtp_lookup_mime_subtype(1, codec),
			 sample_rate);
	/* We indicate that we do support G.729B in case the other side supports.
	 * Not sending 'annexb=' means 'annexb=yes' */
	if (codec == AST_FORMAT_G729A && p->owner && 
	        (pbx_builtin_getvar_helper(p->owner,"G729_ANNEXB") ||
		!ast_test_flag((&p->flags_page2), SIP_PAGE2_G729_ANNEXB)))
	{
		ast_build_string(a_buf, a_size, "a=fmtp:%d annexb=no\r\n", rtp_code);
	}
}

static void add_noncodec_to_sdp(const struct sip_pvt *p, int format, int sample_rate,
				char **m_buf, size_t *m_size, char **a_buf, size_t *a_size,
				int debug)
{
	int rtp_code;

	if (debug)
		ast_verbose("Adding non-codec 0x%x (%s) to SDP\n", format, ast_rtp_lookup_mime_subtype(0, format));
	if ((rtp_code = ast_rtp_lookup_code(p->rtp, 0, format)) == -1)
		return;

	ast_build_string(m_buf, m_size, " %d", rtp_code);
	ast_build_string(a_buf, a_size, "a=rtpmap:%d %s/%d\r\n", rtp_code,
			 ast_rtp_lookup_mime_subtype(0, format),
			 sample_rate);
	if (format == AST_RTP_DTMF)
		/* Indicate we support DTMF and FLASH... */
		ast_build_string(a_buf, a_size, "a=fmtp:%d 0-16\r\n", rtp_code);
}

#if defined(T38_SUPPORT)
/*--- t38_get_rate: Get Max T.38 Transmision rate from T38 capabilities ---*/
int t38_get_rate(int t38cap)
{
    int maxrate = (t38cap & (T38FAX_RATE_14400 | T38FAX_RATE_12000 | T38FAX_RATE_9600 | T38FAX_RATE_7200 | T38FAX_RATE_4800 | T38FAX_RATE_2400));
    if (maxrate & T38FAX_RATE_14400) {
		ast_log(LOG_DEBUG, "T38MaxFaxRate 14400 found\n");
		return 14400;
    } else if (maxrate & T38FAX_RATE_12000) {
		ast_log(LOG_DEBUG, "T38MaxFaxRate 12000 found\n");
		return 12000;
    } else if (maxrate & T38FAX_RATE_9600) {
		ast_log(LOG_DEBUG, "T38MaxFaxRate 9600 found\n");
		return 9600;
    } else if (maxrate & T38FAX_RATE_7200) {
		ast_log(LOG_DEBUG, "T38MaxFaxRate 7200 found\n");
		return 7200;
    } else if (maxrate & T38FAX_RATE_4800) {
		ast_log(LOG_DEBUG, "T38MaxFaxRate 4800 found\n");
		return 4800;
    } else if (maxrate & T38FAX_RATE_2400) {
		ast_log(LOG_DEBUG, "T38MaxFaxRate 2400 found\n");
		return 2400;
    } else {
		ast_log(LOG_DEBUG, "Strange, T38MaxFaxRate NOT found in peers T38 SDP.\n");
		return 0;
    }
}

/*--- add_t38_sdp: Add T.38 Session Description Protocol message ---
    If oldsdp is TRUE, then the SDP version number is not incremented. This mechanism
    is used in Session-Timers where RE-INVITEs are used for refreshing SIP sessions 
    without modifying the media session in any way. 
*/
static int add_t38_sdp(struct sip_request *resp, struct sip_pvt *p, int oldsdp)
{
	int len = 0;
	int x = 0;
	struct sockaddr_in udptlsin;
	char v[256] = "";
	char s[256] = "";
	char o[256] = "";
	char c[256] = "";
	char t[256] = "";
	char m_modem[256];
	char a_modem[1024];
	char m_audio[256];
	char *m_modem_next = m_modem;
	size_t m_modem_left = sizeof(m_modem);
	char *a_modem_next = a_modem;
	size_t a_modem_left = sizeof(a_modem);
	char *m_audio_next = m_audio;
	size_t m_audio_left = sizeof(m_audio);
	char iabuf[INET_ADDRSTRLEN];
	struct sockaddr_in udptldest = { 0, };
	int debug;
	int has_audio = 0;

	debug = sip_debug_test_pvt(p);
	len = 0;
	if (!p->udptl) {
		ast_log(LOG_WARNING, "No way to add SDP without an UDPTL structure\n");
		return -1;
	}

	if (!p->sessionid) {
		p->sessionid = getpid();
		p->sessionversion = p->sessionid;
	} else {
		if (oldsdp == FALSE)
			p->sessionversion++;
		else
			memcpy(p->offer_m_order, p->last_offer_m_order, sizeof(p->offer_m_order));
	}

	/* Our T.38 end is */
	if (p->udptl)
		ast_udptl_get_us(p->udptl, &udptlsin);

	/* Determine T.38 UDPTL destination */
	if (p->udptl) {
		if (p->udptlredirip.sin_addr.s_addr) {
			udptldest.sin_port = p->udptlredirip.sin_port;
			udptldest.sin_addr = p->udptlredirip.sin_addr;
		} else {
			udptldest.sin_addr = p->ourip;
			udptldest.sin_port = udptlsin.sin_port;
		}
	}

	if (debug){
		if (p->udptl)
			ast_verbose("T.38 UDPTL is at %s port %d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip), ntohs(udptlsin.sin_port));	
	}

	/* We break with the "recommendation" and send our IP, in order that our
	   peer doesn't have to ast_gethostbyname() us */

	if (debug){
		ast_log(LOG_DEBUG, "Our T38 capability (%d), peer T38 capability (%d), joint capability (%d)\n",
			p->t38capability,
			p->t38peercapability,
			p->t38jointcapability);	
	}
	snprintf(v, sizeof(v), "v=0\r\n");
	snprintf(o, sizeof(o), "o=root %d %d IN IP4 %s\r\n", p->sessionid, p->sessionversion, ast_inet_ntoa(iabuf, sizeof(iabuf), udptldest.sin_addr));
	snprintf(s, sizeof(s), "s=session\r\n");
	snprintf(c, sizeof(c), "c=IN IP4 %s\r\n", ast_inet_ntoa(iabuf, sizeof(iabuf), udptldest.sin_addr));
	snprintf(t, sizeof(t), "t=0 0\r\n");

	/* check if we got audio offer */
	for (x = 0; p->offer_m_order[x] && x < SIP_MAX_OFFER_MEDIA; x++)
	{
		if (p->offer_m_order[x] == SIP_MEDIA_AUDIO)
		{
			has_audio = 1;
			break;
		}
	}

	if (!p->offer_m_order[0])  /* if order is zero, then this is an offer */
	{
		/* set new order for this offer */
		p->offer_m_order[0] = SIP_MEDIA_T38;
	}
	else if (has_audio)	/* its an answer, check if need to answer to audio */
	{
		int codec, x, rtp_code, alreadysent = 0;

		ast_build_string(&m_audio_next, &m_audio_left, "m=audio 0 RTP/AVP");
		for (x = 0; x < 32; x++) 
		{
			if (!(codec = ast_codec_pref_index_audio(&p->formats, x)))
				continue;

			if (alreadysent & codec)
				continue;

			if ((rtp_code = ast_rtp_lookup_code(p->rtp, 1, codec)) == -1) 
				continue;

			ast_build_string(&m_audio_next, &m_audio_left, " %d", rtp_code);
			alreadysent |= codec;
		}		
		ast_build_string(&m_audio_next, &m_audio_left, "\r\n");
	}

	ast_build_string(&m_modem_next, &m_modem_left, "m=image %d udptl t38\r\n", ntohs(udptldest.sin_port));

	if ((p->t38jointcapability & T38FAX_VERSION) == T38FAX_VERSION_0)
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxVersion:0\r\n");
	if ((p->t38jointcapability & T38FAX_VERSION) == T38FAX_VERSION_1)
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxVersion:1\r\n");
	if ((x = t38_get_rate(p->t38jointcapability))) 
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38MaxBitRate:%d\r\n",x);
	if ((p->t38jointcapability & T38FAX_FILL_BIT_REMOVAL) == T38FAX_FILL_BIT_REMOVAL)
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxFillBitRemoval\r\n");
	if ((p->t38jointcapability & T38FAX_TRANSCODING_MMR) == T38FAX_TRANSCODING_MMR)
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxTranscodingMMR\r\n");
	if ((p->t38jointcapability & T38FAX_TRANSCODING_JBIG) == T38FAX_TRANSCODING_JBIG)
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxTranscodingJBIG\r\n");
	ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxRateManagement:%s\r\n", (p->t38jointcapability & T38FAX_RATE_MANAGEMENT_LOCAL_TCF) ? "localTCF" : "transferredTCF");
	x = ast_udptl_get_local_max_datagram(p->udptl);
	ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxMaxBuffer:%d\r\n",x);
	ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxMaxDatagram:%d\r\n",x);
	if (p->t38jointcapability & T38FAX_UDP_EC_REDUNDANCY ||
		p->t38jointcapability & T38FAX_UDP_EC_FEC)
	{
		ast_build_string(&a_modem_next, &a_modem_left, "a=T38FaxUdpEC:%s\r\n", 
			(p->t38jointcapability & T38FAX_UDP_EC_REDUNDANCY) ? 
			"t38UDPRedundancy" : "t38UDPFEC");
	}
	len += strlen(v) + strlen(s) + strlen(o) + strlen(c) + strlen(t);
	if (p->udptl)
		len += strlen(m_modem) + strlen(a_modem);
	if (has_audio)
		len += strlen(m_audio);
	add_header(resp, "Content-Type", "application/sdp");
	add_header_contentLength(resp, len);
	add_line(resp, v);
	add_line(resp, o);
	add_line(resp, s);
	add_line(resp, c);
	add_line(resp, t);
	
	for (x = 0; p->offer_m_order[x] && x < SIP_MAX_OFFER_MEDIA; x++)
	{
		switch (p->offer_m_order[x])
		{
		case SIP_MEDIA_T38:
			add_line(resp, m_modem);
			add_line(resp, a_modem);
			break;
		case SIP_MEDIA_AUDIO:
			add_line(resp, m_audio);
			break;
		default:
			ast_log(LOG_ERROR, "Unknown media type %d!!! BUG!!!\n", 
				p->offer_m_order[x]);
		}
	}

	/* Update lastrtprx when we send our SDP */
	time(&p->lastrtprx);
	time(&p->lastrtptx);

	/* reset offer media order */
	memcpy(p->last_offer_m_order, p->offer_m_order, sizeof(p->offer_m_order));
	memset(p->offer_m_order, 0, sizeof(p->offer_m_order));

	if (resp->method == SIP_RESPONSE)
	    ast_clear_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);
	else
	    ast_set_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);
	
	return 0;
}
#endif

/*! \brief Add Session Description Protocol message 

    If oldsdp is TRUE, then the SDP version number is not incremented. This mechanism
    is used in Session-Timers where RE-INVITEs are used for refreshing SIP sessions 
    without modifying the media session in any way. 
*/
static int add_sdp(struct sip_request *resp, struct sip_pvt *p, int oldsdp)
{
	int len = 0;
	int pref_codec;
	int alreadysent = 0;
	struct sockaddr_in sin;
	struct sockaddr_in vsin;
	char v[256];
	char s[256];
	char o[256];
	char c[256];
	char t[256];
	char *hold;
	char m_modem[256];
	char m_audio[256];
	char m_video[256];
	char a_audio[1024];
	char a_video[1024];
	char *m_modem_next = m_modem;
	char *m_audio_next = m_audio;
	char *m_video_next = m_video;
	size_t m_modem_left = sizeof(m_modem);
	size_t m_audio_left = sizeof(m_audio);
	size_t m_video_left = sizeof(m_video);
	char *a_audio_next = a_audio;
	char *a_video_next = a_video;
	size_t a_audio_left = sizeof(a_audio);
	size_t a_video_left = sizeof(a_video);
	char iabuf[INET_ADDRSTRLEN];
	int x;
	const struct ast_codec_pref *capability;
	struct sockaddr_in dest;
	struct sockaddr_in vdest = { 0, };
	int debug;
	int has_t38 = 0;
	
	debug = sip_debug_test_pvt(p);

	len = 0;
	if (!p->rtp) {
		ast_log(LOG_WARNING, "No way to add SDP without an RTP structure\n");
		return -1;
	}
	capability = &p->formats;
		
	if (!p->sessionid) {
		p->sessionid = getpid();
		p->sessionversion = p->sessionid;
	} else {
		if (oldsdp == FALSE)
			p->sessionversion++;
		else
			memcpy(p->offer_m_order, p->last_offer_m_order, sizeof(p->offer_m_order));
	}
	ast_rtp_get_us(p->rtp, &sin);
	if (p->vrtp)
		ast_rtp_get_us(p->vrtp, &vsin);

	if (p->redirip.sin_addr.s_addr) {
		dest.sin_port = p->redirip.sin_port;
		if (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE) {
			dest.sin_port = sin.sin_port;
			dest.sin_addr = p->ourip;
		} else {
			dest.sin_port = p->redirip.sin_port;
		dest.sin_addr = p->redirip.sin_addr;
		}
                if (ast_codec_pref_bits(&p->redircodecs)) {
                        /* We are to get all compatible with redircodecs codecs */
			capability = &p->redircodecs;
                }
	} else {
		dest.sin_addr = p->ourip;
		dest.sin_port = sin.sin_port;
	}

	/* Determine video destination */
	if (p->vrtp) {
		if (p->vredirip.sin_addr.s_addr) {
			vdest.sin_port = p->vredirip.sin_port;
			vdest.sin_addr = p->vredirip.sin_addr;
		} else {
			vdest.sin_addr = p->ourip;
			vdest.sin_port = vsin.sin_port;
		}
	}
	if (debug){
		ast_verbose("We're at %s port %d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip), ntohs(sin.sin_port));	
		if (p->vrtp)
			ast_verbose("Video is at %s port %d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip), ntohs(vsin.sin_port));	
	}

	/* We break with the "recommendation" and send our IP, in order that our
	   peer doesn't have to ast_gethostbyname() us */

	snprintf(v, sizeof(v), "v=0\r\n");
	snprintf(o, sizeof(o), "o=root %d %d IN IP4 %s\r\n", p->sessionid, p->sessionversion, ast_inet_ntoa(iabuf, sizeof(iabuf), dest.sin_addr));
	snprintf(s, sizeof(s), "s=session\r\n");
	snprintf(c, sizeof(c), "c=IN IP4 %s\r\n", ast_inet_ntoa(iabuf, sizeof(iabuf), dest.sin_addr));
	snprintf(t, sizeof(t), "t=0 0\r\n");

	/* check if we got T38 offer */
	for (x = 0; p->offer_m_order[x] && x < SIP_MAX_OFFER_MEDIA; x++)
	{
		if (p->offer_m_order[x] == SIP_MEDIA_T38)
		{
			has_t38 = 1;
			break;
		}
	}

	if (!p->offer_m_order[0])  /* if order is zero, then this is an offer */
	{
		/* set new order for this offer */
		p->offer_m_order[0] = SIP_MEDIA_AUDIO;
		p->offer_m_order[1] = SIP_MEDIA_VIDEO;
	}

	ast_build_string(&m_modem_next, &m_modem_left, "m=image 0 udptl t38\r\n");
	ast_build_string(&m_audio_next, &m_audio_left, "m=audio %d RTP/AVP", ntohs(dest.sin_port));
	ast_build_string(&m_video_next, &m_video_left, "m=video %d RTP/AVP", ntohs(vdest.sin_port));

	if (ast_test_flag(&p->flags_page2, SIP_PAGE2_PEERONHOLD))
		hold = "a=sendonly\r\n";
	else if (ast_test_flag(p, SIP_CALL_ONHOLD))
		hold = "a=recvonly\r\n";
	else
		hold = "a=sendrecv\r\n";
	
#if defined(T38_SUPPORT)
	if (t38rtpsupport) {
    		/* TODO: Improve this */
		len = snprintf(a_audio_next, a_audio_left, " %d", 191);
    		a_audio_next += len;
    		a_audio_left -= len;
		len = snprintf(a_audio_next, a_audio_left, "a=rtpmap:%d %s/8000\r\n", 191, "t38");
    		a_audio_next += len;
    		a_audio_left -= len;
	}
#endif

	/* Start by sending our preferred codecs */
	for (x = 0; x < 32; x++) {

		if (!(pref_codec = ast_codec_pref_index_audio(&p->userprefs, x)))
			break; 

		if (!(capability->audio_bits & pref_codec) ||
		    (alreadysent & pref_codec))
		{
			continue;
		}
		add_codec_to_sdp(p, pref_codec, 8000,
				 &m_audio_next, &m_audio_left,
				 &a_audio_next, &a_audio_left,
				 debug);
		alreadysent |= pref_codec;
	}
	for (x = 0; x < 32; x++) {
		if (!(pref_codec = ast_codec_pref_index_video(&p->userprefs, x)))
			break; 

		if (!(capability->video_bits & pref_codec) ||
		    (alreadysent & pref_codec))
		{
			continue;
		}
		add_codec_to_sdp(p, pref_codec, 90000,
				 &m_video_next, &m_video_left,
				 &a_video_next, &a_video_left,
				 debug);
		alreadysent |= pref_codec;
	}
	/* Now send any other common codecs, and non-codec formats: */
	for (x = 0 ; x < 32 ; x++) {

		if(!(pref_codec = ast_codec_pref_index_audio(capability, x)))
			break;
		if (alreadysent & pref_codec)
			continue;

		add_codec_to_sdp(p, pref_codec, 8000,
				 &m_audio_next, &m_audio_left,
				 &a_audio_next, &a_audio_left,
				 debug);
		alreadysent |= pref_codec;
	}
	for (x = 0 ; x < 32 ; x++) {
		if(!(pref_codec = ast_codec_pref_index_video(capability, x)))
			break;
		if (alreadysent & pref_codec)
			continue;

		add_codec_to_sdp(p, pref_codec, 90000,
				 &m_video_next, &m_video_left,
				 &a_video_next, &a_video_left,
				 debug);
		alreadysent |= pref_codec;
	}

	for (x = 1; x <= AST_RTP_MAX; x <<= 1) {
		if (!(p->noncodeccapability & x))
			continue;

		add_noncodec_to_sdp(p, x, 8000,
				    &m_audio_next, &m_audio_left,
				    &a_audio_next, &a_audio_left,
				    debug);
	}

	if (p->userprefs.audio_order[0].ptime_list[0])
	{
		ast_build_string(&a_audio_next, &a_audio_left, "a=ptime:%d\r\n", 
			p->userprefs.audio_order[0].ptime_list[0]);
		ast_log(LOG_DEBUG, "SIP DSP audio packet size: %d\n", 
			p->userprefs.audio_order[0].ptime_list[0]);
	}

	if ((m_audio_left < 2) || (m_video_left < 2) || (a_audio_left == 0) || (a_video_left == 0))
		ast_log(LOG_WARNING, "SIP SDP may be truncated due to undersized buffer!!\n");

	ast_build_string(&m_audio_next, &m_audio_left, "\r\n");
	ast_build_string(&m_video_next, &m_video_left, "\r\n");

	len = strlen(v) + strlen(s) + strlen(o) + strlen(c) + strlen(t) + strlen(m_audio) + strlen(a_audio) + strlen(hold);
	if ((p->vrtp) && (!ast_test_flag(p, SIP_NOVIDEO)) && capability->video_bits) /* only if video response is appropriate */
		len += strlen(m_video) + strlen(a_video) + strlen(hold);
	if (has_t38)
		len += strlen(m_modem);

	add_header(resp, "Content-Type", "application/sdp");
	add_header_contentLength(resp, len);
	add_line(resp, v);
	add_line(resp, o);
	add_line(resp, s);
	add_line(resp, c);
	add_line(resp, t);
	for (x = 0; p->offer_m_order[x] && x < SIP_MAX_OFFER_MEDIA; x++)
	{
		switch (p->offer_m_order[x])
		{
		case SIP_MEDIA_AUDIO:
			add_line(resp, m_audio);
			add_line(resp, a_audio);
			add_line(resp, hold);
			break;
		case SIP_MEDIA_VIDEO:
			if ((p->vrtp) && (!ast_test_flag(p, SIP_NOVIDEO)) && capability->video_bits) { /* only if video response is appropriate */
				add_line(resp, m_video);
				add_line(resp, a_video);
				add_line(resp, hold);
			}
			break;
		case SIP_MEDIA_T38:
			add_line(resp, m_modem);
			break;
		default:
			ast_log(LOG_ERROR, "Unknown media type %d!!! BUG!!!\n",
				p->offer_m_order[x]);
		}
	}

	/* Update lastrtprx when we send our SDP */
	time(&p->lastrtprx);
	time(&p->lastrtptx);

	/* reset offer media order */
	memcpy(p->last_offer_m_order, p->offer_m_order, sizeof(p->offer_m_order));
	memset(p->offer_m_order, 0, sizeof(p->offer_m_order));

	if (resp->method == SIP_RESPONSE)
	    ast_clear_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);
	else
	    ast_set_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);
	return 0;
}

/*! \brief  copy_request: copy SIP request (mostly used to save request for responses) ---*/
static void copy_request(struct sip_request *dst, struct sip_request *src)
{
	long offset;
	int x;
	offset = ((void *)dst) - ((void *)src);
	/* First copy stuff */
	memcpy(dst, src, sizeof(*dst));
	/* Now fix pointer arithmetic */
	for (x=0; x < src->headers; x++)
		dst->header[x] += offset;
	for (x=0; x < src->lines; x++)
		dst->line[x] += offset;
}

/*! \brief adapt_session_codec_list: Used in order to modify codec list
		so that we wouldn't send a whole list in SIP 200 OK or reInvite,
		in case we've agreed the codec earlier and it exists in owner->readformat
		of the channel.
---*/
static void adapt_session_codec_list(struct sip_pvt *p)
{
	int has_cn;

	if (!(p->owner->readformat & p->formats.audio_bits
		&& p->formats.audio_bits != p->owner->readformat))
	{
		return;
	}

	has_cn = p->formats.audio_bits & AST_FORMAT_CN;
	ast_codec_pref_init(&p->formats);
	ast_codec_pref_append_ex(&p->formats, p->owner->readformat, NULL, 0);

	if (has_cn)
		ast_codec_pref_append_ex(&p->formats, AST_FORMAT_CN, NULL, 0);
}

/*! \brief  transmit_response_with_sdp: Used for 200 OK and 183 early media ---*/
static int transmit_response_with_sdp(struct sip_pvt *p, char *msg, struct sip_request *req, int retrans, int oldsdp)
{
	struct sip_request resp;
	int seqno;

	if (sscanf(get_header(req, "CSeq"), "%d ", &seqno) != 1) {
		ast_log(LOG_WARNING, "Unable to get seqno from '%s'\n", get_header(req, "CSeq"));
		return -1;
	}

	if (!send_100rel_allowed(p, msg))
	{
	    ast_log(LOG_WARNING, "First packet sent with 100rel still waiting for PRACK; discarding this one:\n%s\n", msg);
	    return -1;
	}

	respprep(&resp, p, msg, req);

	if (p->prack_level == PRACK_LEVEL_REQUIRE)
	{
	    int msgno = atoi(msg); /* Remember - converts up to first non-digit */

	    /* PRACK requires that provisional messages (with the exception of
	     * message 100) be transmitted in a reliable manner. Convert to
	     * reliable transmission. */
	    if ((msgno > 100) && (msgno <= 199)) {
		char rseq[12]; /* large enough to represent 2^32 - 1 */

		if (!p->prack_rseq)
		    p->prack_rseq = ((unsigned int)rand() % 0x7FFFFFFE) + 1;
		p->prack_expected_rack = p->prack_rseq;

		sprintf(rseq, "%u", p->prack_rseq);
		add_header(&resp, "RSeq", rseq);
		add_header(&resp, "Require", "100rel");
		++p->prack_rseq;
		if (p->prack_status == PRACK_NONE)
		    p->prack_status = PRACK_FIRST_ACK_PENDING;

		retrans = 1; /* Transmit reliably */
	    }
	}

	if (p->rtp) {
		try_suggested_sip_codec(p);	
		if (msg[0] == '2') /* "200 OK" response */
			adapt_session_codec_list(p);
		add_sdp(&resp, p, oldsdp);
	} else {
		ast_log(LOG_ERROR, "Can't add SDP to response, since we have no RTP session allocated. Call-ID %s\n", p->callid);
	}
	return send_response(p, &resp, retrans, seqno);
}

#if defined(T38_SUPPORT)
/*--- transmit_response_with_t38_sdp: Used for 200 OK and 183 early media ---*/
static int transmit_response_with_t38_sdp(struct sip_pvt *p, char *msg, struct sip_request *req, int retrans, int oldsdp)
{
	struct sip_request resp;
	int seqno;
	if (sscanf(get_header(req, "CSeq"), "%d ", &seqno) != 1) {
		ast_log(LOG_WARNING, "Unable to get seqno from '%s'\n", get_header(req, "CSeq"));
		return -1;
	}
	respprep(&resp, p, msg, req);
	if (p->udptl) {
		ast_udptl_offered_from_local(p->udptl, 0);
    		add_t38_sdp(&resp, p, oldsdp);
		ast_set_flag(p->owner, AST_FLAG_T38);
	} else {
		ast_log(LOG_ERROR, "Can't add SDP to response, since we have no UDPTL session allocated. Call-ID %s\n", p->callid);
	}
	return send_response(p, &resp, retrans, seqno);
}
#endif

/*! \brief  determine_firstline_parts: parse first line of incoming SIP request */
static int determine_firstline_parts( struct sip_request *req ) 
{
	char *e, *cmd;
	int len;
  
	cmd = ast_skip_blanks(req->header[0]);
	if (!*cmd)
		return -1;
	req->rlPart1 = cmd;
	e = ast_skip_nonblanks(cmd);
	/* Get the command */
	if (*e)
		*e++ = '\0';
	e = ast_skip_blanks(e);
	if ( !*e )
		return -1;

	if ( !strcasecmp(cmd, "SIP/2.0") ) {
		/* We have a response */
		req->rlPart2 = e;
		len = strlen( req->rlPart2 );
		if ( len < 2 ) { 
			return -1;
		}
		ast_trim_blanks(e);
	} else {
		/* We have a request */
		if ( *e == '<' ) { 
			e++;
			if ( !*e ) { 
				return -1; 
			}  
		}
		req->rlPart2 = e;	/* URI */
		if ( ( e= strrchr( req->rlPart2, 'S' ) ) == NULL ) {
			return -1;
		}
		/* XXX maybe trim_blanks() ? */
		while( isspace( *(--e) ) ) {}
		if ( *e == '>' ) {
			*e = '\0';
		} else {
			*(++e)= '\0';
		}
	}
	return 1;
}

/*! \brief  transmit_reinvite_with_sdp: Transmit reinvite with SDP :-) ---*/
/* 	A re-invite is basically a new INVITE with the same CALL-ID and TAG as the
	INVITE that opened the SIP dialogue 
	We reinvite so that the audio stream (RTP) go directly between
	the SIP UAs. SIP Signalling stays with * in the path.

    If oldsdp is TRUE then the SDP version number is not incremented. This
    is needed for Session-Timers so we can send a re-invite to refresh the
    SIP session without modifying the media session. 
*/
static int transmit_reinvite_with_sdp(struct sip_pvt *p, int oldsdp)
{
	if (ast_test_flag(p, SIP_REINVITE_UPDATE))
		return transmit_reinvite(p, oldsdp, 0, SIP_UPDATE);
	else 
		return transmit_reinvite(p, oldsdp, 0, SIP_INVITE);	

}

#if defined(T38_SUPPORT)
/*--- transmit_reinvite_with_t38_sdp: Transmit reinvite with T38 SDP ---*/
/* 	A re-invite is basically a new INVITE with the same CALL-ID and TAG as the
	INVITE that opened the SIP dialogue 
	We reinvite so that the T38 processing can take place.
	SIP Signalling stays with * in the path.
*/
static int transmit_reinvite_with_t38_sdp(struct sip_pvt *p, int oldsdp)
{
	if (ast_test_flag(p, SIP_REINVITE_UPDATE))
		return transmit_reinvite(p, oldsdp, 1, SIP_UPDATE);
	else 
		return transmit_reinvite(p, oldsdp, 1, SIP_INVITE);	
}
#endif

static int transmit_reinvite(struct sip_pvt *p, int oldsdp, int t38, int sipmethod)
{
	struct sip_request req;

	if (sipmethod == SIP_UPDATE && !is_method_allowed(&p->allowed_methods,
		SIP_UPDATE))
	{
		ast_log(LOG_WARNING, "Trying to issue UPDATE that not allowed by remote "
			"side, fallback to INVITE\n");
		sipmethod = SIP_INVITE;
	}	
	reqprep(&req, p, sipmethod, 0, 1);

	add_header(&req, "Allow", p->prack_level != PRACK_LEVEL_NONE && !t38 ?
	    ALLOWED_METHODS_PRACK : ALLOWED_METHODS);
	add_supported_header(p, &req);
	if (sipdebug) {
		if (oldsdp)
			add_header(&req, "X-asterisk-info", "SIP re-invite (Session-Timers)");
		else
			add_header(&req, "X-asterisk-info", "SIP re-invite (RTP bridge/"
				"T38 switchover)");
	}

	if (!t38)
	{
		if (sipmethod == SIP_UPDATE && oldsdp)
		{
			/* For UPATE with oldsdp (when no intention to change media)
			 * send no sdp at all */
			add_header_contentLength(&req, 0);
			add_blank_header(&req);
		}
		else
			add_sdp(&req, p, oldsdp);
	}
#if defined(T38_SUPPORT)	
	else
	{
		ast_udptl_offered_from_local(p->udptl, 1);
		add_t38_sdp(&req, p, oldsdp);
	}
#endif
	/* Use this as the basis */
	copy_request(&p->initreq, &req);
	parse_request(&p->initreq);
	if (sip_debug_test_pvt(p))
		ast_verbose("%d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
	if (sipmethod == SIP_INVITE)
		p->lastinvite = p->ocseq;
	ast_set_flag(p, SIP_OUTGOING);

	/* oldsdp (session timers) need to be "fatal" if retransmit failed -
	 * opposite to rtp-bridge reinvite */  
	return send_request(p, &req, oldsdp ? 2 : 1, p->ocseq);
}

/*! \brief  extract_uri: Check Contact: URI of SIP message ---*/
static void extract_uri(struct sip_pvt *p, struct sip_request *req)
{
	char stripped[256];
	char *c, *n;
	ast_copy_string(stripped, get_header(req, "Contact"), sizeof(stripped));
	c = get_in_brackets(stripped);
	n = strchr(c, ';');
	if (n)
		*n = '\0';
	if (!ast_strlen_zero(c))
		ast_copy_string(p->uri, c, sizeof(p->uri));
}

/*! \brief  build_contact: Build contact header - the contact header we send out ---*/
static void build_contact(struct sip_pvt *p)
{
	char iabuf[INET_ADDRSTRLEN];

	/* Construct Contact: header */
	if (ourport != 5060)	/* Needs to be 5060, according to the RFC */
	{
		char *name = p->clir? NETWORK_ANONYMOUS_USERNAME : p->exten;
		snprintf(p->our_contact, sizeof(p->our_contact), "<sip:%s%s%s:%d>",
			name, ast_strlen_zero(name) ? "" : "@", 
			ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip), ourport);
	}
	else
	{
		char *name = p->clir? NETWORK_ANONYMOUS_USERNAME : p->exten;
		snprintf(p->our_contact, sizeof(p->our_contact), "<sip:%s%s%s>", 
			name, ast_strlen_zero(name) ? "" : "@", 
			ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip));
	}
}

/*! \brief  build_rpid: Build the Remote Party-ID & From using callingpres options ---*/
static void build_rpid(struct sip_pvt *p)
{
	int send_pres_tags = 1;
	const char *privacy=NULL;
	const char *screen=NULL;
	char buf[256];
	const char *clid = default_callerid;
	const char *clin = NULL;
	char iabuf[INET_ADDRSTRLEN];
	const char *fromdomain;

	if (p->rpid || p->rpid_from)
		return;

	if (p->owner && p->owner->cid.cid_num)
		clid = p->owner->cid.cid_num;
	if (p->owner && p->owner->cid.cid_name)
		clin = p->owner->cid.cid_name;
	if (ast_strlen_zero(clin))
		clin = clid;

	switch (p->callingpres) {
	case AST_PRES_ALLOWED_USER_NUMBER_NOT_SCREENED:
		privacy = "off";
		screen = "no";
		break;
	case AST_PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN:
		privacy = "off";
		screen = "pass";
		break;
	case AST_PRES_ALLOWED_USER_NUMBER_FAILED_SCREEN:
		privacy = "off";
		screen = "fail";
		break;
	case AST_PRES_ALLOWED_NETWORK_NUMBER:
		privacy = "off";
		screen = "yes";
		break;
	case AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED:
		privacy = "full";
		screen = "no";
		break;
	case AST_PRES_PROHIB_USER_NUMBER_PASSED_SCREEN:
		privacy = "full";
		screen = "pass";
		break;
	case AST_PRES_PROHIB_USER_NUMBER_FAILED_SCREEN:
		privacy = "full";
		screen = "fail";
		break;
	case AST_PRES_PROHIB_NETWORK_NUMBER:
		privacy = "full";
		screen = "pass";
		break;
	case AST_PRES_NUMBER_NOT_AVAILABLE:
		send_pres_tags = 0;
		break;
	default:
		ast_log(LOG_WARNING, "Unsupported callingpres (%d)\n", p->callingpres);
		if ((p->callingpres & AST_PRES_RESTRICTION) != AST_PRES_ALLOWED)
			privacy = "full";
		else
			privacy = "off";
		screen = "no";
		break;
	}
	
	fromdomain = ast_strlen_zero(p->fromdomain) ? ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip) : p->fromdomain;

	snprintf(buf, sizeof(buf), "\"%s\" <sip:%s@%s>", clin, clid, fromdomain);
	if (send_pres_tags)
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ";privacy=%s;screen=%s", privacy, screen);
	p->rpid = strdup(buf);

	snprintf(buf, sizeof(buf), "\"%s\" <sip:%s@%s>;tag=%s", clin,
		 ast_strlen_zero(p->fromuser) ? clid : p->fromuser,
		 fromdomain, p->tag);
	p->rpid_from = strdup(buf);
}

/*! \brief  initreqprep: Initiate new SIP request to peer/user ---*/
static void initreqprep(struct sip_request *req, struct sip_pvt *p, int sipmethod)
{
	char invite_buf[256] = "";
	char *invite = invite_buf;
	size_t invite_max = sizeof(invite_buf);
	char from[256];
	char display_name[256] = ""; 
	char to[256];
	char tmp[BUFSIZ/2];
	char tmp2[BUFSIZ/2];
	char iabuf[INET_ADDRSTRLEN];
	char *l = NULL, *n = NULL;
	int x;
	char urioptions[256]="";
	const char *fromdomain, *fromuri;
	int need_domain;

	if (ast_test_flag(p, SIP_USEREQPHONE)) {
	 	char onlydigits = 1;
		x=0;

		/* Test p->username against allowed characters in AST_DIGIT_ANY
		If it matches the allowed characters list, then sipuser = ";user=phone"
		If not, then sipuser = ""
        	*/
        	/* + is allowed in first position in a tel: uri */
        	if (p->username && p->username[0] == '+')
			x=1;

		for (; x < strlen(p->username); x++) {
			if (!strchr(AST_DIGIT_ANYNUM, p->username[x])) {
                		onlydigits = 0;
				break;
			}
		}

		/* If we have only digits, add ;user=phone to the uri */
		if (onlydigits)
			strcpy(urioptions, ";user=phone");
	}


	snprintf(p->lastmsg, sizeof(p->lastmsg), "Init: %s", sip_methods[sipmethod].text);

	if (p->owner) {
		l = p->owner->cid.cid_num;
		n = p->owner->cid.cid_name;
	}
	/* if we are not sending RPID and user wants his callerid restricted */
	if (!ast_test_flag(p, SIP_SENDRPID) && ((p->callingpres & AST_PRES_RESTRICTION) != AST_PRES_ALLOWED)) {
		l = CALLERID_UNKNOWN;
		n = l;
	}
	if (!l)
		l = default_callerid;
	/* Allow user to be overridden */
	if (!ast_strlen_zero(p->fromuser))
		l = p->fromuser;
	else /* Save for any further attempts */
		ast_copy_string(p->fromuser, l, sizeof(p->fromuser));
	if (ast_strlen_zero(n))
		n = l;

	/* Allow user to be overridden */
	if (!ast_strlen_zero(p->fromname))
		n = p->fromname;
	else /* Save for any further attempts */
		ast_copy_string(p->fromname, n, sizeof(p->fromname));

	if (pedanticsipchecking) {
		ast_uri_encode(n, tmp, sizeof(tmp), 0);
		n = tmp;
		ast_uri_encode(l, tmp2, sizeof(tmp2), 0);
		l = tmp2;
	}

	fromdomain = p->clir? NETWORK_ANONYMOUS_DOMAIN : 
		(ast_strlen_zero(p->fromdomain) ? ast_inet_ntoa(iabuf, sizeof(iabuf),
			p->ourip) : p->fromdomain);
	fromuri = p->clir ? NETWORK_ANONYMOUS_USERNAME : 
		(ast_strlen_zero(p->fromuri) ? l : p->fromuri);
	need_domain = !strchr(fromuri, '@');

	if (p->displayinfo)
	{
		snprintf(display_name, sizeof(display_name), "\"%s\" ",
			p->clir ? NETWORK_ANONYMOUS_CALLERID: n);
	}

	if ((ourport != 5060) && ast_strlen_zero(p->fromdomain))	/* Needs to be 5060 */
		snprintf(from, sizeof(from), "%s<sip:%s%s%s:%d>;tag=%s", display_name, fromuri, need_domain ? "@" : "", need_domain ? fromdomain : "", ourport, p->tag);
	else
		snprintf(from, sizeof(from), "%s<sip:%s%s%s>;tag=%s", display_name, fromuri, need_domain ? "@" : "", need_domain ? fromdomain : "", p->tag);

	/* If we're calling a registered SIP peer, use the fullcontact to dial to the peer */
	if (!ast_strlen_zero(p->fullcontact)) {
		/* If we have full contact, trust it */
		ast_build_string(&invite, &invite_max, "%s", p->fullcontact);
	} else {
		/* Otherwise, use the username while waiting for registration */
		ast_build_string(&invite, &invite_max, "sip:");
		if (!ast_strlen_zero(p->username)) {
			n = p->username;
			if (pedanticsipchecking) {
				ast_uri_encode(n, tmp, sizeof(tmp), 1);
				n = tmp;
			}
			ast_build_string(&invite, &invite_max, "%s@", n);
		}
		ast_build_string(&invite, &invite_max, "%s", 
			!ast_strlen_zero(p->todomain) ? p->todomain : p->tohost);
		if (p->toport != 5060)		/* Needs to be 5060 */
			ast_build_string(&invite, &invite_max, ":%d", p->toport);
		ast_build_string(&invite, &invite_max, "%s", urioptions);
	}

	/* If custom URI options have been provided, append them */
	if (p->options && p->options->uri_options)
		ast_build_string(&invite, &invite_max, ";%s", p->options->uri_options);

	ast_copy_string(p->uri, invite_buf, sizeof(p->uri));

	/* If there is a VXML URL append it to the SIP URL */
	if (p->options && p->options->vxml_url) {
		snprintf(to, sizeof(to), "<%s>;%s", p->uri, p->options->vxml_url);
	} else {
		snprintf(to, sizeof(to), "<%s>", p->uri);
	}
	memset(req, 0, sizeof(struct sip_request));
	init_req(req, sipmethod, p->uri);
	snprintf(tmp, sizeof(tmp), "%d %s", ++p->ocseq, sip_methods[sipmethod].text);

	add_header(req, "Via", p->via);
	/* SLD: FIXME?: do Route: here too?  I think not cos this is the first request.
	 * OTOH, then we won't have anything in p->route anyway */
	/* Build Remote Party-ID and From */
	if (ast_test_flag(p, SIP_SENDRPID) && (sipmethod == SIP_INVITE)) {
		build_rpid(p);
		add_header(req, "From", p->rpid_from);
	} else {
		add_header(req, "From", from);
	}
	add_header(req, "To", to);
	ast_copy_string(p->exten, l, sizeof(p->exten));
	build_contact(p);
	add_header(req, "Contact", p->our_contact);
	add_header(req, "Call-ID", p->callid);
	add_header(req, "CSeq", tmp);
	add_header(req, "User-Agent", default_useragent);
	add_header(req, "Max-Forwards", DEFAULT_MAX_FORWARDS);
	add_accept_header(req);
	if (p->rpid)
		add_header(req, "Remote-Party-ID", p->rpid);
}

/*--- transmit_prack: Build PRACK message and transmit it ---*/
static int transmit_prack(struct sip_pvt *p, int sdp, struct sip_request *resp)
{
    struct sip_request req;
    char *cseq = get_header(resp, "CSeq");
    char  rack[128];

    reqprep(&req, p, SIP_PRACK, 0, 1);

#ifdef OSP_SUPPORT
	if (options && options->osptoken && !ast_strlen_zero(options->osptoken)) {
		ast_log(LOG_DEBUG,"Adding OSP Token: %s\n", options->osptoken);
		add_header(&req, "P-OSP-Auth-Token", options->osptoken);
	} else {
		ast_log(LOG_DEBUG,"NOT Adding OSP Token\n");
	}
#endif
	sprintf(rack, "%d %s", p->prack_rack, cseq);
	add_header(&req, "RAck", rack);
	++p->prack_rack;
	
	if (sdp) {
		ast_rtp_offered_from_local(p->rtp, 1);
		add_sdp(&req, p, 0);
	} else {
		add_header(&req, "Content-Length", "0");
		add_blank_header(&req);
	}

	p->lastprack = p->ocseq;
	return send_request(p, &req, 1, p->ocseq);
}

/* set P-Preferred-Identity */
static int build_ppi(struct sip_request *req, struct sip_pvt *p)
{
	char addr_spec[256];
	char iabuf[INET_ADDRSTRLEN];

	snprintf(addr_spec, sizeof(addr_spec), "<sip:%s@%s>", p->peername, 
		ast_strlen_zero(p->fromdomain) ? ast_inet_ntoa(iabuf, sizeof(iabuf),
		p->ourip) : p->fromdomain);
	add_header(req, "P-Preferred-Identity", addr_spec);
	
	return 0;
}

/*! \brief  transmit_invite: Build REFER/INVITE/OPTIONS message and transmit it ---*/
static int transmit_invite(struct sip_pvt *p, int sipmethod, int sdp, int init)
{
	struct sip_request req;
	
	req.method = sipmethod;
	if (init) {
		/* Bump branch even on initial requests */
		p->branch ^= thread_safe_rand();
		p->invite_branch = p->branch;
		build_via(p, p->via, sizeof(p->via));
		if (init > 1)
			initreqprep(&req, p, sipmethod);
		else
			reqprep(&req, p, sipmethod, 0, 1);
	} else
		reqprep(&req, p, sipmethod, 0, 1);
		
	if (p->options && p->options->auth)
		add_header(&req, p->options->authheader, p->options->auth);
	append_date(&req);
	if (sipmethod == SIP_REFER) {	/* Call transfer */
		if (!ast_strlen_zero(p->refer_to))
			add_header(&req, "Refer-To", p->refer_to);
		if (!ast_strlen_zero(p->referred_by))
			add_header(&req, "Referred-By", p->referred_by);
	}
#ifdef OSP_SUPPORT
	if ((req.method != SIP_OPTIONS) && p->options && !ast_strlen_zero(p->options->osptoken)) {
		ast_log(LOG_DEBUG,"Adding OSP Token: %s\n", p->options->osptoken);
		add_header(&req, "P-OSP-Auth-Token", p->options->osptoken);
	}
#endif
	if (!ast_strlen_zero(p->organization))
		add_header(&req, "Organization", p->organization);
	if (p->options && !ast_strlen_zero(p->options->distinctive_ring))
	{
		add_header(&req, "Alert-Info", p->options->distinctive_ring);
	}
	/* add P-Preferred-Identity and Privacy headers, if needed */
	if (use_asserted_identity && p->clir)
	{
		build_ppi(&req, p);
		add_header(&req, "Privacy", "user;id");
	}
	/* Add Session-Timers related headers */
	if (st_get_mode(p, 0) == SESSION_TIMER_MODE_ORIGINATE) {
		char i2astr[30];

		if (!p->stimer->st_interval)
			p->stimer->st_interval = st_get_se(p, TRUE);

		if (!p->stimer->st_min_se)
			p->stimer->st_min_se = st_get_se(p, FALSE);

		p->stimer->st_active = TRUE;
		
		snprintf(i2astr, sizeof(i2astr), "%d", MAX(p->stimer->st_interval, p->stimer->st_min_se));
		/* We add "refresher" to outgoing initial INVITE, which is not
		 * recommended by RFC4028, but allowed with "uac", if we want to force
		 * ourselves to be the refresher.
		 * However, we may also set "refresher=uas" if st_refresher is
		 * configured for "REMOTE". This is not valid since we don't yet know
		 * if the UAS supports session timers, thus we shouldn't force him to be
		 * the refresher. Nevertheless, this is what Vodafone IT want. (B122249)
		 */
                if (global_st_refresher_force)
                {
                        snprintf(i2astr+strlen(i2astr), sizeof(i2astr)-strlen(i2astr), ";refresher=%s",
                                strefresher2header(st_get_refresher(p), 0));
                }
		add_header(&req, "Session-Expires", i2astr);
		snprintf(i2astr, sizeof(i2astr), "%d", p->stimer->st_min_se);
		add_header(&req, "Min-SE", i2astr);
	}

	add_header(&req, "Allow", p->prack_level != PRACK_LEVEL_NONE ?
	    ALLOWED_METHODS_PRACK : ALLOWED_METHODS);
	add_supported_header(p, &req);
	if (p->options && p->options->addsipheaders ) {
		struct ast_channel *ast;
		char *header = (char *) NULL;
		char *content = (char *) NULL;
		char *end = (char *) NULL;
		struct varshead *headp = (struct varshead *) NULL;
		struct ast_var_t *current;

		ast = p->owner;	/* The owner channel */
		if (ast) {
			char *headdup;
	 		headp = &ast->varshead;
			if (!headp)
				ast_log(LOG_WARNING,"No Headp for the channel...ooops!\n");
			else {
				AST_LIST_TRAVERSE(headp, current, entries) {  
					/* SIPADDHEADER: Add SIP header to outgoing call        */
					if (!strncasecmp(ast_var_name(current), "SIPADDHEADER", strlen("SIPADDHEADER"))) {
						header = ast_var_value(current);
						headdup = ast_strdupa(header);
						/* Strip of the starting " (if it's there) */
						if (*headdup == '"')
					 		headdup++;
						if ((content = strchr(headdup, ':'))) {
							*content = '\0';
							content++;	/* Move pointer ahead */
							/* Skip white space */
							while (*content == ' ')
						  		content++;
							/* Strip the ending " (if it's there) */
					 		end = content + strlen(content) -1;	
							if (*end == '"')
								*end = '\0';
						
							add_header(&req, headdup, content);
							if (sipdebug)
								ast_log(LOG_DEBUG, "Adding SIP Header \"%s\" with content :%s: \n", headdup, content);
						}
					}
				}
			}
		}
	}
	
	if (p->prack_level == PRACK_LEVEL_REQUIRE) {
	    add_header(&req, "Require", "100rel");
	    if (sipdebug)
		ast_log(LOG_DEBUG, "Adding SIP Header \"Require\" with content :100rel: \n");
	}

#if defined(T38_SUPPORT)
	if (sdp && (p->udptl) && ((p->t38state == T38_LOCAL_DIRECT) || (p->t38state == T38_LOCAL_REINVITE))) {
		ast_udptl_offered_from_local(p->udptl, 1);
		ast_log(LOG_DEBUG, "T38 is in state %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
		add_t38_sdp(&req, p, FALSE);
	} else if (sdp && p->rtp) {
#else
	if (sdp && p->rtp) {
#endif
		add_sdp(&req, p, FALSE);
	} else {
		add_header_contentLength(&req, 0);
		add_blank_header(&req);
	}

	if (!p->initreq.headers) {
		/* Use this as the basis */
		copy_request(&p->initreq, &req);
		parse_request(&p->initreq);
		if (sip_debug_test_pvt(p))
			ast_verbose("%d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
	}
	p->lastinvite = p->ocseq;
	return send_request(p, &req, init ? 2 : 1, p->ocseq);
}

/*! \brief  transmit_state_notify: Used in the SUBSCRIBE notification subsystem ----*/
static int transmit_state_notify(struct sip_pvt *p, int state, int full, int substate)
{
	char tmp[4000], from[256], to[256];
	char *t = tmp, *c, *a, *mfrom, *mto;
	size_t maxbytes = sizeof(tmp);
	struct sip_request req;
	char hint[AST_MAX_EXTENSION];
	char *statestring = "terminated";
	const struct cfsubscription_types *subscriptiontype;
	enum state { NOTIFY_OPEN, NOTIFY_INUSE, NOTIFY_CLOSED } local_state = NOTIFY_OPEN;
	char *pidfstate = "--";
	char *pidfnote= "Ready";

	memset(from, 0, sizeof(from));
	memset(to, 0, sizeof(to));
	memset(tmp, 0, sizeof(tmp));

	switch (state) {
	case (AST_EXTENSION_RINGING | AST_EXTENSION_INUSE):
		if (global_notifyringing)
			statestring = "early";
		else
			statestring = "confirmed";
		local_state = NOTIFY_INUSE;
		pidfstate = "busy";
		pidfnote = "Ringing";
		break;
	case AST_EXTENSION_RINGING:
		statestring = "early";
		local_state = NOTIFY_INUSE;
		pidfstate = "busy";
		pidfnote = "Ringing";
		break;
	case AST_EXTENSION_INUSE:
		statestring = "confirmed";
		local_state = NOTIFY_INUSE;
		pidfstate = "busy";
		pidfnote = "On the phone";
		break;
	case AST_EXTENSION_BUSY:
		statestring = "confirmed";
		local_state = NOTIFY_CLOSED;
		pidfstate = "busy";
		pidfnote = "On the phone";
		break;
	case AST_EXTENSION_UNAVAILABLE:
		statestring = "confirmed";
		local_state = NOTIFY_CLOSED;
		pidfstate = "away";
		pidfnote = "Unavailable";
		break;
	case AST_EXTENSION_NOT_INUSE:
	default:
		/* Default setting */
		break;
	}

	subscriptiontype = find_subscription_type(p->subscribed);
	
	/* Check which device/devices we are watching  and if they are registered */
	if (ast_get_hint(hint, sizeof(hint), NULL, 0, NULL, p->context, p->exten)) {
		/* If they are not registered, we will override notification and show no availability */
		if (ast_device_state(hint) == AST_DEVICE_UNAVAILABLE) {
			local_state = NOTIFY_CLOSED;
			pidfstate = "away";
			pidfnote = "Not online";
		}
	}

	ast_copy_string(from, get_header(&p->initreq, "From"), sizeof(from));
	c = get_in_brackets(from);
	if (strncmp(c, "sip:", 4)) {
		ast_log(LOG_WARNING, "Huh?  Not a SIP header (%s)?\n", c);
		return -1;
	}
	if ((a = strchr(c, ';')))
		*a = '\0';
	mfrom = c;

	ast_copy_string(to, get_header(&p->initreq, "To"), sizeof(to));
	c = get_in_brackets(to);
	if (strncmp(c, "sip:", 4)) {
		ast_log(LOG_WARNING, "Huh?  Not a SIP header (%s)?\n", c);
		return -1;
	}
	if ((a = strchr(c, ';')))
		*a = '\0';
	mto = c;

	reqprep(&req, p, SIP_NOTIFY, 0, 1);

	
	add_header(&req, "Event", subscriptiontype->event);
	add_header(&req, "Content-Type", subscriptiontype->mediatype);
	switch(state) {
	case AST_EXTENSION_DEACTIVATED:
		if (p->subscribed == TIMEOUT)
			add_header(&req, "Subscription-State", "terminated;reason=timeout");
		else {
			add_header(&req, "Subscription-State", "terminated;reason=probation");
			add_header(&req, "Retry-After", "60");
		}
		break;
	case AST_EXTENSION_REMOVED:
		add_header(&req, "Subscription-State", "terminated;reason=noresource");
		break;
		break;
	default:
		if (p->expiry)
			add_header(&req, "Subscription-State", "active");
		else	/* Expired */
			add_header(&req, "Subscription-State", "terminated;reason=timeout");
	}
	switch (p->subscribed) {
	case XPIDF_XML:
	case CPIM_PIDF_XML:
		ast_build_string(&t, &maxbytes, "<?xml version=\"1.0\"?>\n");
		ast_build_string(&t, &maxbytes, "<!DOCTYPE presence PUBLIC \"-//IETF//DTD RFCxxxx XPIDF 1.0//EN\" \"xpidf.dtd\">\n");
		ast_build_string(&t, &maxbytes, "<presence>\n");
		ast_build_string(&t, &maxbytes, "<presentity uri=\"%s;method=SUBSCRIBE\" />\n", mfrom);
		ast_build_string(&t, &maxbytes, "<atom id=\"%s\">\n", p->exten);
		ast_build_string(&t, &maxbytes, "<address uri=\"%s;user=ip\" priority=\"0.800000\">\n", mto);
		ast_build_string(&t, &maxbytes, "<status status=\"%s\" />\n", (local_state ==  NOTIFY_OPEN) ? "open" : (local_state == NOTIFY_INUSE) ? "inuse" : "closed");
		ast_build_string(&t, &maxbytes, "<msnsubstatus substatus=\"%s\" />\n", (local_state == NOTIFY_OPEN) ? "online" : (local_state == NOTIFY_INUSE) ? "onthephone" : "offline");
		ast_build_string(&t, &maxbytes, "</address>\n</atom>\n</presence>\n");
		break;
	case PIDF_XML: /* Eyebeam supports this format */
		ast_build_string(&t, &maxbytes, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n");
		ast_build_string(&t, &maxbytes, "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" \nxmlns:pp=\"urn:ietf:params:xml:ns:pidf:person\"\nxmlns:es=\"urn:ietf:params:xml:ns:pidf:rpid:status:rpid-status\"\nxmlns:ep=\"urn:ietf:params:xml:ns:pidf:rpid:rpid-person\"\nentity=\"%s\">\n", mfrom);
		ast_build_string(&t, &maxbytes, "<pp:person><status>\n");
		if (pidfstate[0] != '-')
			ast_build_string(&t, &maxbytes, "<ep:activities><ep:%s/></ep:activities>\n", pidfstate);
		ast_build_string(&t, &maxbytes, "</status></pp:person>\n");
		ast_build_string(&t, &maxbytes, "<note>%s</note>\n", pidfnote); /* Note */
		ast_build_string(&t, &maxbytes, "<tuple id=\"%s\">\n", p->exten); /* Tuple start */
		ast_build_string(&t, &maxbytes, "<contact priority=\"1\">%s</contact>\n", mto);
		if (pidfstate[0] == 'b') /* Busy? Still open ... */
			ast_build_string(&t, &maxbytes, "<status><basic>open</basic></status>\n");
		else
			ast_build_string(&t, &maxbytes, "<status><basic>%s</basic></status>\n", (local_state != NOTIFY_CLOSED) ? "open" : "closed");
		ast_build_string(&t, &maxbytes, "</tuple>\n</presence>\n");
		break;
	case DIALOG_INFO_XML: /* SNOM subscribes in this format */
		ast_build_string(&t, &maxbytes, "<?xml version=\"1.0\"?>\n");
		ast_build_string(&t, &maxbytes, "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"%d\" state=\"%s\" entity=\"%s\">\n", p->dialogver++, full ? "full":"partial", mto);
		if ((state & AST_EXTENSION_RINGING) && global_notifyringing)
			ast_build_string(&t, &maxbytes, "<dialog id=\"%s\" direction=\"recipient\">\n", p->exten);
		else
			ast_build_string(&t, &maxbytes, "<dialog id=\"%s\">\n", p->exten);
		ast_build_string(&t, &maxbytes, "<state>%s</state>\n", statestring);
		ast_build_string(&t, &maxbytes, "</dialog>\n</dialog-info>\n");
		break;
	case NONE:
	default:
		break;
	}

	if (t > tmp + sizeof(tmp))
		ast_log(LOG_WARNING, "Buffer overflow detected!!  (Please file a bug report)\n");

	add_header_contentLength(&req, strlen(tmp));
	add_line(&req, tmp);

	return send_request(p, &req, 1, p->ocseq);
}

/*! \brief  transmit_notify_with_mwi: Notify user of messages waiting in voicemail ---*/
/*      Notification only works for registered peers with mailbox= definitions
 *      in sip.conf
 *      We use the SIP Event package message-summary
 *      MIME type defaults to  "application/simple-message-summary";
 */
static int transmit_notify_with_mwi(struct sip_pvt *p, int newmsgs, int oldmsgs, char *vmexten)
{
	struct sip_request req;
	char tmp[500];
	char *t = tmp;
	size_t maxbytes = sizeof(tmp);
	char iabuf[INET_ADDRSTRLEN];

	initreqprep(&req, p, SIP_NOTIFY);
	add_header(&req, "Event", "message-summary");
	add_header(&req, "Content-Type", default_notifymime);

	ast_build_string(&t, &maxbytes, "Messages-Waiting: %s\r\n", newmsgs ? "yes" : "no");
	ast_build_string(&t, &maxbytes, "Message-Account: sip:%s@%s\r\n", !ast_strlen_zero(vmexten) ? vmexten : global_vmexten, ast_strlen_zero(p->fromdomain) ? ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip) : p->fromdomain);
	ast_build_string(&t, &maxbytes, "Voice-Message: %d/%d (0/0)\r\n", newmsgs, oldmsgs);

	if (t > tmp + sizeof(tmp))
		ast_log(LOG_WARNING, "Buffer overflow detected!!  (Please file a bug report)\n");

	add_header_contentLength(&req, strlen(tmp));
	add_line(&req, tmp);

	if (!p->initreq.headers) { /* Use this as the basis */
		copy_request(&p->initreq, &req);
		parse_request(&p->initreq);
		if (sip_debug_test_pvt(p))
			ast_verbose("%d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
		determine_firstline_parts(&p->initreq);
	}

	return send_request(p, &req, 1, p->ocseq);
}

/*! \brief  transmit_sip_request: Transmit SIP request */
static int transmit_sip_request(struct sip_pvt *p,struct sip_request *req, int reliable)
{
	if (!p->initreq.headers) {
		/* Use this as the basis */
		copy_request(&p->initreq, req);
		parse_request(&p->initreq);
		if (sip_debug_test_pvt(p))
			ast_verbose("%d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
		determine_firstline_parts(&p->initreq);
	}

	return send_request(p, req, reliable, p->ocseq);
}

/*! \brief  transmit_notify_with_sipfrag: Notify a transferring party of the status of trasnfer ---*/
/*      Apparently the draft SIP REFER structure was too simple, so it was decided that the
 *      status of transfers also needed to be sent via NOTIFY instead of just the 202 Accepted
 *      that had worked heretofore.
 */
static int transmit_notify_with_sipfrag(struct sip_pvt *p, int cseq)
{
	struct sip_request req;
	char tmp[20];
	reqprep(&req, p, SIP_NOTIFY, 0, 1);
	snprintf(tmp, sizeof(tmp), "refer;id=%d", cseq);
	add_header(&req, "Event", tmp);
	add_header(&req, "Subscription-state", "terminated;reason=noresource");
	add_header(&req, "Content-Type", "message/sipfrag;version=2.0");
	add_supported_header(p, &req);

	strcpy(tmp, "SIP/2.0 200 OK");
	add_header_contentLength(&req, strlen(tmp));
	add_line(&req, tmp);

	if (!p->initreq.headers) {
		/* Use this as the basis */
		copy_request(&p->initreq, &req);
		parse_request(&p->initreq);
		if (sip_debug_test_pvt(p))
			ast_verbose("%d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
		determine_firstline_parts(&p->initreq);
	}

	return send_request(p, &req, 1, p->ocseq);
}

static char *substate2str(int substate)
{
	switch(substate) {
	case SUB_STATE_FAILED:
		return "Failed";
	case SUB_STATE_UNSUBSCRIBED:
		return "Unsubscribed";
	case SUB_STATE_SUBSENT:
		return "Request Sent";
	case SUB_STATE_AUTHSENT:
		return "Auth. Sent";
	case SUB_STATE_SUBSCRIBED:
		return "Subscribed";
	case SUB_STATE_REJECTED:
		return "Rejected";
	case SUB_STATE_TIMEOUT:
		return "Timeout";
	case SUB_STATE_NOAUTH:
		return "No Authentication";
	default:
		return "Unknown";
	}
}

/* Status strings are used in enums.h:voip_status_t enum */
static char *regstate2str(int regstate)
{
	switch(regstate) {
	case REG_STATE_FAILED:
		return "Failed";
	case REG_STATE_UNREGISTERED:
		return "Unregistered";
	case REG_STATE_REGSENT:
		return "Request Sent";
	case REG_STATE_AUTHSENT:
		return "Auth. Sent";
	case REG_STATE_REGISTERED:
		return "Registered";
	case REG_STATE_REJECTED:
		return "Rejected";
	case REG_STATE_TIMEOUT:
		return "Timeout";
	case REG_STATE_NOAUTH:
		return "No Authentication";
	default:
		return "Unknown";
	}
}

static int transmit_register(struct sip_registry *r, int sipmethod, char *auth,
char *authheader, int unregister);
static int transmit_subscribe(struct sip_subscription *s, int sipmethod, char *auth, char *authheader);

/*--- sip_resubscribe: Update subscription with SIP notifiers---*/
static int sip_resubscribe(void *data) 
{
	/* if we are here, we know that we need to resubscribe. */
	struct sip_subscription *s= ASTOBJ_REF((struct sip_subscription *) data);

	/* if we couldn't get a reference to the subscription object, punt */
	if (!s)
		return 0;

	if (sipdebug)
		ast_log(LOG_NOTICE, "   -- Re-subscribing for  %s@%s\n", s->username, s->hostname);

	s->expire = -1;
	__sip_do_subscribe(s);
	ASTOBJ_UNREF(s,sip_subscription_destroy);
	return 0;
}

/*! \brief  sip_reregister: Update registration with SIP Proxy---*/
static int sip_reregister(void *data, int unregister, int is_expire)
{
	/* if we are here, we know that we need to reregister. */
	struct sip_registry *r= ASTOBJ_REF((struct sip_registry *) data);

	/* if we couldn't get a reference to the registry object, punt */
	if (!r)
		return 0;

	if (r->call && recordhistory) {
		char tmp[80];
		snprintf(tmp, sizeof(tmp), "Account: %s@%s", r->username, r->hostname);
		append_history(r->call, "RegistryRenew", tmp);
	}
	/* Since registry's are only added/removed by the the monitor thread, this
	   may be overkill to reference/dereference at all here */
	if (sipdebug)
		ast_log(LOG_NOTICE, "   -- Re-registration for  %s@%s\n", r->username, r->hostname);

	if (is_expire)
		r->expire = -1;
	else
		r->timeout = -1;
	__sip_do_register(r, unregister);
	ASTOBJ_UNREF(r, sip_registry_destroy);
	return 0;
}

/*! \brief sip_timeout_redoregister: Update registeration on timeout event*/
static int sip_timeout_redoregister(void *data)
{
    return sip_reregister(data, 0, 0);
}

/*! \brief sip_expire_redoregister: Update registeration on expire event*/
static int sip_expire_redoregister(void *data)
{
    return sip_reregister(data, 0, 1);
}

static int sip_reunregister(void *data)
{
    return sip_reregister(data, 1, 1);
}

/*--- __sip_do_subscribe: Subscribe with SIP notifier ---*/
static int __sip_do_subscribe(struct sip_subscription *s)
{
	int res;
	res=transmit_subscribe(s, SIP_SUBSCRIBE, NULL, NULL);
	return res;
}

/*! \brief  __sip_do_register: Register with SIP proxy ---*/
static int __sip_do_register(struct sip_registry *r, int unregister)
{
	int res;

	res = transmit_register(r, SIP_REGISTER, NULL, NULL, unregister);
	return res;
}

/*--- sip_sub_timeout: Subscription timeout, subscribe again */
static int sip_sub_timeout(void *data)
{

	/* if we are here, our subscription timed out, so we'll just do it over */
	struct sip_subscription *s = ASTOBJ_REF((struct sip_subscription *) data);
	struct sip_pvt *p;
	int res;

	/* if we couldn't get a reference to the subscription object, punt */
	if (!s)
		return 0;

	ast_log(LOG_NOTICE, "   -- Subscription for '%s@%s' timed out, trying again (Attempt #%d)\n", s->username, s->hostname, s->subattempts); 
	if (s->call) {
		/* Unlink us, destroy old call.  Locking is not relevent here because all this happens
		   in the single SIP manager thread. */
		p = s->call;
		if (p->subscription)
			ASTOBJ_UNREF(p->subscription, sip_subscription_destroy);
		s->call = NULL;
		ast_set_flag(p, SIP_NEEDDESTROY);	
		/* Pretend to ACK anything just in case */
		/* OEJ: Ack what??? */
		__sip_pretend_ack(p);
	}
	/* If we have a limit, stop subscription and give up */
	if (global_subattempts_max && s->subattempts > global_subattempts_max) {
		/* Ok, enough is enough. Don't try any more */
		/* We could add an external notification here... 
			steal it from app_voicemail :-) */
		s->substate=SUB_STATE_FAILED;
	} else {
		s->substate=SUB_STATE_UNSUBSCRIBED;
		s->timeout = -1;
		res=transmit_subscribe(s, SIP_SUBSCRIBE, NULL, NULL);
	}
	manager_event(EVENT_FLAG_SYSTEM, "Subscription", "Channel: SIP\r\nUser: %s\r\nDomain: %s\r\nStatus: %s\r\n", s->username, s->hostname, substate2str(s->substate));
	
	ASTOBJ_UNREF(s,sip_subscription_destroy);
	return 0;
}

static void reg_cancel_call( struct sip_registry * r); 
static int do_proxy_failover(void *data);

static void do_reg_srv_failover(struct sip_registry *r)
{
	int timeout = srv_recover_time;

	r->uas_srv.healthy = 0;
	if (r->uas_srv.context)
	{
		/* Timeout on an SRV record, let's try the next one */
		r->regstate = REG_STATE_UNREGISTERED;
		transmit_register(r, SIP_REGISTER, NULL, NULL, 0);
	}
	else
	{
		/* Timeout on the last SRV record. Let's wait and try again */

		if (global_regattempts_max && r->regattempts >= 
			global_regattempts_max)
		{
			/* Exhusted our regattempts, try a different timeout */
			r->regattempts = 0;
			timeout = reg_recover_time;
		}

		ast_log(LOG_DEBUG, "New register attempt in %d seconds\n", timeout);
		r->timeout = ast_sched_add(sched, timeout * 1000, 
			sip_timeout_redoregister, r);
	}

	manager_event_sip_registry();
	ASTOBJ_UNREF(r, sip_registry_destroy);
}

/*! \brief  sip_reg_timeout: Registration timeout, register again */
static int sip_reg_timeout(void *data)
{

	/* if we are here, our registration timed out, so we'll just do it over */
	struct sip_registry *r = ASTOBJ_REF((struct sip_registry *) data);
	int res;

	/* if we couldn't get a reference to the registry object, punt */
	if (!r)
		return 0;

	ast_log(LOG_NOTICE, "   -- Registration for '%s@%s' timed out, trying again (Attempt #%d)\n", r->username, r->hostname, r->regattempts); 
	set_server_na(r, 1);
	reg_cancel_call(r);

	r->timeout = -1;
	if (r->srv_failover)
	{
		do_reg_srv_failover(r);
		return 0;
	}

	/* If we have a limit, stop registration and give up */
	if (global_regattempts_max && (r->regattempts >= global_regattempts_max)) {
		/* Ok, enough is enough. Don't try any more */
		/* We could add an external notification here... 
			steal it from app_voicemail :-) */
		ast_log(LOG_NOTICE, "   -- Giving up forever trying to register '%s@%s'\n", r->username, r->hostname);
		r->regstate=REG_STATE_FAILED;
	} else {
		r->regstate=REG_STATE_UNREGISTERED;
		res=transmit_register(r, SIP_REGISTER, NULL, NULL, 0);
	}
	//manager_event(EVENT_FLAG_SYSTEM, "Registry", "Channel: SIP\r\nUsername: %s\r\nDomain: %s\r\nStatus: %s\r\n", r->username, r->hostname, regstate2str(r->regstate));
	manager_event_sip_registry();
	ASTOBJ_UNREF(r,sip_registry_destroy);
	return 0;
}

/*! \brief  reg_cancel_call: Cancel a transaction of a register ---*/
static void reg_cancel_call( struct sip_registry * r)
{
	struct sip_pvt *p;

	if (!r->call)
		return;

	ast_log(LOG_DEBUG, "Canceling outbound register message to proxy "
		"'%s'.\n", r->hostname);

	/* Unlink us, destroy old call.  Locking is not relevant here because all
	   this happens in the single SIP manager thread. */
	p = r->call;
	if (p->registry)
		ASTOBJ_UNREF(p->registry, sip_registry_destroy);
	r->call = NULL;
	ast_set_flag(p, SIP_NEEDDESTROY);

	/* Pretend to ACK anything just in case */
	__sip_pretend_ack(p);
}

static int recover_to_primary(void *data)
{
	/* if we are here, our registration timed out, we invoke failover 
	   procedure*/
	struct sip_registry *r = ASTOBJ_REF((struct sip_registry *) data);

	/* if we couldn't get a reference to the registry object, punt */
	if (!r)
		return 0;

	r->sched_recover_primary = -1;

	/* if there are active calls on backup proxy, recover to primary only when
	 * the calls are ended */
	if (r->reg_backup && get_active_calls_count_on_proxy(r->reg_backup) > 0)
	{
		/* mark reg_backup to recover to primary when calls are ended */
		r->reg_backup->need_recover_to_primary = 1;
		ast_log(LOG_DEBUG, "need to recover to primary but "
			"backup proxy %s still has active calls\n", 
			r->reg_backup->name);
	}
	else
	{
		ast_log(LOG_DEBUG, "Trying to recover to Primary Proxy: '%s@%s'\n",
			r->username, r->hostname);
		sip_expire_redoregister(data);
	}
	ASTOBJ_UNREF(r, sip_registry_destroy);
	return 0;
}

/*! \brief  proxy_failover: timeout or error on proxy. init a failover */
static int proxy_failover_timeout(void *data)
{
    int delay;
    struct sip_registry *r = ASTOBJ_REF((struct sip_registry *) data);

    /* if we couldn't get a reference to the registry object, punt */
	if (!r)
	{
		ast_log(LOG_NOTICE, "couldn't get a reference to the registry object"
			"in %p\n", data);
		return 0;
	}
    ast_log(LOG_DEBUG, "Proxy Failover Timeout/Error from %s proxy,"
		"'%s'\n", r->reg_backup ? "PRIMARY" : "BACKUP", r->hostname);

    r->timeout = -1;
    r->regstate = (r->got_response_from_server ? REG_STATE_FAILED :
	REG_STATE_UNREGISTERED);
    reg_cancel_call(r);

    if (r->sched_recover_primary > -1)
    {
		ast_sched_del(sched, r->sched_recover_primary);
		r->sched_recover_primary = -1;
    }

	if (r->reg_backup && r->reg_backup->is_backup_active)
	{
		/* Timeout/Error for primary proxy while Backup proxy is active. 
		   This is not a proxy_failover case but rather a recovery attempt
		   to the primary proxy that has failed. Just restart T2. */
		r->sched_recover_primary = ast_sched_add(sched, 
			global_vf_t2_delay_recover_primary, recover_to_primary, r); 
		ast_log(LOG_DEBUG, "Recovery to primary proxy '%s@%s' has "
			"failed. Schedule another recovery to %dms.\n", r->username,
			r->hostname, global_vf_t2_delay_recover_primary);
		goto Exit;
	}

	if (r->reg_primary)
	{
		if (r->expiry == 0)
		{
			/* Timeout/error while unregistered from backup. As this
			   is unregister, we ignore this error and continue recovery 
			   to primary proxy. */
			ast_log(LOG_DEBUG, "Error/Timeoutfor un-REGISTER to backup"
				" proxy. Schedule reregister to primary proxy in %dms\n",
				global_vf_1s_delay_dereg_backup);
			SCHED_CANCEL(r->reg_primary->sched_recover_primary);
			r->expiry = default_expiry;
			SCHED_CANCEL(r->reg_primary->expire);
			r->reg_primary->expire = ast_sched_add(sched,
				global_vf_1s_delay_dereg_backup,
				sip_expire_redoregister, r->reg_primary); 
			goto Exit;
		}
		else
		{
			/* If Timeout/Error on backup, set peer address (for outbound
			   calls) to primary. note: even if primary is not yet active... */
			r->is_backup_active = 0;
			reg_cancel_call(r->reg_primary);
			r->reg_primary->regstate = REG_STATE_UNREGISTERED;
			SCHED_CANCEL(r->reg_primary->timeout);
			SCHED_CANCEL(r->reg_primary->expire);
			SCHED_CANCEL(r->reg_primary->sched_recover_primary);
		}
	}

	/* setting S1 or S2 delay to start registering with next proxy. */
	delay = r->reg_backup ? global_vf_s1_delay_backup_proxy :
		global_vf_s2_delay_primary_proxy;
	if (r->sched_failover_delay > -1)
		ast_sched_del(sched, r->sched_failover_delay);
	r->sched_failover_delay = ast_sched_add(sched, delay, do_proxy_failover,
		r);
	ast_log(LOG_DEBUG, "Scheduling Failover from proxy '%s' in %dms.\n",
		r->hostname, delay);
    
Exit:
    manager_event_sip_registry();
    ASTOBJ_UNREF(r, sip_registry_destroy);
    return 0;
}

static void reg_after_srv_not_found(struct sip_registry *r, struct sip_pvt *p)
{
	/* we have what we hope is a temporary network error,
	 * probably DNS.  We need to reschedule a registration try */
	sip_destroy(p);
	SCHED_CANCEL(r->timeout);
	sip_reg_timeout(r);
}

static void schedule_reg_after_dns_error(struct sip_registry *r, 
	struct sip_pvt *p)
{
	sip_destroy(p);
	SCHED_CANCEL(r->timeout);

	r->timeout = ast_sched_add(sched, global_reg_timeout * 1000, 
		(r->reg_backup ||r->reg_primary) ? proxy_failover_timeout : 
		sip_reg_timeout, r);
	ast_log(LOG_WARNING, "DNS error - Trying registration again - "
		"timeout %d\n", r->timeout);
	r->regattempts++;
}

/*! \brief  do_proxy_failover: implement to move from one proxy to the other */
static int do_proxy_failover(void *data)
{
	/* if we are here, our registration timed out, we invoke failover 
	   procedure */
	struct sip_registry *r = ASTOBJ_REF((struct sip_registry *) data);

	/* if we couldn't get a reference to the registry object, punt */
	if (!r)
	{
		ast_log(LOG_NOTICE, "couldn't get a reference to the registry object "
			"in %p\n", data);
		return 0;
	}
	r->sched_failover_delay = -1;

	ast_log(LOG_DEBUG, "Failing over to %s proxy, '%s'.\n",
		r->reg_backup ? "BACKUP" : "PRIMARY", r->reg_backup ?
		r->reg_backup->hostname : r->reg_primary->hostname);

	if (r->reg_backup)
		r->reg_backup->is_backup_active = 1;
	sip_expire_redoregister(r->reg_backup ? r->reg_backup : r->reg_primary);

	ASTOBJ_UNREF(r,sip_registry_destroy);
	return 0;
}

/*--- transmit_subscribe: Transmit subscribe to SIP proxy or UA ---*/
static int transmit_subscribe(struct sip_subscription *s, int sipmethod, char *auth, char *authheader)
{
	struct sip_request req;
	char from[256];
	char to[256];
	char tmp[80];
	char via[80];
	char addr[80];
	struct sip_pvt *p;
	char *destaddr;
	int destport;

	/* exit if we are already in process with this notifier ?*/
	if ( s == NULL || ((auth==NULL) && (s->substate==SUB_STATE_SUBSENT || s->substate==SUB_STATE_AUTHSENT))) {
		ast_log(LOG_NOTICE, "Strange, trying to subscribe %s@%s when subscription already pending\n", s->username, s->hostname);
		return 0;
	}

	destaddr = (s->obproxy && *s->obproxy) ? s->obproxy : s->hostname;
	destport = s->obproxyport ? s->obproxyport : s->portno;


	if (s->call) {	/* We have a subscription */
		if (!auth) {
			ast_log(LOG_WARNING, "Already have a SUBSCRIBE going on to %s@%s?? \n", s->username, s->hostname);
			return 0;
		} else {
			p = s->call;
			make_our_tag(p->tag, sizeof(p->tag));   /* create a new local tag for every subscribe attempt */
			p->theirtag[0]='\0';	/* forget their old tag, so we don't match tags when getting response */
		}
	} else {
		/* Build callid for subscription if we haven't subscribed before */
		if (!s->callid_valid) {
			build_callid(s->callid, sizeof(s->callid), __ourip, default_fromdomain);
			s->callid_valid = 1;
		}
		/* Allocate SIP packet for subscription */
		p=sip_alloc( s->callid, NULL, 0, SIP_SUBSCRIBE);
		if (!p) {
			ast_log(LOG_WARNING, "Unable to allocate subscription call\n");
			return 0;
		}
		/* Find address to hostname */
		if (create_addr(p, destaddr)) {
			/* we have what we hope is a temporary network error,
			 * probably DNS.  We need to reschedule a subscription try */
			sip_destroy(p);
			if (s->timeout > -1) {
				ast_sched_del(sched, s->timeout);
				s->timeout = ast_sched_add(sched, default_subscription_expiry*1000, sip_sub_timeout, s);
				ast_log(LOG_WARNING, "Still have a subscription timeout for %s@%s (create_addr() error), %d\n", s->username, s->hostname, s->timeout);
			} else {
				s->timeout = ast_sched_add(sched, default_subscription_expiry*1000, sip_sub_timeout, s);
				ast_log(LOG_WARNING, "Propably a DNS error for subscription to %s@%s, trying SUBSCRIBE again (after %d seconds)\n", s->username, s->hostname, default_subscription_expiry * 1000);
			}
			s->subattempts++;
			return 0;
		}

		/* Copy back Call-ID in case create_addr changed it */
		ast_copy_string(s->callid, p->callid, sizeof(s->callid));
		if (destport)
			p->sa.sin_port = htons(destport);
		ast_set_flag(p, SIP_OUTGOING);	/* Subscription is outgoing call */
		s->call=p;			/* Save pointer to SIP packet */
		p->subscription=ASTOBJ_REF(s);	/* Add pointer to subscription in packet */
		if (!ast_strlen_zero(s->secret))	/* Secret (password) */
			ast_copy_string(p->peersecret, s->secret, sizeof(p->peersecret));
		if (!ast_strlen_zero(s->md5secret))
			ast_copy_string(p->peermd5secret, s->md5secret, sizeof(p->peermd5secret));
		/* User name in this realm  
		- if authuser is set, use that, otherwise use username */
		if (!ast_strlen_zero(s->authuser)) { /* XXX check if we need peer ... */
			ast_copy_string(p->peername, s->authuser, sizeof(p->peername));
			ast_copy_string(p->authname, s->authuser, sizeof(p->authname));
		} else {
			if (!ast_strlen_zero(s->username)) {
				ast_copy_string(p->peername, s->username, sizeof(p->peername));
				ast_copy_string(p->authname, s->username, sizeof(p->authname));
				ast_copy_string(p->fromuser, s->username, sizeof(p->fromuser));
			}
		}
		if (!ast_strlen_zero(s->username))
			ast_copy_string(p->username, s->username, sizeof(p->username));
		/* Save extension in packet */
		ast_copy_string(p->exten, s->contact, sizeof(p->exten));

		/*
		  check which address we should use in our contact header 
		  based on whether the remote host is on the external or
		  internal network so we can subscribe through nat
		 */
		if (ast_sip_ouraddrfor(&p->sa.sin_addr, &p->ourip))
			memcpy(&p->ourip, &bindaddr.sin_addr, sizeof(p->ourip));
		build_contact(p);
	}

	/* set up a timeout */
	if (auth == NULL)  {
		if (s->timeout > -1) {
			ast_log(LOG_WARNING, "Still have a subscription timeout, %d\n", s->timeout);
			ast_sched_del(sched, s->timeout);
		}
		s->timeout = ast_sched_add(sched, default_subscription_expiry * 1000, sip_sub_timeout, s);
		ast_log(LOG_DEBUG, "Scheduled a subscription timeout for %s : %d\n", s->hostname, s->timeout);
	}

	if (strchr(s->username, '@')) {
		snprintf(from, sizeof(from), "<sip:%s>;tag=%s", s->username, p->tag);
		if (!ast_strlen_zero(p->theirtag))
			snprintf(to, sizeof(to), "<sip:%s>;tag=%s", s->username, p->theirtag);
		else
			snprintf(to, sizeof(to), "<sip:%s>", s->username);
	} else {
		snprintf(from, sizeof(from), "<sip:%s@%s>;tag=%s", s->username, p->tohost, p->tag);
		if (!ast_strlen_zero(p->theirtag))
			snprintf(to, sizeof(to), "<sip:%s@%s>;tag=%s", s->username, p->tohost, p->theirtag);
		else
			snprintf(to, sizeof(to), "<sip:%s@%s>", s->username, p->tohost);
	}
	
	/* Fromdomain is what we are subscribing to, regardless of actual
	   host name from SRV */
	if (p->fromdomain && !ast_strlen_zero(p->fromdomain))
		snprintf(addr, sizeof(addr), "sip:%s", p->fromdomain);
	else if (s->subdomain && !ast_strlen_zero(s->subdomain))
		snprintf(addr, sizeof(addr), "sip:%s", s->subdomain);
	else
	{
		if (s->portno != DEFAULT_SIP_PORT)
			snprintf(addr, sizeof(addr), "sip:%s:%d", s->hostname, s->portno);
		else
			snprintf(addr, sizeof(addr), "sip:%s", s->hostname);
	}
	ast_copy_string(p->uri, addr, sizeof(p->uri));

	p->branch ^= rand();

	memset(&req, 0, sizeof(req));
	init_req(&req, sipmethod, addr);

	/* Add to CSEQ */
	snprintf(tmp, sizeof(tmp), "%u %s", ++s->ocseq, sip_methods[sipmethod].text);
	p->ocseq = s->ocseq;

	build_via(p, via, sizeof(via));
	add_header(&req, "Via", via);
	add_header(&req, "From", from);
	add_header(&req, "To", to);
	add_header(&req, "CSeq", tmp);
	add_header(&req, "Call-ID", p->callid);
	add_header(&req, "Contact", p->our_contact);
	add_header(&req, "Allow", ALLOWED_METHODS);
	add_header(&req, "User-Agent", default_useragent);
	add_header(&req, "Max-Forwards", "70");

	
	if (auth) 	/* Add auth header */
		add_header(&req, authheader, auth);
	else if ( !ast_strlen_zero(s->nonce) ) {
		char digest[1024];

		/* We have auth data to reuse, build a digest header! */
		if (sipdebug)
			ast_log(LOG_DEBUG, "   >>> Re-using Auth data for %s@%s\n", s->username, s->hostname);
		ast_copy_string(p->realm, s->realm, sizeof(p->realm));
		ast_copy_string(p->nonce, s->nonce, sizeof(p->nonce));
		ast_copy_string(p->domain, s->domain, sizeof(p->domain));
		ast_copy_string(p->opaque, s->opaque, sizeof(p->opaque));
		ast_copy_string(p->qop, s->qop, sizeof(p->qop));

		memset(digest,0,sizeof(digest));
		build_reply_digest(p, sipmethod, digest, sizeof(digest));
		add_header(&req, "Authorization", digest);
	
	}

	add_header(&req, "Event", "message-summary");
	snprintf(tmp, sizeof(tmp), "%d", default_subscription_expiry);
	add_header(&req, "Expires", tmp);
	add_header(&req, "Accept", "application/simple-message-summary"); 
	add_header(&req, "Content-Length", "0");
	add_blank_header(&req);
	copy_request(&p->initreq, &req);
	parse_request(&p->initreq);
	if (sip_debug_test_pvt(p)) {
		ast_verbose("SUBSCRIBE %d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
	}
	determine_firstline_parts(&p->initreq);
	s->substate=auth?SUB_STATE_AUTHSENT:SUB_STATE_SUBSENT;
	s->subattempts++;	/* Another attempt */
	if (option_debug > 3)
		ast_verbose("SUBSCRIBE attempt %d to %s@%s\n", s->subattempts, s->username, s->hostname);
	return send_request(p, &req, 2, p->ocseq);
}

static void sip_peer_server_down_time_calc(struct sip_registry *r, int reset)
{
	struct sip_peer *peer = NULL;

	if (!(peer = find_peer(r->contact, NULL, 1))) {
		ast_log(LOG_ERROR, "Fail to find peer %s'\n", r->contact);
		return;
	}

	if (peer->last_down_timestamp) 
		peer->total_down_time += ast_tvnow().tv_sec - peer->last_down_timestamp;
	peer->last_down_timestamp = !reset ? ast_tvnow().tv_sec : 0;
	ASTOBJ_UNREF(peer, sip_destroy_peer);
}

/*! \brief  transmit_register: Transmit register to SIP proxy or UA ---*/
static int transmit_register(struct sip_registry *r, int sipmethod, char *auth,
char *authheader, int unregister)
{
	struct sip_request req;
	char from[256];
	char to[256];
	char tmp[80];
	char via[80];
	char addr[80];
	char contact[384];
	struct sip_pvt *p;
	char *destaddr;
	int destport;
	char *todomain;
	int in_srv_failover = 0;

	/* exit if we are already in process with this registrar ?*/
	if ( r == NULL || ((auth==NULL) && (r->regstate==REG_STATE_REGSENT || r->regstate==REG_STATE_AUTHSENT))) {
		ast_log(LOG_NOTICE, "Strange, trying to register %s@%s when registration already pending\n", r->username, r->hostname);
		return 0;
	}

	destaddr = (r->obproxy && *r->obproxy) ? r->obproxy : r->hostname;
	destport = r->obproxyport ? r->obproxyport : r->portno;

	in_srv_failover = r->srv_failover && r->uas_srv.context;
	if (r->call) {	/* We have a registration */
		if (!auth && ast_strlen_zero(r->nextnonce)) {
			ast_log(LOG_WARNING, "Already have a REGISTER going on to %s@%s?? \n", r->username, r->hostname);
			return 0;
		} else {
			p = r->call;
			make_our_tag(p->tag, sizeof(p->tag));	/* create a new local tag for every register attempt */
			p->theirtag[0]='\0';	/* forget their old tag, so we don't match tags when getting response */
		}
	} else {
		/* Build callid for registration if we haven't registered before */
		if (!r->callid_valid) {
			build_callid(r->callid, sizeof(r->callid), __ourip, default_fromdomain);
			r->callid_valid = 1;
		}
		/* Allocate SIP packet for registration */
		p=sip_alloc( r->callid, NULL, 0, SIP_REGISTER);
		if (!p) {
			ast_log(LOG_WARNING, "Unable to allocate registration call\n");
			return 0;
		}
		if (recordhistory) {
			char tmp[80];
			snprintf(tmp, sizeof(tmp), "Account: %s@%s", r->username, r->hostname);
			append_history(p, "RegistryInit", tmp);
		}
		/* Find address to hostname */
		if (r->srv_failover) {
			int ret;
			p->timer_t1 = 500; /* Default SIP retransmission timer T1 (RFC 3261) */
			
			if ((ret = create_uas_addr(p, &r->uas_srv, destaddr))) {
				/* All our srv records are invalid. Do not send any more
				 * retransmissions , Start T1 timer and try again. */
				set_server_na(r, 1);
				if (ret == 1)
					reg_after_srv_not_found(r,p);
				else
					/* we have what we hope is a temporary network error,
					 * probably DNS.  We need to reschedule a retransmission */
					schedule_reg_after_dns_error(r, p);
				return 0;
			}
		}
		else if (create_addr(p, destaddr)) {
			/* we have what we hope is a temporary network error,
			 * probably DNS.  We need to reschedule a registration try */
			set_server_na(r, 1);
			schedule_reg_after_dns_error(r, p);
			sip_peer_server_down_time_calc(r, 0);
			return 0;
		}
		/* Copy back Call-ID in case create_addr changed it */
		ast_copy_string(r->callid, p->callid, sizeof(r->callid));
		if (destport)
			p->sa.sin_port = htons(destport);
		ast_set_flag(p, SIP_OUTGOING);	/* Registration is outgoing call */
		r->call=p;			/* Save pointer to SIP packet */
		p->registry=ASTOBJ_REF(r);	/* Add pointer to registry in packet */
		if (!ast_strlen_zero(r->secret))	/* Secret (password) */
			ast_copy_string(p->peersecret, r->secret, sizeof(p->peersecret));
		if (!ast_strlen_zero(r->md5secret))
			ast_copy_string(p->peermd5secret, r->md5secret, sizeof(p->peermd5secret));
		/* User name in this realm  
		- if authuser is set, use that, otherwise use username */
		if (!ast_strlen_zero(r->authuser)) {	
			ast_copy_string(p->peername, r->authuser, sizeof(p->peername));
			ast_copy_string(p->authname, r->authuser, sizeof(p->authname));
		} else {
			if (!ast_strlen_zero(r->username)) {
				ast_copy_string(p->peername, r->username, sizeof(p->peername));
				ast_copy_string(p->authname, r->username, sizeof(p->authname));
				ast_copy_string(p->fromuser, r->username, sizeof(p->fromuser));
			}
		}
		if (!ast_strlen_zero(r->username))
			ast_copy_string(p->username, r->username, sizeof(p->username));
		/* Save extension in packet */
		ast_copy_string(p->exten, r->contact, sizeof(p->exten));

		/*
		  check which address we should use in our contact header 
		  based on whether the remote host is on the external or
		  internal network so we can register through nat
		 */
		if (ast_sip_ouraddrfor(&p->sa.sin_addr, &p->ourip))
			memcpy(&p->ourip, &bindaddr.sin_addr, sizeof(p->ourip));
		build_contact(p);
	}

	/* set up a timeout */
	if (auth == NULL) {
		if (r->timeout > -1) {
			ast_log(LOG_WARNING, "Still have a registration timeout, #%d - deleting it\n", r->timeout);
			ast_sched_del(sched, r->timeout);
		}
		r->timeout = ast_sched_add(sched, global_reg_timeout * 1000,
		    (r->reg_backup || r->reg_primary) ? proxy_failover_timeout : sip_reg_timeout, r);
		ast_log(LOG_DEBUG, "Scheduled a registration timeout for %s id  #%d \n", r->hostname, r->timeout);
	}

	if (strchr(r->username, '@')) {
		snprintf(from, sizeof(from), "<sip:%s>;tag=%s", r->username, p->tag);
		if (!ast_strlen_zero(p->theirtag))
			snprintf(to, sizeof(to), "<sip:%s>;tag=%s", r->username, p->theirtag);
		else
			snprintf(to, sizeof(to), "<sip:%s>", r->username);
	} else {
		todomain = !ast_strlen_zero(p->todomain) ? p->todomain : p->tohost;
		snprintf(from, sizeof(from), "<sip:%s@%s>;tag=%s", r->username, todomain, p->tag);
		if (!ast_strlen_zero(p->theirtag))
			snprintf(to, sizeof(to), "<sip:%s@%s>;tag=%s", r->username, todomain, p->theirtag);
		else
			snprintf(to, sizeof(to), "<sip:%s@%s>", r->username, todomain);
	}
	
	/* Fromdomain is what we are registering to, regardless of actual
	   host name from SRV */
	if (!ast_strlen_zero(p->fromdomain))
		snprintf(addr, sizeof(addr), "sip:%s", p->fromdomain);
	else if (!ast_strlen_zero(r->regdomain))
		snprintf(addr, sizeof(addr), "sip:%s", r->regdomain);
	else
	{
		if (r->portno != DEFAULT_SIP_PORT)
			snprintf(addr, sizeof(addr), "sip:%s:%d", r->hostname, r->portno);
		else
			snprintf(addr, sizeof(addr), "sip:%s", r->hostname);
	}
	ast_copy_string(p->uri, addr, sizeof(p->uri));

	p->branch ^= thread_safe_rand();

	memset(&req, 0, sizeof(req));
	init_req(&req, sipmethod, addr);

	/* Add to CSEQ */
	snprintf(tmp, sizeof(tmp), "%u %s", ++r->ocseq, sip_methods[sipmethod].text);
	p->ocseq = r->ocseq;

	build_via(p, via, sizeof(via));
	add_header(&req, "Via", via);
	add_header(&req, "From", from);
	add_header(&req, "To", to);
	add_header(&req, "Call-ID", p->callid);
	add_header(&req, "CSeq", tmp);
	add_header(&req, "User-Agent", default_useragent);
	add_header(&req, "Max-Forwards", DEFAULT_MAX_FORWARDS);

	
	if (auth) 	/* Add auth header */
		add_header(&req, authheader, auth);

 
	/* for Vodafone, Authorization is needed only if received 401/407 or we got
	 * nextnonce from last 200OK */
	else if (!ast_strlen_zero(r->nextnonce)) {
		char digest[1024];

		/* We have auth data to reuse, build a digest header! */
		if (sipdebug)
			ast_log(LOG_DEBUG, "   >>> Re-using Auth data for %s@%s\n", r->username, r->hostname);
		ast_copy_string(p->realm, r->realm, sizeof(p->realm));
		ast_copy_string(p->nonce, r->nextnonce, sizeof(p->nonce));
		r->nextnonce[0] = '\0';
		ast_copy_string(p->domain, r->domain, sizeof(p->domain));
		ast_copy_string(p->opaque, r->opaque, sizeof(p->opaque));
		ast_copy_string(p->qop, r->qop, sizeof(p->qop));
		p->noncecount = r->noncecount++;

		memset(digest,0,sizeof(digest));
		if(!build_reply_digest(p, sipmethod, digest, sizeof(digest)))
			add_header(&req, "Authorization", digest);
		else
			ast_log(LOG_NOTICE, "No authorization available for authentication of registration to %s@%s\n", r->username, r->hostname);
	
	}

	r->fetch_state = r->ip_changed ? r->fetch_state : FETCH_BINDING_STATE_UNBOUND;

	switch (r->fetch_state)
	{
	case FETCH_BINDING_STATE_UNBOUND:
		if (unregister)
			r->expiry = 0;
		snprintf(tmp, sizeof(tmp), "%d", r->expiry);
		add_header(&req, "Expires", tmp);
		snprintf(contact, sizeof(contact), "%s%s", p->our_contact, 
			ast_test_flag(&global_flags_page2, SIP_PAGE2_FEATURE_3GPP_SMS) ? 
			";+g.3gpp.smsip" : "");
		add_header(&req, "Contact", contact);
		add_header(&req, "Event", "registration");
		add_accept_header(&req);
		break;
	case FETCH_BINDING_STATE_QUERY:
		break;
	case FETCH_BINDING_STATE_UNREGISTER:
		{
			contact_list_t *c;

			r->expiry = 0;
			snprintf(tmp, sizeof(tmp), "%d", r->expiry);
			add_header(&req, "Expires", tmp);

			for (c = r->contact_list; c; c = c->next)
			{
				char *name = p->clir ? NETWORK_ANONYMOUS_USERNAME : p->exten;

				snprintf(contact, sizeof(contact), "<sip:%s%s%s:%d>%s",
					name, ast_strlen_zero(name) ? "" : "@", c->host, c->port,  
					ast_test_flag(&global_flags_page2, SIP_PAGE2_FEATURE_3GPP_SMS) ? 
					";+g.3gpp.smsip" : "");
				add_header(&req, "Contact", contact);
			}

			add_header(&req, "Event", "registration");
			add_accept_header(&req);
			break;
		}
	}

	add_header_contentLength(&req, 0);
	add_blank_header(&req);
	copy_request(&p->initreq, &req);
	parse_request(&p->initreq);
	if (sip_debug_test_pvt(p)) {
		ast_verbose("REGISTER %d headers, %d lines\n", p->initreq.headers, p->initreq.lines);
	}
	determine_firstline_parts(&p->initreq);
	r->regstate=auth?REG_STATE_AUTHSENT:REG_STATE_REGSENT;
	manager_event_sip_registry();
	if (!in_srv_failover && r->fetch_state == FETCH_BINDING_STATE_UNBOUND)
		r->regattempts++;	/* Another attempt */
	if (option_debug > 3)
		ast_verbose("REGISTER attempt %d to %s@%s, fetch bindings %d\n", r->regattempts, r->username, r->hostname, r->fetch_state);
	return send_request(p, &req, 2, p->ocseq);
}

/* Transmit SIP MESSAGE, with arbitrary content, either within or out of call */
static int transmit_message_with_content(struct sip_pvt *p, const char *type, const char *buf, int len, int in_dialog)
{
	struct sip_request req;

	if (in_dialog)
		reqprep(&req, p, SIP_MESSAGE, 0, 1);
	else
		initreqprep(&req, p, SIP_MESSAGE);

	add_content(&req, type, buf, len);
	return transmit_sip_request(p, &req, 1);
}

/*! \brief  transmit_message_with_text: Transmit text with SIP MESSAGE method ---*/
static int transmit_message_with_text(struct sip_pvt *p, const char *text)
{
	return transmit_message_with_content(p, "text/plain", text, strlen(text), 1);
}

/*--- sip_escape_uri: Turn an ascii char into %XX---*/
static int sip_escape_uri(char *uri, char *buf, size_t len)
{
    	char *p;
    	int x = 0;
    	const char urlunsafe[] = " \"#%&+:;<=>?@[\\]^`{|}";
    	const char hex[] = "0123456789ABCDEF";

    	memset(buf, 0, len);
    	for (p = uri; *p; p++) {
		if (*p < ' ' || *p > '~' || strchr(urlunsafe, *p)) {
	    		if (x + 3 > len)
				break;

			buf[x++] = '%';
            		buf[x++] = hex[*p >> 4];
            		buf[x++] = hex[*p & 0x0f];
		} else {
	    		if (x + 1 > len)
				break;

            		buf[x++] = *p;
		}
    	}
    
    	return x;
}

static int hex2int(char a)
{
	if ((a >= '0') && (a <= '9')) {
		return a - '0';
	} else if ((a >= 'a') && (a <= 'f')) {
		return a - 'a' + 10;
	} else if ((a >= 'A') && (a <= 'F')) {
		return a - 'A' + 10;
	}
	return 0;
}

/*--- sip_unescape_uri: Turn %XX into and ascii char ---*/
static int sip_unescape_uri(char *uri) 
{
	char *ptr = uri;
	int replaced = 0;

	while ((ptr = strchr(ptr, '%'))) {
		/* un-escape urlencoded text */
		if (strlen(ptr) < 3)
			break;
		*ptr = hex2int(ptr[1]) * 16 + hex2int(ptr[2]);
		memmove(ptr+1, ptr+3, strlen(ptr+3) + 1);
		ptr++;
		replaced++;
	}

	return replaced;
}

/*! \brief  transmit_refer: Transmit SIP REFER message ---*/
static int transmit_refer(struct sip_pvt *p, struct sip_pvt *target_pvt, const char *dest)
{
	struct sip_request req;
	char from[256];
	char *of, *c;
	char referto[256];

	if (ast_test_flag(p, SIP_OUTGOING)) 
		of = get_header(&p->initreq, "To");
	else
		of = get_header(&p->initreq, "From");
	ast_copy_string(from, of, sizeof(from));
	of = get_in_brackets(from);
	ast_copy_string(p->from,of,sizeof(p->from));
	if (strncmp(of, "sip:", 4)) {
		ast_log(LOG_NOTICE, "From address missing 'sip:', using it anyway\n");
	} else
		of += 4;
	/* Get just the username part */
	if ((c = strchr(dest, '@'))) {
		c = NULL;
	} else if ((c = strchr(of, '@'))) {
		*c = '\0';
		c++;
	}
	if (c) {
	    if (target_pvt)
	    {
			char callid[256];
			char *destination = get_header(&target_pvt->initreq, "To");
			sip_escape_uri(get_header(&target_pvt->initreq, "Call-ID"), callid, sizeof(callid));
			destination = get_in_brackets(destination);
			snprintf(referto, sizeof(referto),
			    "<%s?Replaces=%s%%3Bfrom-tag%%3D%s%%3Bto-tag%%3D%s>",
			    destination, callid, target_pvt->tag, target_pvt->theirtag);
	    }
	    else {
			struct sip_peer *peer = find_peer(p->username, NULL, 1);

			/* Although the RFC allows for different Refer-To formats, some
			 * service providers (e.g. BroadVoice) will accept only username@proxy */
			snprintf(referto, sizeof(referto), "<sip:%s@%s>", dest, peer ?
				peer->tohost : c);
			if (peer)
				ASTOBJ_UNREF(peer, sip_destroy_peer);
		}
	} else {
		snprintf(referto, sizeof(referto), "<sip:%s>", dest);
	}

	/* save in case we get 407 challenge */
	ast_copy_string(p->refer_to, referto, sizeof(p->refer_to));
	ast_copy_string(p->referred_by, p->our_contact, sizeof(p->referred_by));

	reqprep(&req, p, SIP_REFER, 0, 1);
	add_header(&req, "Refer-To", referto);
	add_supported_header(p, &req);
	if (!ast_strlen_zero(p->our_contact))
		add_header(&req, "Referred-By", p->our_contact);
	add_blank_header(&req);
	return send_request(p, &req, 1, p->ocseq);
}

/*! \brief  transmit_info_with_digit: Send SIP INFO dtmf message, see Cisco documentation on cisco.co
m ---*/
static int transmit_info_with_digit(struct sip_pvt *p, char digit)
{
	struct sip_request req;
	reqprep(&req, p, SIP_INFO, 0, 1);
	add_digit(&req, digit);
	return send_request(p, &req, 1, p->ocseq);
}

/*! \brief  transmit_info_with_broadsoft_flash: Send SIP INFO flash message in Broadsoft compatibility mode ---*/
static int transmit_info_with_broadsoft_flash(struct sip_pvt *p)
{
	struct sip_request req;
	char tmp[256];

	reqprep(&req, p, SIP_INFO, 0, 1);
	snprintf(tmp, sizeof(tmp), "event flashhook\r\n");
	add_header(&req, "Content-Type", "application/broadsoft; version=1.0");
	add_header_contentLength(&req, strlen(tmp));
	add_line(&req, tmp);

	return send_request(p, &req, 1, p->ocseq);
}

/*! \brief  transmit_info_with_vidupdate: Send SIP INFO with video update request ---*/
static int transmit_info_with_vidupdate(struct sip_pvt *p)
{
	struct sip_request req;
	reqprep(&req, p, SIP_INFO, 0, 1);
	add_vidupdate(&req);
	return send_request(p, &req, 1, p->ocseq);
}

/*! \brief  transmit_request: transmit generic SIP request ---*/
static int transmit_request(struct sip_pvt *p, int sipmethod, int seqno, int reliable, int newbranch)
{
	struct sip_request resp;
	reqprep(&resp, p, sipmethod, seqno, newbranch);
	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_request(p, &resp, reliable, seqno ? seqno : p->ocseq);
}

/*! \brief  transmit_request_with_auth: Transmit SIP request, auth added ---*/
static int transmit_request_with_auth(struct sip_pvt *p, int sipmethod, int seqno, int reliable, int newbranch)
{
	struct sip_request resp;

	reqprep(&resp, p, sipmethod, seqno, newbranch);
	if (*p->realm) {
		char digest[1024];

		memset(digest, 0, sizeof(digest));
		if(!build_reply_digest(p, sipmethod, digest, sizeof(digest))) {
			if (p->options && p->options->auth_type == PROXY_AUTH)
				add_header(&resp, "Proxy-Authorization", digest);
			else if (p->options && p->options->auth_type == WWW_AUTH)
				add_header(&resp, "Authorization", digest);
			else	/* Default, to be backwards compatible (maybe being too careful, but leaving it for now) */
				add_header(&resp, "Proxy-Authorization", digest);
		} else
			ast_log(LOG_WARNING, "No authentication available for call %s\n", p->callid);
	}

	add_header_contentLength(&resp, 0);
	add_blank_header(&resp);
	return send_request(p, &resp, reliable, seqno ? seqno : p->ocseq);	
}

static void destroy_association(struct sip_peer *peer)
{
	if (!ast_test_flag((&global_flags_page2), SIP_PAGE2_IGNOREREGEXPIRE)) {
		if (ast_test_flag(&(peer->flags_page2), SIP_PAGE2_RT_FROMCONTACT)) {
			ast_update_realtime("sippeers", "name", peer->name, "fullcontact", "", "ipaddr", "", "port", "", "regseconds", "0", "username", "", NULL);
		} else {
			ast_db_del("SIP/Registry", peer->name);
		}
	}
}

/*! \brief  expire_register: Expire registration of SIP peer ---*/
static int expire_register(void *data)
{
	struct sip_peer *peer = data;

	memset(&peer->addr, 0, sizeof(peer->addr));

	destroy_association(peer);
	
	manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Unregistered\r\nCause: Expired\r\n", peer->name);
	register_peer_exten(peer, 0);
	peer->expire = -1;
	ast_device_state_changed("SIP/%s", peer->name);
	if (ast_test_flag(peer, SIP_SELFDESTRUCT) || ast_test_flag((&peer->flags_page2), SIP_PAGE2_RTAUTOCLEAR)) {
		ASTOBJ_CONTAINER_UNLINK(&peerl, peer);
		ASTOBJ_UNREF(peer, sip_destroy_peer);
	}

	return 0;
}

static int sip_poke_peer(struct sip_peer *peer);

static int sip_poke_peer_s(void *data)
{
	struct sip_peer *peer = data;
	peer->pokeexpire = -1;
	sip_poke_peer(peer);
	return 0;
}

/*! \brief  reg_source_db: Get registration details from Asterisk DB ---*/
static void reg_source_db(struct sip_peer *peer)
{
	char data[256];
	char iabuf[INET_ADDRSTRLEN];
	struct in_addr in;
	int expiry;
	int port;
	char *scan, *addr, *port_str, *expiry_str, *username, *contact;

	if (ast_test_flag(&(peer->flags_page2), SIP_PAGE2_RT_FROMCONTACT)) 
		return;
	if (ast_db_get("SIP/Registry", peer->name, data, sizeof(data)))
		return;

	scan = data;
	addr = strsep(&scan, ":");
	port_str = strsep(&scan, ":");
	expiry_str = strsep(&scan, ":");
	username = strsep(&scan, ":");
	contact = scan;	/* Contact include sip: and has to be the last part of the database entry as long as we use : as a separator */

	if (!inet_aton(addr, &in))
		return;

	if (port_str)
		port = atoi(port_str);
	else
		return;

	if (expiry_str)
		expiry = atoi(expiry_str);
	else
		return;

	if (username)
		ast_copy_string(peer->username, username, sizeof(peer->username));
	if (contact)
		ast_copy_string(peer->fullcontact, contact, sizeof(peer->fullcontact));

	if (option_verbose > 2)
		ast_verbose(VERBOSE_PREFIX_3 "SIP Seeding peer from astdb: '%s' at %s@%s:%d for %d\n",
			    peer->name, peer->username, ast_inet_ntoa(iabuf, sizeof(iabuf), in), port, expiry);

	memset(&peer->addr, 0, sizeof(peer->addr));
	peer->addr.sin_family = AF_INET;
	peer->addr.sin_addr = in;
	peer->addr.sin_port = htons(port);
	if (sipsock < 0) {
		/* SIP isn't up yet, so schedule a poke only, pretty soon */
		if (peer->pokeexpire > -1)
			ast_sched_del(sched, peer->pokeexpire);
		peer->pokeexpire = ast_sched_add(sched, thread_safe_rand() % 5000 + 1, sip_poke_peer_s, peer);
	} else
		sip_poke_peer(peer);
	if (peer->expire > -1)
		ast_sched_del(sched, peer->expire);
	peer->expire = ast_sched_add(sched, (expiry + 10) * 1000, expire_register, peer);
	register_peer_exten(peer, 1);
}

static int parse_ok_authinfo (struct sip_registry *reg, struct sip_request *req)
{
	char *authinfo = get_header(req, "Authentication-Info");
	char tmp[265];
	char *c;
	char *nextnonce = 0;
	
	if (ast_strlen_zero(authinfo))
		return -1; /* header not present */

	ast_log(LOG_DEBUG, "Authentication-Info header found: %s\n", authinfo); 
	ast_copy_string(tmp, authinfo, sizeof(tmp));
	c = tmp;

	while(c)
	{
		c = ast_skip_blanks(c);
		if (!*c)
			break;
		if (!strncasecmp(c, "nextnonce=", strlen("nextnonce=")))
		{
			ast_log(LOG_DEBUG, "Authentication-Info header parsing: nextnonce field found\n"); 
			c+= strlen("nextnonce=");
			if ((*c == '\"'))
			{
				nextnonce = ++c;
				if ((c = strchr(c,'\"')))
					*c = '\0';
			}
			else
			{
				nextnonce = c;
				if ((c = strchr(c,',')))
					*c = '\0';
			}

		} 
		if (c)
			c++;
	}

	if (!nextnonce)
	{
		ast_log(LOG_DEBUG, "nextnonce value not found \n");
		return -1;
	}
	ast_copy_string(reg->nextnonce, nextnonce, sizeof(reg->nextnonce));
	ast_log(LOG_DEBUG, "nextnonce value extracted: %s \n", reg->nextnonce); 
	return 0;
}

/*! \brief  parse_ok_contact: Parse contact header for 200 OK on INVITE ---*/
static int parse_ok_contact(struct sip_pvt *pvt, struct sip_request *req)
{
	char contact[250]; 
	char *c, *n, *pt;
	int port;
	struct sockaddr_in sin;
	struct sockaddr_in oldsin;

	/* Look for brackets */
	ast_copy_string(contact, get_header(req, "Contact"), sizeof(contact));
	c = get_in_brackets(contact);

	/* Save full contact to call pvt for later bye or re-invite */
	ast_copy_string(pvt->fullcontact, c, sizeof(pvt->fullcontact));	

	/* Save URI for later ACKs, BYE or RE-invites */
	ast_copy_string(pvt->okcontacturi, c, sizeof(pvt->okcontacturi));
	
	/* Make sure it's a SIP URL */
	if (strncasecmp(c, "sip:", 4)) {
		ast_log(LOG_NOTICE, "'%s' is not a valid SIP contact (missing sip:) trying to use anyway\n", c);
	} else
		c += 4;

	/* Ditch arguments */
	n = strchr(c, ';');
	if (n) 
		*n = '\0';

	/* Grab host */
	n = strchr(c, '@');
	if (!n) {
		n = c;
		c = NULL;
	} else {
		*n = '\0';
		n++;
	}
	pt = strchr(n, ':');
	if (pt) {
		*pt = '\0';
		pt++;
		port = atoi(pt);
	} else
		port = DEFAULT_SIP_PORT;

	memcpy(&oldsin, &pvt->sa, sizeof(oldsin));

	if (!(ast_test_flag(pvt, SIP_NAT) & SIP_NAT_ROUTE)) {
		/* XXX This could block for a long time XXX */
		/* We should only do this if it's a name, not an IP */

		if (ast_get_ip_or_srv(&sin, n, srvlookup ? "_sip._udp" : NULL)) {
			ast_log(LOG_WARNING, "Invalid host '%s'\n", n);
			return -1;
		}
		pvt->sa.sin_family = AF_INET;
		memcpy(&pvt->sa.sin_addr, &sin.sin_addr, sizeof(pvt->sa.sin_addr));
		pvt->sa.sin_port = htons(port);
	} else {
		/* Don't trust the contact field.  Just use what they came to us
		   with. */
		memcpy(&pvt->sa, &pvt->recv, sizeof(pvt->sa));
	}
	return 0;
}


enum parse_register_result {
	PARSE_REGISTER_FAILED,
	PARSE_REGISTER_UPDATE,
	PARSE_REGISTER_QUERY,
};

/*! \brief  parse_register_contact: Parse contact header and save registration ---*/
static enum parse_register_result parse_register_contact(struct sip_pvt *pvt, struct sip_peer *p, struct sip_request *req)
{
	char contact[80]; 
	char data[256];
	char iabuf[INET_ADDRSTRLEN];
	char *expires = get_header(req, "Expires");
	int expiry = atoi(expires);
	char *c, *n, *pt;
	int port;
	char *useragent;
	struct hostent *hp;
	struct ast_hostent ahp;
	struct sockaddr_in oldsin;

	if (ast_strlen_zero(expires)) {	/* No expires header */
		expires = strcasestr(get_header(req, "Contact"), ";expires=");
		if (expires) {
			char *ptr;
			if ((ptr = strchr(expires, ';')))
				*ptr = '\0';
			if (sscanf(expires + 9, "%d", &expiry) != 1)
				expiry = default_expiry_server;
		} else {
			/* Nothing has been specified */
			expiry = default_expiry_server;
		}
	}
	/* Look for brackets */
	ast_copy_string(contact, get_header(req, "Contact"), sizeof(contact));
	if (strchr(contact, '<') == NULL) {	/* No <, check for ; and strip it */
		char *ptr = strchr(contact, ';');	/* This is Header options, not URI options */
		if (ptr)
			*ptr = '\0';
	}
	c = get_in_brackets(contact);

	/* if they did not specify Contact: or Expires:, they are querying
	   what we currently have stored as their contact address, so return
	   it
	*/
	if (ast_strlen_zero(c) && ast_strlen_zero(expires)) {
		/* If we have an active registration, tell them when the registration is going to expire */
		if ((p->expire > -1) && !ast_strlen_zero(p->fullcontact)) {
			pvt->expiry = ast_sched_when(sched, p->expire);
		} 
		return PARSE_REGISTER_QUERY;
	} else if (!strcasecmp(c, "*") || !expiry) {	/* Unregister this peer */
		/* This means remove all registrations and return OK */
		memset(&p->addr, 0, sizeof(p->addr));
		if (p->expire > -1)
			ast_sched_del(sched, p->expire);
		p->expire = -1;

		destroy_association(p);
		
		register_peer_exten(p, 0);
		p->fullcontact[0] = '\0';
		p->useragent[0] = '\0';
		p->sipoptions = 0;
		p->lastms = 0;

		if (option_verbose > 2)
			ast_verbose(VERBOSE_PREFIX_3 "Unregistered SIP '%s'\n", p->name);
			manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Unregistered\r\n", p->name);

		expire_register(p);

		return PARSE_REGISTER_UPDATE;
	}
	ast_copy_string(p->fullcontact, c, sizeof(p->fullcontact));
	/* For the 200 OK, we should use the received contact */
	snprintf(pvt->our_contact, sizeof(pvt->our_contact) - 1, "<%s>", c);
	/* Make sure it's a SIP URL */
	if (strncasecmp(c, "sip:", 4)) {
		ast_log(LOG_NOTICE, "'%s' is not a valid SIP contact (missing sip:) trying to use anyway\n", c);
	} else
		c += 4;
	/* Ditch q */
	n = strchr(c, ';');
	if (n) {
		*n = '\0';
	}
	/* Grab host */
	n = strchr(c, '@');
	if (!n) {
		n = c;
		c = NULL;
	} else {
		*n = '\0';
		n++;
	}
	pt = strchr(n, ':');
	if (pt) {
		*pt = '\0';
		pt++;
		port = atoi(pt);
	} else
		port = DEFAULT_SIP_PORT;
	memcpy(&oldsin, &p->addr, sizeof(oldsin));
	if (!(ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE)) {
		/* XXX This could block for a long time XXX */
		hp = ast_gethostbyname(n, &ahp);
		if (!hp)  {
			ast_log(LOG_WARNING, "Invalid host '%s'\n", n);
			return PARSE_REGISTER_FAILED;
		}
		p->addr.sin_family = AF_INET;
		memcpy(&p->addr.sin_addr, hp->h_addr, sizeof(p->addr.sin_addr));
		p->addr.sin_port = htons(port);
	} else {
		/* Don't trust the contact field.  Just use what they came to us
		   with */
		memcpy(&p->addr, &pvt->recv, sizeof(p->addr));
	}

	if (p->expire > -1)
		ast_sched_del(sched, p->expire);
	if ((expiry < 1 || expiry > default_expiry_server) && !ast_test_flag(&(p->flags_page2), SIP_PAGE2_FROM_TEMPLATE))
		expiry = default_expiry_server;
	if (!ast_test_flag(p, SIP_REALTIME))
		p->expire = ast_sched_add(sched, (expiry + 10) * 1000, expire_register, p);
	else
		p->expire = -1;
	pvt->expiry = expiry;
	snprintf(data, sizeof(data), "%s:%d:%d:%s:%s", ast_inet_ntoa(iabuf, sizeof(iabuf), p->addr.sin_addr), ntohs(p->addr.sin_port), expiry, p->username, p->fullcontact);
	if (!ast_test_flag((&p->flags_page2), SIP_PAGE2_RT_FROMCONTACT)) 
		ast_db_put("SIP/Registry", p->name, data);
	manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Registered\r\n", p->name);
	if (inaddrcmp(&p->addr, &oldsin)) {
		sip_poke_peer(p);
		if (option_verbose > 2)
			ast_verbose(VERBOSE_PREFIX_3 "Registered SIP '%s' at %s port %d expires %d\n", p->name, ast_inet_ntoa(iabuf, sizeof(iabuf), p->addr.sin_addr), ntohs(p->addr.sin_port), expiry);
		register_peer_exten(p, 1);
	}
	
	/* Save SIP options profile */
	p->sipoptions = pvt->sipoptions;

	/* Save User agent */
	useragent = get_header(req, "User-Agent");
	if (useragent && strcasecmp(useragent, p->useragent)) {
		ast_copy_string(p->useragent, useragent, sizeof(p->useragent));
		if (option_verbose > 3) {
			ast_verbose(VERBOSE_PREFIX_3 "Saved useragent \"%s\" for peer %s\n",p->useragent,p->name);  
		}
	}
	return PARSE_REGISTER_UPDATE;
}

/*! \brief  free_old_route: Remove route from route list ---*/
static void free_old_route(struct sip_route *route)
{
	struct sip_route *next;
	while (route) {
		next = route->next;
		free(route);
		route = next;
	}
}

/*! \brief  list_route: List all routes - mostly for debugging ---*/
static void list_route(struct sip_route *route)
{
	if (!route) {
		ast_verbose("list_route: no route\n");
		return;
	}
	while (route) {
		ast_verbose("list_route: hop: <%s>\n", route->hop);
		route = route->next;
	}
}

/*! \brief  build_route: Build route list from Record-Route header ---*/
static void build_route(struct sip_pvt *p, struct sip_request *req, int backwards)
{
	struct sip_route *thishop, *head, *tail;
	int start = 0;
	int len;
	char *rr, *contact, *c;

	/* Once a persistant route is set, don't fool with it */
	if (p->route && p->route_persistant) {
		ast_log(LOG_DEBUG, "build_route: Retaining previous route: <%s>\n", p->route->hop);
		return;
	}

	p->route_persistant = backwards;
	
	/* We build up head, then assign it to p->route when we're done */
	head = NULL;  tail = head;
	/* 1st we pass through all the hops in any Record-Route headers */
	for (;;) {
		/* Each Record-Route header */
		rr = __get_header(req, "Record-Route", &start);
		if (*rr == '\0') break;
		for (;;) {
			/* Each route entry */
			/* Find < */
			rr = strchr(rr, '<');
			if (!rr) break; /* No more hops */
			++rr;
			len = strcspn(rr, ">") + 1;
			/* Make a struct route */
			thishop = malloc(sizeof(*thishop) + len);
			if (thishop) {
				ast_copy_string(thishop->hop, rr, len);
				ast_log(LOG_DEBUG, "build_route: Record-Route hop: <%s>\n", thishop->hop);
				/* Link in */
				if (backwards) {
					/* Link in at head so they end up in reverse order */
					thishop->next = head;
					head = thishop;
					/* If this was the first then it'll be the tail */
					if (!tail) tail = thishop;
				} else {
					thishop->next = NULL;
					/* Link in at the end */
					if (tail)
						tail->next = thishop;
					else
						head = thishop;
					tail = thishop;
				}
			}
			rr += len;
		}
	}

	/* XXX Workaround for B37713. Keep the previous route if there was no
	 * Record-Route field in req */
	if (p->route) {
		if (!head) {
			head = p->route;
			for (tail = head; tail && tail->next; tail = tail->next);
		} else {
			free_old_route(p->route);
			p->route = NULL;
		}
	}

	/* Only append the contact if we are dealing with a strict router */
	if (!head || (!ast_strlen_zero(head->hop) && strstr(head->hop,";lr") == NULL) ) {
		/* 2nd append the Contact: if there is one */
		/* Can be multiple Contact headers, comma separated values - we just take the first */
		contact = get_header(req, "Contact");
		if (!ast_strlen_zero(contact)) {
			ast_log(LOG_DEBUG, "build_route: Contact hop: %s\n", contact);
			/* Look for <: delimited address */
			c = strchr(contact, '<');
			if (c) {
				/* Take to > */
				++c;
				len = strcspn(c, ">") + 1;
			} else {
				/* No <> - just take the lot */
				c = contact;
				len = strlen(contact) + 1;
			}
			thishop = malloc(sizeof(*thishop) + len);
			if (thishop) {
				ast_copy_string(thishop->hop, c, len);
				thishop->next = NULL;
				/* Goes at the end */
				if (tail)
					tail->next = thishop;
				else
					head = thishop;
			}
		}
	}

	/* Store as new route */
	p->route = head;

	/* For debugging dump what we ended up with */
	if (sip_debug_test_pvt(p))
		list_route(p->route);
}

#ifdef OSP_SUPPORT
/*! \brief  check_osptoken: Validate OSP token for user authrroization ---*/
static int check_osptoken (struct sip_pvt *p, char *token)
{
	char tmp[80];

	if (ast_osp_validate (NULL, token, &p->osphandle, &p->osptimelimit, p->cid_num, p->sa.sin_addr, p->exten) < 1) {
		return (-1);
	} else {
		snprintf (tmp, sizeof (tmp), "%d", p->osphandle);
		pbx_builtin_setvar_helper (p->owner, "_OSPHANDLE", tmp);
		return (0);
	}
}
#endif

/*! \brief  check_auth: Check user authorization from peer definition ---*/
/*      Some actions, like REGISTER and INVITEs from peers require
        authentication (if peer have secret set) */
static int check_auth(struct sip_pvt *p, struct sip_request *req, char *randdata, int randlen, char *username, char *secret, char *md5secret, int sipmethod, char *uri, int reliable, int ignore)
{
	int res = -1;
	char *response = "407 Proxy Authentication Required";
	char *reqheader = "Proxy-Authorization";
	char *respheader = "Proxy-Authenticate";
	char *authtoken;
#ifdef OSP_SUPPORT
	char *osptoken;
#endif
	/* Always OK if no secret */
	if (ast_strlen_zero(secret) && ast_strlen_zero(md5secret)
#ifdef OSP_SUPPORT
	    && !ast_test_flag(p, SIP_OSPAUTH)
	    && global_allowguest != 2
#endif
		)
		return 0;
	if (sipmethod == SIP_REGISTER || sipmethod == SIP_SUBSCRIBE) {
		/* On a REGISTER, we have to use 401 and its family of headers instead of 407 and its family
		   of headers -- GO SIP!  Whoo hoo!  Two things that do the same thing but are used in
		   different circumstances! What a surprise. */
		response = "401 Unauthorized";
		reqheader = "Authorization";
		respheader = "WWW-Authenticate";
	}
#ifdef OSP_SUPPORT
	else {
		ast_log (LOG_DEBUG, "Checking OSP Authentication!\n");
		osptoken = get_header (req, "P-OSP-Auth-Token");
		switch (ast_test_flag (p, SIP_OSPAUTH)) {
			case SIP_OSPAUTH_NO:
				break;
			case SIP_OSPAUTH_GATEWAY:
				if (ast_strlen_zero (osptoken)) {
					if (ast_strlen_zero (secret) && ast_strlen_zero (md5secret)) {
						return (0);
					}
				}
				else {
					return (check_osptoken (p, osptoken));
				}
				break;
			case SIP_OSPAUTH_PROXY:
				if (ast_strlen_zero (osptoken)) {
					return (0);
				} 
				else {
					return (check_osptoken (p, osptoken));
				}
				break;
			case SIP_OSPAUTH_EXCLUSIVE:
				if (ast_strlen_zero (osptoken)) {
					return (-1);
				}
				else {
					return (check_osptoken (p, osptoken));
				}
				break;
			default:
				return (-1);
		}
 	}
#endif	
	authtoken =  get_header(req, reqheader);	
	if (ignore && !ast_strlen_zero(randdata) && ast_strlen_zero(authtoken)) {
		/* This is a retransmitted invite/register/etc, don't reconstruct authentication
		   information */
		if (!ast_strlen_zero(randdata)) {
			if (!reliable) {
				/* Resend message if this was NOT a reliable delivery.   Otherwise the
				   retransmission should get it */
				transmit_response_with_auth(p, response, req, randdata, reliable, respheader, 0);
				/* Schedule auto destroy in 15 seconds */
				sip_scheddestroy(p, 15000);
			}
			res = 1;
		}
	} else if (ast_strlen_zero(randdata) || ast_strlen_zero(authtoken)) {
		snprintf(randdata, randlen, "%08x", thread_safe_rand());
		transmit_response_with_auth(p, response, req, randdata, reliable, respheader, 0);
		/* Schedule auto destroy in 15 seconds */
		sip_scheddestroy(p, 15000);
		res = 1;
	} else {
		/* Whoever came up with the authentication section of SIP can suck my %&#$&* for not putting
		   an example in the spec of just what it is you're doing a hash on. */
		char a1[256];
		char a2[256];
		char a1_hash[256];
		char a2_hash[256];
		char resp[256];
		char resp_hash[256]="";
		char tmp[256];
		char *c;
		char *z;
		char *ua_hash ="";
		char *resp_uri ="";
		char *nonce = "";
		char *digestusername = "";
		int  wrongnonce = 0;
		char *usednonce = randdata;

		/* Find their response among the mess that we'r sent for comparison */
		ast_copy_string(tmp, authtoken, sizeof(tmp));
		c = tmp;

		while(c) {
			c = ast_skip_blanks(c);
			if (!*c)
				break;
			if (!strncasecmp(c, "response=", strlen("response="))) {
				c+= strlen("response=");
				if ((*c == '\"')) {
					ua_hash=++c;
					if ((c = strchr(c,'\"')))
						*c = '\0';

				} else {
					ua_hash=c;
					if ((c = strchr(c,',')))
						*c = '\0';
				}

			} else if (!strncasecmp(c, "uri=", strlen("uri="))) {
				c+= strlen("uri=");
				if ((*c == '\"')) {
					resp_uri=++c;
					if ((c = strchr(c,'\"')))
						*c = '\0';
				} else {
					resp_uri=c;
					if ((c = strchr(c,',')))
						*c = '\0';
				}

			} else if (!strncasecmp(c, "username=", strlen("username="))) {
				c+= strlen("username=");
				if ((*c == '\"')) {
					digestusername=++c;
					if((c = strchr(c,'\"')))
						*c = '\0';
				} else {
					digestusername=c;
					if((c = strchr(c,',')))
						*c = '\0';
				}
			} else if (!strncasecmp(c, "nonce=", strlen("nonce="))) {
				c+= strlen("nonce=");
				if ((*c == '\"')) {
					nonce=++c;
					if ((c = strchr(c,'\"')))
						*c = '\0';
				} else {
					nonce=c;
					if ((c = strchr(c,',')))
						*c = '\0';
				}

			} else
				if ((z = strchr(c,' ')) || (z = strchr(c,','))) c=z;
			if (c)
				c++;
		}
		/* Verify that digest username matches  the username we auth as */
		if (strcmp(username, digestusername)) {
			/* Oops, we're trying something here */
			return -2;
		}

		/* Verify nonce from request matches our nonce.  If not, send 401 with new nonce */
		if (strncasecmp(randdata, nonce, randlen)) {
			wrongnonce = 1;
			usednonce = nonce;
		}

		snprintf(a1, sizeof(a1), "%s:%s:%s", username, global_realm, secret);

		if (!ast_strlen_zero(resp_uri))
			snprintf(a2, sizeof(a2), "%s:%s", sip_methods[sipmethod].text, resp_uri);
		else
			snprintf(a2, sizeof(a2), "%s:%s", sip_methods[sipmethod].text, uri);

		if (!ast_strlen_zero(md5secret))
			snprintf(a1_hash, sizeof(a1_hash), "%s", md5secret);
		else
			ast_md5_hash(a1_hash, a1);

		ast_md5_hash(a2_hash, a2);

		snprintf(resp, sizeof(resp), "%s:%s:%s", a1_hash, usednonce, a2_hash);
		ast_md5_hash(resp_hash, resp);

		if (wrongnonce) {

			snprintf(randdata, randlen, "%08x", thread_safe_rand());
			if (ua_hash && !strncasecmp(ua_hash, resp_hash, strlen(resp_hash))) {
				if (sipdebug)
					ast_log(LOG_NOTICE, "stale nonce received from '%s'\n", get_header(req, "To"));
				/* We got working auth token, based on stale nonce . */
				transmit_response_with_auth(p, response, req, randdata, reliable, respheader, 1);
			} else {
				/* Everything was wrong, so give the device one more try with a new challenge */
				if (sipdebug)
					ast_log(LOG_NOTICE, "Bad authentication received from '%s'\n", get_header(req, "To"));
				transmit_response_with_auth(p, response, req, randdata, reliable, respheader, 0);
			}

			/* Schedule auto destroy in 15 seconds */
			sip_scheddestroy(p, 15000);
			return 1;
		} 
		/* resp_hash now has the expected response, compare the two */
		if (ua_hash && !strncasecmp(ua_hash, resp_hash, strlen(resp_hash))) {
			/* Auth is OK */
			res = 0;
		}
	}
	/* Failure */
	return res;
}

/*! \brief  cb_extensionstate: Callback for the devicestate notification (SUBSCRIBE) support subsystem ---*/
/*    If you add an "hint" priority to the extension in the dial plan,
      you will get notifications on device state changes */
static int cb_extensionstate(char *context, char* exten, int state, void *data)
{
	struct sip_pvt *p = data;

	switch(state) {
	case AST_EXTENSION_DEACTIVATED:	/* Retry after a while */
	case AST_EXTENSION_REMOVED:	/* Extension is gone */
		if (p->autokillid > -1)
			sip_cancel_destroy(p);	/* Remove subscription expiry for renewals */
		sip_scheddestroy(p, 15000);	/* Delete subscription in 15 secs */
		ast_verbose(VERBOSE_PREFIX_2 "Extension state: Watcher for hint %s %s. Notify User %s\n", exten, state == AST_EXTENSION_DEACTIVATED ? "deactivated" : "removed", p->username);
		p->stateid = -1;
		p->subscribed = NONE;
		append_history(p, "Subscribestatus", state == AST_EXTENSION_REMOVED ? "HintRemoved" : "Deactivated");
		break;
	default:	/* Tell user */
		p->laststate = state;
		break;
	}
	transmit_state_notify(p, state, 1, 1);

	if (option_debug > 1)
		ast_verbose(VERBOSE_PREFIX_1 "Extension Changed %s new state %s for Notify User %s\n", exten, ast_extension_state2str(state), p->username);
	return 0;
}

static int check_unique_register(char *username, const char *req_domain)
{
     	char data[256];
 	char *addr, *scan;
 
     	if(ast_db_get("SIP/Registry", username, data, sizeof(data)))
         	return 1;
 
 	scan = data;
 	addr = strsep(&scan, ":");
 
     	if (strcmp(addr, req_domain))
         	return 0;
 	
     	return 1;
}

/*! \brief  register_verify: Verify registration of user */
static int register_verify(struct sip_pvt *p, struct sockaddr_in *sin, struct sip_request *req, char *uri, int ignore)
{
	int res = -3;
	struct sip_peer *peer;
	char tmp[256];
	char iabuf[INET_ADDRSTRLEN];
	char *name, *c;
	char *t;
	char *domain = NULL;

	/* Terminate URI */
	t = uri;
	while(*t && (*t > 32) && (*t != ';'))
		t++;
	*t = '\0';
	
	ast_copy_string(tmp, get_header(req, "To"), sizeof(tmp));
	if (pedanticsipchecking)
		ast_uri_decode(tmp);

	c = get_in_brackets(tmp);
	/* Ditch ;user=phone */
	name = strchr(c, ';');
	if (name)
		*name = '\0';

	if (!strncmp(c, "sip:", 4)) {
		name = c + 4;
	} else {
		name = c;
		ast_log(LOG_NOTICE, "Invalid to address: '%s' from %s (missing sip:) trying to use anyway...\n", c, ast_inet_ntoa(iabuf, sizeof(iabuf), sin->sin_addr));
	}

	/* Strip off the domain name */
	if ((c = strchr(name, '@'))) {
		*c++ = '\0';
		domain = c;
		if ((c = strchr(domain, ':')))	/* Remove :port */
			*c = '\0';
		if (!AST_LIST_EMPTY(&domain_list)) {
			if (!check_sip_domain(domain, NULL, 0)) {
				transmit_response(p, "404 Not found (unknown domain)", &p->initreq);
				return -3;
			}
		}
	}

	ast_copy_string(p->exten, name, sizeof(p->exten));
	build_contact(p);
	peer = find_create_template_peer(name, NULL, 1, &res);
	if (!(peer && ast_apply_ha(peer->ha, sin))) {
		if (peer)
			ASTOBJ_UNREF(peer,sip_destroy_peer);
	}
	if (peer) {
		if (!ast_test_flag(peer, SIP_DYNAMIC)) {
			ast_log(LOG_ERROR, "Peer '%s' is trying to register, but not configured as host=dynamic\n", peer->name);
		} else if (global_template_registration_forbid && ast_test_flag(&(peer->flags_page2), SIP_PAGE2_FROM_TEMPLATE)) {
	    		res = -5;
		} else {
			ast_copy_flags(p, peer, SIP_NAT);
			transmit_response(p, "100 Trying", req);
			if (check_for_unique_register && !check_unique_register(name, ast_inet_ntoa(iabuf, sizeof(iabuf), sin->sin_addr))){
 			    	res = -5; 
 			}
			else if (!(res = check_auth(p, req, p->randdata, sizeof(p->randdata), peer->username, peer->secret, peer->md5secret, SIP_REGISTER, uri, 0, ignore))) {
				sip_cancel_destroy(p);
				switch (parse_register_contact(p, peer, req)) {
				case PARSE_REGISTER_FAILED:
					ast_log(LOG_WARNING, "Failed to parse contact info\n");
					break;
				case PARSE_REGISTER_QUERY:
					transmit_response_with_date(p, "200 OK", req);
					peer->lastmsgssent = -1;
					res = 0;
					break;
				case PARSE_REGISTER_UPDATE:
					update_peer(peer, p->expiry);
					/* Say OK and ask subsystem to retransmit msg counter */
					transmit_response_with_date(p, "200 OK", req);
					peer->lastmsgssent = -1;
					res = 0;
					break;
				}
			} 
		}
	}
	if (!peer && autocreatepeer) {
		/* Create peer if we have autocreate mode enabled */
		peer = temp_peer(name);
		if (peer) {
			ASTOBJ_CONTAINER_LINK(&peerl, peer);
			peer->lastmsgssent = -1;
			sip_cancel_destroy(p);
			switch (parse_register_contact(p, peer, req)) {
			case PARSE_REGISTER_FAILED:
				ast_log(LOG_WARNING, "Failed to parse contact info\n");
				break;
			case PARSE_REGISTER_QUERY:
				transmit_response_with_date(p, "200 OK", req);
				peer->lastmsgssent = -1;
				res = 0;
				break;
			case PARSE_REGISTER_UPDATE:
				/* Say OK and ask subsystem to retransmit msg counter */
				transmit_response_with_date(p, "200 OK", req);
				manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Registered\r\n", peer->name);
				peer->lastmsgssent = -1;
				res = 0;
				break;
			}
		}
	}
	if (!res) {
		ast_device_state_changed("SIP/%s", peer->name);
	}
	if (res < 0) {
		switch (res) {
		case -1:
			/* Wrong password in authentication. Go away, don't try again until you fixed it */
			transmit_response(p, "403 Forbidden (Bad auth)", &p->initreq);
			break;
		case -2:
			/* Username and digest username does not match. 
			   Asterisk uses the From: username for authentication. We need the
			   users to use the same authentication user name until we support
			   proper authentication by digest auth name */
			transmit_response(p, "403 Authentication user name does not match account name", &p->initreq);
			break;
		case -3:
			/* URI not found */
			transmit_response(p, "404 Not found", &p->initreq);
			/* Set res back to -2 because we don't want to return an invalid domain message. That check already happened up above. */
			res = -2;
			break;
		case -4:
			/* Maximum from templates peers */
			transmit_response(p, "480 Temporarily Unavailable (Register limit)", &p->initreq);
			break;
		case -5:
			/* Registration failed */
			transmit_response(p, "503 Service Unavailable", &p->initreq);
			break;
		}
		if (option_debug > 1) {
			switch (res) {
			case -1: ast_log(LOG_DEBUG, "SIP REGISTER attempt failed for %s : Bad password\n", name);
				 break;
			case -2: ast_log(LOG_DEBUG, "SIP REGISTER attempt failed for %s : Bad digest user\n", name);
				 break;
			case -3: ast_log(LOG_DEBUG, "SIP REGISTER attempt failed for %s : Not found\n", name);
				 break;
			case -4: ast_log(LOG_DEBUG, "SIP REGISTER attempt failed for %s : Registration limit\n", name);
				 break;
			case -5: ast_log(LOG_DEBUG, "SIP REGISTER attempt failed for %s : Registration failed\n", name);
				 break;				 
			}
		}
	}
	if (peer)
		ASTOBJ_UNREF(peer,sip_destroy_peer);

	return res;
}

/*! \brief  get_rdnis: get referring dnis ---*/
static int get_rdnis(struct sip_pvt *p, struct sip_request *oreq)
{
	char tmp[256], *c, *a;
	struct sip_request *req;
	
	req = oreq;
	if (!req)
		req = &p->initreq;
	ast_copy_string(tmp, get_header(req, "Diversion"), sizeof(tmp));
	if (ast_strlen_zero(tmp))
		return 0;
	c = get_in_brackets(tmp);
	if (strncmp(c, "sip:", 4)) {
		ast_log(LOG_WARNING, "Huh?  Not an RDNIS SIP header (%s)?\n", c);
		return -1;
	}
	c += 4;
	if ((a = strchr(c, '@')) || (a = strchr(c, ';'))) {
		*a = '\0';
	}
	if (sip_debug_test_pvt(p))
		ast_verbose("RDNIS is %s\n", c);
	ast_copy_string(p->rdnis, c, sizeof(p->rdnis));

	return 0;
}

/*! \brief  get_destination: Find out who the call is for --*/
static int get_destination(struct sip_pvt *p, struct sip_request *oreq)
{
	char tmp[256] = "", *uri, *a;
	char tmpf[256], *from;
	struct sip_request *req;
	
	req = oreq;
	if (!req)
		req = &p->initreq;
	if (req->rlPart2)
		ast_copy_string(tmp, req->rlPart2, sizeof(tmp));
	uri = get_in_brackets(tmp);
	
	ast_copy_string(tmpf, get_header(req, "From"), sizeof(tmpf));

	from = get_in_brackets(tmpf);
	
	if (strncmp(uri, "sip:", 4)) {
		ast_log(LOG_WARNING, "Huh?  Not a SIP header (%s)?\n", uri);
		return -1;
	}
	uri += 4;
	if (!ast_strlen_zero(from)) {
		if (strncmp(from, "sip:", 4)) {
			ast_log(LOG_WARNING, "Huh?  Not a SIP header (%s)?\n", from);
			return -1;
		}
		from += 4;
		/* XXX Workaround for B36304. Revert when upgrading to asterisk
		 * 1.4 */
		if (pedanticsipchecking)
			ast_uri_decode(from);
	} else
		from = NULL;

	if (pedanticsipchecking)
		ast_uri_decode(uri);

	/* Get the target domain */
	if ((a = strchr(uri, '@'))) {
		char *colon;
		*a = '\0';
		a++;
		colon = strchr(a, ':'); /* Remove :port */
		if (colon)
			*colon = '\0';
		ast_copy_string(p->domain, a, sizeof(p->domain));
	}
	/* Skip any options */
	if ((a = strchr(uri, ';'))) {
		*a = '\0';
	}

	if (!AST_LIST_EMPTY(&domain_list)) {
		char domain_context[AST_MAX_EXTENSION];

		domain_context[0] = '\0';
		if (!check_sip_domain(p->domain, domain_context, sizeof(domain_context))) {
			if (!allow_external_domains && (req->method == SIP_INVITE || req->method == SIP_REFER)) {
				ast_log(LOG_DEBUG, "Got SIP %s to non-local domain '%s'; refusing request.\n", sip_methods[req->method].text, p->domain);
				return -2;
			}
		}
		/* If we have a context defined, overwrite the original context */
		if (!ast_strlen_zero(domain_context))
			ast_copy_string(p->context, domain_context, sizeof(p->context));
	}

	if (from) {
		if ((a = strchr(from, ';')))
			*a = '\0';
		if ((a = strchr(from, '@'))) {
			*a = '\0';
			ast_copy_string(p->fromdomain, a + 1, sizeof(p->fromdomain));
		} else
			ast_copy_string(p->fromdomain, from, sizeof(p->fromdomain));
	}
	if (sip_debug_test_pvt(p))
		ast_verbose("Looking for %s in %s (domain %s)\n", uri, p->context, p->domain);

	/* Return 0 if we have a matching extension */
	if (ast_exists_extension(NULL, p->context, uri, 1, from) ||
		!strcmp(uri, ast_pickup_ext())) {
		if (!oreq)
			ast_copy_string(p->exten, uri, sizeof(p->exten));
		return 0;
	}

	/* Return 1 for overlap dialling support */
	if (ast_canmatch_extension(NULL, p->context, uri, 1, from) ||
	    !strncmp(uri, ast_pickup_ext(),strlen(uri))) {
		return 1;
	}
	
	return -1;
}

static int sip_extract_tag(char **in) 
{
	char *tag;

	if ((tag = strcasestr(*in, "tag="))) {
		char *ptr;
		tag += 4;
		if ((ptr = strchr(tag, ';'))) {
			*ptr = '\0';
		}
		*in = tag;
		return 0;
	}
	return -1;
}

/*! \brief  get_sip_pvt_byid_locked: Lock interface lock and find matching pvt lock  ---*/
static struct sip_pvt *get_sip_pvt_byid_locked(char *callid, struct sip_request *req, char *totag, char *fromtag) 
{
	struct sip_pvt *sip_pvt_ptr = NULL;
	
	/* Search interfaces and find the match */
	ast_mutex_lock(&iflock);
	for (sip_pvt_ptr = iflist; sip_pvt_ptr; sip_pvt_ptr = sip_pvt_ptr->next) {
		if (!strcmp(sip_pvt_ptr->callid, callid)) {
			char *real_totag = NULL, *real_fromtag = NULL;
			int match = 1;

			/* Go ahead and lock it (and its owner) before returning */
			ast_mutex_lock(&sip_pvt_ptr->lock);

			if (req && pedanticsipchecking) {
				if (totag) {
					real_totag = ast_strdupa(get_header(req, "To"));
					if (sip_extract_tag(&real_totag)) {
						real_totag = NULL;
					}
					if (strcmp(real_totag, totag)) {
						match = 0;
					}
				}
				if (match && fromtag) {
					real_fromtag = ast_strdupa(get_header(req, "From"));
					if (sip_extract_tag(&real_fromtag)) {
						real_fromtag = NULL;
					}
					if (strcmp(real_fromtag, fromtag)) {
						match = 0;
					}
				}
			}
			
			if (!match) {
				ast_mutex_unlock(&sip_pvt_ptr->lock);
				continue;
			}
			
			if (sip_pvt_ptr->owner) {
				while(ast_mutex_trylock(&sip_pvt_ptr->owner->lock)) {
					ast_mutex_unlock(&sip_pvt_ptr->lock);
					usleep(1);
					ast_mutex_lock(&sip_pvt_ptr->lock);
					if (!sip_pvt_ptr->owner)
						break;
				}
			}
			break;
		}
	}
	ast_mutex_unlock(&iflock);
	return sip_pvt_ptr;
}

/*! \brief  get_refer_info: Call transfer support (the REFER method) ---*/
static int get_refer_info(struct sip_pvt *sip_pvt, struct sip_request *outgoing_req)
{

	char *p_refer_to = NULL, *p_referred_by = NULL, *h_refer_to = NULL, *h_referred_by = NULL, *h_contact = NULL;
	char *replace_callid = "", *refer_to = NULL, *refer_to_domain = NULL, *referred_by = NULL, *ptr = NULL, *replaces_header = NULL, *refer_uri;
	char replaces_callid_fromtag[BUFSIZ/2], replaces_callid_totag[BUFSIZ/2];
	struct sip_request *req = NULL;
	struct sip_pvt *sip_pvt_ptr = NULL;
	struct ast_channel *chan = NULL, *peer = NULL;

	req = outgoing_req;

	if (!req) {
		req = &sip_pvt->initreq;
	}
	
	if (!( (p_refer_to = get_header(req, "Refer-To")) && (h_refer_to = ast_strdupa(p_refer_to)) )) {
		ast_log(LOG_WARNING, "No Refer-To Header That's illegal\n");
		return -1;
	}

	refer_to = get_in_brackets(h_refer_to);

	if (!( (p_referred_by = get_header(req, "Referred-By")) && (h_referred_by = ast_strdupa(p_referred_by)) )) {
		ast_log(LOG_WARNING, "No Referrred-By Header That's not illegal\n");
		return -1;
	} else {
		if (pedanticsipchecking) {
			ast_uri_decode(h_referred_by);
		}
		referred_by = get_in_brackets(h_referred_by);
	}
	h_contact = get_header(req, "Contact");
	
	if (strncmp(refer_to, "sip:", 4)) {
		ast_log(LOG_WARNING, "Refer-to: Huh?  Not a SIP header (%s)?\n", refer_to);
		return -1;
	}

	if (strncmp(referred_by, "sip:", 4)) {
		ast_log(LOG_WARNING, "Referred-by: Huh?  Not a SIP header (%s) Ignoring?\n", referred_by);
		referred_by = NULL;
	}

	if (refer_to)
		refer_to += 4;

	if (referred_by)
		referred_by += 4;
	
	refer_uri = ast_strdupa(refer_to);	
	
	if ((ptr = strchr(refer_to, '?'))) {
		/* Search for arguments */
		*ptr = '\0';
		ptr++;
		if (!strncasecmp(ptr, "REPLACES=", 9)) {
			char *to = NULL, *from = NULL;
			char *p;
			replace_callid = ast_strdupa(ptr + 9);
			ast_uri_decode(replace_callid);
			replaces_header = ast_strdupa(replace_callid); 
			if ((ptr = strchr(replace_callid, '%'))) 
				*ptr++ = '\0';
			if ((ptr = strchr(replace_callid, ';'))) 
				*ptr++ = '\0';
			/* Skip leading whitespace XXX memmove behaviour with overlaps ? */
			p = ast_skip_blanks(replace_callid);
			if (p != replace_callid)
				memmove(replace_callid, p, strlen(p));

			if (ptr) {
				/* Find the different tags before we destroy the string */
			    	to = strcasestr(ptr, "to-tag=");
				from = strcasestr(ptr, "from-tag=");
			}
			
			/* Grab the to header */
			if (to) {
				ptr = to + 7;
				if ((to = strchr(ptr, '&')))
					*to = '\0';
				if ((to = strchr(ptr, ';')))
					*to = '\0';
				ast_copy_string(replaces_callid_totag, ptr, sizeof(replaces_callid_totag));
			}

			if (from) {
				ptr = from + 9;
				if ((to = strchr(ptr, '&')))
					*to = '\0';
				if ((to = strchr(ptr, ';')))
					*to = '\0';
				ast_copy_string(replaces_callid_fromtag, ptr, sizeof(replaces_callid_fromtag));
			}
		}
	}
	
	if ((refer_to_domain = strchr(refer_to, '@'))) {	/* Separate domain */
		*refer_to_domain++ = '\0';
		if ((ptr = strchr(refer_to_domain, ';'))) 
			*ptr++ = '\0';
	}
	
	if (referred_by) {
		if ((ptr = strchr(referred_by, '@')))
			*ptr = '\0';
		if ((ptr = strchr(referred_by, ';'))) 
			*ptr = '\0';
	}
	
	if (sip_debug_test_pvt(sip_pvt)) {
		ast_verbose("Transfer to %s in %s\n", refer_to, sip_pvt->context);
		if (referred_by)
			ast_verbose("Transfer from %s in %s\n", referred_by, sip_pvt->context);
	}
	if (!ast_strlen_zero(replace_callid)) {	
		struct sip_pvt *p = iflist;
	        while(p) {
			if (!strcmp(p->callid, replace_callid) && ast_bridged_channel(p->owner))
			 	ast_indicate(ast_bridged_channel(p->owner), AST_CONTROL_UNHOLD);
			p = p->next;
	    	}

		/* This is a supervised transfer */
		ast_log(LOG_DEBUG,"Assigning Replace-Call-ID Info %s to REPLACE_CALL_ID\n",replace_callid);
		
		ast_copy_string(sip_pvt->refer_to, "", sizeof(sip_pvt->refer_to));
		ast_copy_string(sip_pvt->refer_to_domain, "", sizeof(sip_pvt->refer_to_domain));
		ast_copy_string(sip_pvt->referred_by, "", sizeof(sip_pvt->referred_by));
		ast_copy_string(sip_pvt->refer_contact, "", sizeof(sip_pvt->refer_contact));
		sip_pvt->refer_call = NULL;
		if ((sip_pvt_ptr = get_sip_pvt_byid_locked(replace_callid, req, replaces_callid_totag, replaces_callid_fromtag))) {
			sip_pvt->refer_call = sip_pvt_ptr;
			if (sip_pvt->refer_call == sip_pvt) {
				ast_log(LOG_NOTICE, "Supervised transfer attempted to transfer into same call id (%s == %s)!\n", replace_callid, sip_pvt->callid);
				sip_pvt->refer_call = NULL;
			} 
			return 0;
		} else {
			/* Don't ask me =0 ?, SIP made do it! */
			int cause = 0, res = -1;
			struct ast_channel *ichan = NULL;
			struct ast_codec_pref format;

			transmit_notify_with_sipfrag(sip_pvt, sip_pvt->ocseq);
			if ((ptr = strchr(refer_uri, ';'))) {
				*ptr = '\0';
			}

                        if (sip_pvt->owner)
			    ast_codec_pref_set2(&format, sip_pvt->owner->readformat);
			else
			    ast_codec_pref_set2(&format, AST_FORMAT_ULAW);

			if ((ichan = sip_request_call("SIP", &format, refer_uri, &cause))) {
				struct ast_frame *f;
				char *rbuf;	
				ast_log(LOG_DEBUG, "Going hunting for a remote INVITE/Replaces at [%s] Wish me luck!\n", refer_uri);
				if ((rbuf = alloca(strlen(replaces_header) + 10))) {
					sprintf(rbuf, "Replaces: %s", replaces_header);
					sip_addheader(ichan, rbuf);
					sip_call(ichan, refer_uri, 20000);
					ast_channel_masquerade(sip_pvt->owner, ichan);
					if ((f = ast_read(ichan))) {
						ast_log(LOG_DEBUG, "WooHoo! The INVITE/Replaces Worked!\n");
						ast_frfree(f);
						transmit_response(sip_pvt, "202 Accepted", req);
						res = SIP_RETVAL_IGNORE; /* means do nothing more */
					} else {
						res = -1;
					}
				} else {
					ast_log(LOG_ERROR,"Memory Error!\n");
					res = -1;
				}

				ast_hangup(ichan);
			} else {
				res = -1;
			}
			return res;
		}
	} else if (ast_exists_extension(NULL, sip_pvt->context, refer_to, 1, NULL) || !strcmp(refer_to, ast_parking_ext())) {
		/* This is an unsupervised transfer (blind transfer) */
		
		ast_log(LOG_DEBUG,"Unsupervised transfer to (Refer-To): %s (domain %s)\n", refer_to, refer_to_domain ? : "");
		if (referred_by)
			ast_log(LOG_DEBUG,"Transferred by  (Referred-by: ) %s \n", referred_by);
		ast_log(LOG_DEBUG,"Transfer Contact Info %s (REFER_CONTACT)\n", h_contact);
		ast_copy_string(sip_pvt->refer_to, refer_to, sizeof(sip_pvt->refer_to));
		if (refer_to_domain)
			ast_copy_string(sip_pvt->refer_to_domain, refer_to_domain, sizeof(sip_pvt->refer_to_domain));
		if (referred_by)
			ast_copy_string(sip_pvt->referred_by, referred_by, sizeof(sip_pvt->referred_by));
		if (h_contact) {
			ast_copy_string(sip_pvt->refer_contact, h_contact, sizeof(sip_pvt->refer_contact));
		}
		sip_pvt->refer_call = NULL;
		if ((chan = sip_pvt->owner) && (peer = ast_bridged_channel(sip_pvt->owner))) {
			pbx_builtin_setvar_helper(chan, "BLINDTRANSFER", peer->name);
			pbx_builtin_setvar_helper(peer, "BLINDTRANSFER", chan->name);
			pbx_builtin_setvar_helper(peer, "PEERTOUSE", sip_pvt->refer_to_domain);
		}
		return 0;
	} else if (ast_canmatch_extension(NULL, sip_pvt->context, refer_to, 1, NULL)) {
		return 1;
	}

	return -1;
}

/*! \brief  get_also_info: Call transfer support (old way, depreciated)--*/
static int get_also_info(struct sip_pvt *p, struct sip_request *oreq)
{
	char tmp[256], *c, *a;
	struct sip_request *req;
	
	req = oreq;
	if (!req)
		req = &p->initreq;
	ast_copy_string(tmp, get_header(req, "Also"), sizeof(tmp));
	
	c = get_in_brackets(tmp);
	
		
	if (strncmp(c, "sip:", 4)) {
		ast_log(LOG_WARNING, "Huh?  Not a SIP header (%s)?\n", c);
		return -1;
	}
	c += 4;
	if ((a = strchr(c, '@')))
		*a = '\0';
	if ((a = strchr(c, ';'))) 
		*a = '\0';
	
	if (sip_debug_test_pvt(p)) {
		ast_verbose("Looking for %s in %s\n", c, p->context);
	}
	if (ast_exists_extension(NULL, p->context, c, 1, NULL)) {
		/* This is an unsupervised transfer */
		ast_log(LOG_DEBUG,"Assigning Extension %s to REFER-TO\n", c);
		ast_copy_string(p->refer_to, c, sizeof(p->refer_to));
		ast_copy_string(p->referred_by, "", sizeof(p->referred_by));
		ast_copy_string(p->refer_contact, "", sizeof(p->refer_contact));
		p->refer_call = NULL;
		return 0;
	} else if (ast_canmatch_extension(NULL, p->context, c, 1, NULL)) {
		return 1;
	}

	return -1;
}

/*! \brief check Via: header for hostname, port and rport request/answer */
static int check_via(struct sip_pvt *p, struct sip_request *req)
{
	char via[256];
	char iabuf[INET_ADDRSTRLEN];
	char *c, *pt;
	struct hostent *hp;
	struct ast_hostent ahp;

	ast_copy_string(via, get_header(req, "Via"), sizeof(via));

	/* Check for rport */
	c = strstr(via, ";rport");
	if (c && (c[6] != '='))	/* rport query, not answer */
		ast_set_flag(p, SIP_NAT_ROUTE);

	c = strchr(via, ';');
	if (c) 
		*c = '\0';

	c = strchr(via, ' ');
	if (c) {
		*c = '\0';
		c = ast_skip_blanks(c+1);
		if (strcasecmp(via, "SIP/2.0/UDP")) {
			ast_log(LOG_WARNING, "Don't know how to respond via '%s'\n", via);
			return -1;
		}
		pt = strchr(c, ':');
		if (pt)
			*pt++ = '\0';	/* remember port pointer */
		hp = ast_gethostbyname(c, &ahp);
		if (!hp) {
			ast_log(LOG_WARNING, "'%s' is not a valid host\n", c);
			return -1;
		}
		memset(&p->sa, 0, sizeof(p->sa));
		p->sa.sin_family = AF_INET;
		memcpy(&p->sa.sin_addr, hp->h_addr, sizeof(p->sa.sin_addr));
		p->sa.sin_port = htons(pt ? atoi(pt) : DEFAULT_SIP_PORT);

		if (sip_debug_test_pvt(p)) {
			c = (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE) ? "NAT" : "non-NAT";
			ast_verbose("Sending to %s : %d (%s)\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr), ntohs(p->sa.sin_port), c);
		}
	}
	return 0;
}

/*! \brief  get_calleridname: Get caller id name from SIP headers ---*/
static char *get_calleridname(char *input, char *output, size_t outputsize)
{
	char *end = strchr(input,'<');
	char *tmp = strchr(input,'\"');
	int bytes = 0;
	int maxbytes = outputsize - 1;

	if (!end || (end == input)) return NULL;
	/* move away from "<" */
	end--;
	/* we found "name" */
	if (tmp && tmp < end) {
		end = strchr(tmp+1, '\"');
		if (!end) return NULL;
		bytes = (int) (end - tmp);
		/* protect the output buffer */
		if (bytes > maxbytes)
			bytes = maxbytes;
		ast_copy_string(output, tmp + 1, bytes);
	} else {
		/* we didn't find "name" */
		/* clear the empty characters in the begining*/
		input = ast_skip_blanks(input);
		/* clear the empty characters in the end */
		while(*end && (*end < 33) && end > input)
			end--;
		if (end >= input) {
			bytes = (int) (end - input) + 2;
			/* protect the output buffer */
			if (bytes > maxbytes) {
				bytes = maxbytes;
			}
			ast_copy_string(output, input, bytes);
		}
		else
			return NULL;
	}
	return output;
}

static int get_asserted_id_num(char *header, char *num, int num_len)
{
	char *start, *end;

	start = strchr(header, ':');
	if (!start) 
	{
		num[0] = '\0';
		ast_log(LOG_DEBUG, "Empty P-Asserted-Identity "
			"header received:%s\n", header);
		return -1;
	}
	start++;

	/* we found the beginning of the number */
	ast_copy_string(num, start, num_len);

	end = strchr(num, '@');
	if (end)
		*end = '\0';
	else
		num[0] = '\0'; /* uri not valid, can't find number */

	return !num[0];
}

/*! \brief  get_rpid_num: Get caller id number from Remote-Party-ID header field 
 *	Returns true if number should be restricted (privacy setting found)
 *	output is set to NULL if no number found
 */
static int get_rpid_num(char *input,char *output, int maxlen)
{
	char *start;
	char *end;

	start = strchr(input,':');
	if (!start) {
		output[0] = '\0';
		return 0;
	}
	start++;

	/* we found "number" */
	ast_copy_string(output,start,maxlen);
	output[maxlen-1] = '\0';

	end = strchr(output,'@');
	if (end)
		*end = '\0';
	else
		output[0] = '\0';
	if (strstr(input,"privacy=full") || strstr(input,"privacy=uri"))
		return AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;

	return 0;
}


/*! \brief  check_user_full: Check if matching user or peer is defined ---*/
/* 	Match user on From: user name and peer on IP/port */
/*	This is used on first invite (not re-invites) and subscribe requests */
static int check_user_full(struct sip_pvt *p, struct sip_request *req, int sipmethod, char *uri, int reliable, struct sockaddr_in *sin, int ignore, char *mailbox, int mailboxlen)
{
	struct sip_user *user = NULL;
	struct sip_peer *peer;
	char *of, from[256], *c;
	char *rpid,rpid_num[50];
	char *asserted_id, asserted_id_num[50];
	char iabuf[INET_ADDRSTRLEN];
	int res = 0;
	char *t;
	char calleridname[50];
	int debug=sip_debug_test_addr(sin);
	struct ast_variable *tmpvar = NULL, *v = NULL;

	/* Terminate URI */
	t = uri;
	while(*t && (*t > 32) && (*t != ';'))
		t++;
	*t = '\0';

	ast_copy_string(from, get_header(req, "From"), sizeof(from));
	if (pedanticsipchecking)
		ast_uri_decode(from);
	
	memset(calleridname,0,sizeof(calleridname));
	get_calleridname(from, calleridname, sizeof(calleridname));
	if (calleridname[0])
		ast_copy_string(p->cid_name, calleridname, sizeof(p->cid_name));

	rpid = get_header(req, "Remote-Party-ID");
	memset(rpid_num,0,sizeof(rpid_num));
	if (!ast_strlen_zero(rpid)) 
		p->callingpres = get_rpid_num(rpid,rpid_num, sizeof(rpid_num));

	asserted_id_num[0] = 0;
	if (use_asserted_identity)
	{
		/* if we got PAI header, use its number as caller id */
		asserted_id = get_header(req, "P-Asserted-Identity");
		if (!ast_strlen_zero(asserted_id))
		{
			get_asserted_id_num(asserted_id, asserted_id_num,
				sizeof(asserted_id_num));
		}
	}

	of = get_in_brackets(from);
	if (ast_strlen_zero(p->exten)) {
		t = uri;
		if (!strncmp(t, "sip:", 4))
			t+= 4;
		ast_copy_string(p->exten, t, sizeof(p->exten));
		t = strchr(p->exten, '@');
		if (t)
			*t = '\0';
		if (ast_strlen_zero(p->our_contact))
			build_contact(p);
	}
	/* save the URI part of the From header */
	ast_copy_string(p->from, of, sizeof(p->from));
	if (strncmp(of, "sip:", 4)) {
		ast_log(LOG_NOTICE, "From address missing 'sip:', using it anyway\n");
	} else
		of += 4;
	/* Get just the username part */
	if ((c = strchr(of, '@'))) {
		*c = '\0';
		if ((c = strchr(of, ':')))
			*c = '\0';
		ast_copy_string(p->cid_num, ast_callerid_adjust(of), sizeof(p->cid_num));
		ast_shrink_phone_number(p->cid_num);

		if(!strcasecmp(p->cid_num, NETWORK_ANONYMOUS_CALLERID))
			ast_copy_string(p->cid_num, AST_ANONYMOUS_CALLER, sizeof(p->cid_num));
	}

	if (!mailbox)	/* If it's a mailbox SUBSCRIBE, don't check users */
		user = find_user(of, 1);

	/* Find user based on user name in the from header */
	if (user && ast_apply_ha(user->ha, sin)) {
		ast_copy_flags(p, user, SIP_FLAGS_TO_COPY);
		/* copy channel vars */
		for (v = user->chanvars ; v ; v = v->next) {
			if ((tmpvar = ast_variable_new(v->name, v->value))) {
				tmpvar->next = p->chanvars; 
				p->chanvars = tmpvar;
			}
		}
		p->usercapability = user->capability;
		p->userprefs = user->prefs;
		/* replace callerid if rpid found, and not restricted */
		if (!ast_strlen_zero(rpid_num) && ast_test_flag(p, SIP_TRUSTRPID)) {
			if (*calleridname)
				ast_copy_string(p->cid_name, calleridname, sizeof(p->cid_name));
			ast_copy_string(p->cid_num, rpid_num, sizeof(p->cid_num));
			ast_shrink_phone_number(p->cid_num);
		}

		/* replace callerid if asserted_id found */
		if (!ast_strlen_zero(asserted_id_num))
		{
			ast_copy_string(p->cid_num, asserted_id_num, sizeof(p->cid_num));
			ast_shrink_phone_number(p->cid_num);
		}

		if (p->rtp) {
			ast_log(LOG_DEBUG, "Setting NAT on RTP to %d\n", (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
			ast_rtp_setnat(p->rtp, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
		}
		if (p->vrtp) {
			ast_log(LOG_DEBUG, "Setting NAT on VRTP to %d\n", (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
			ast_rtp_setnat(p->vrtp, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
		}
#if defined(T38_SUPPORT)
		if (p->udptl) {
			ast_log(LOG_DEBUG, "Setting NAT on UDPTL to %d\n", (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
			ast_udptl_setnat(p->udptl, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
		}
#endif
		if (ast_test_flag(user, SIP_INSECURE_INVITE)) {
 		    /* Pretend there is no required authentication */
 		    user->secret[0] = '\0';
 		    user->md5secret[0] = '\0';
 		}
		if (!(res = check_auth(p, req, p->randdata, sizeof(p->randdata), user->username, user->secret, user->md5secret, sipmethod, uri, reliable, ignore))) {
			sip_cancel_destroy(p);
			ast_copy_flags(p, user, SIP_FLAGS_TO_COPY);
			/* Copy SIP extensions profile from INVITE */
			if (p->sipoptions)
				user->sipoptions = p->sipoptions;

			/* If we have a call limit, set flag */
			if (user->call_limit)
				ast_set_flag(p, SIP_CALL_LIMIT);
			if (!ast_strlen_zero(user->context))
				ast_copy_string(p->context, user->context, sizeof(p->context));
			if (!ast_strlen_zero(user->cid_num) && !ast_strlen_zero(p->cid_num))  {
				ast_copy_string(p->cid_num, user->cid_num, sizeof(p->cid_num));
				ast_shrink_phone_number(p->cid_num);
			}
			if (!ast_strlen_zero(user->cid_name) && !ast_strlen_zero(p->cid_num)) 
				ast_copy_string(p->cid_name, user->cid_name, sizeof(p->cid_name));
			ast_copy_string(p->username, user->name, sizeof(p->username));
			ast_copy_string(p->peersecret, user->secret, sizeof(p->peersecret));
			ast_copy_string(p->subscribecontext, user->subscribecontext, sizeof(p->subscribecontext));
			ast_copy_string(p->peermd5secret, user->md5secret, sizeof(p->peermd5secret));
			ast_copy_string(p->accountcode, user->accountcode, sizeof(p->accountcode));
			ast_copy_string(p->language, user->language, sizeof(p->language));
			ast_copy_string(p->musicclass, user->musicclass, sizeof(p->musicclass));
			p->amaflags = user->amaflags;
			p->callgroup = user->callgroup;
			p->pickupgroup = user->pickupgroup;
			ast_codec_pref_remove2(&p->formats, ~user->capability);
			p->callingpres = user->callingpres;
			if ((ast_test_flag(p, SIP_DTMF) == SIP_DTMF_RFC2833) || (ast_test_flag(p, SIP_DTMF) == SIP_DTMF_AUTO))
				p->noncodeccapability |= AST_RTP_DTMF;
			else
				p->noncodeccapability &= ~AST_RTP_DTMF;
#if defined(T38_SUPPORT)
			if (p->t38peercapability)
				p->t38jointcapability &= p->t38peercapability;
#endif
			ast_copy_flags(&p->flags_page2, &user->flags_page2, 
				SIP_SESSION_TIMERS_FLAGS_TO_COPY);
		}
		if (user && debug)
			ast_verbose("Found user '%s'\n", user->name);
	} else {
		if (user) {
			if (!mailbox && debug)
				ast_verbose("Found user '%s', but fails host access\n", user->name);
			ASTOBJ_UNREF(user,sip_destroy_user);
		}
		user = NULL;
	}

	if (!user) {
		/* If we didn't find a user match, check for peers */
		if (sipmethod == SIP_SUBSCRIBE)
			/* For subscribes, match on peer name only */
			peer = find_peer(of, NULL, 1);
		else {
			/* Look for peer based on the sip uri */
			peer = find_peer(p->exten, NULL, 1);
		}

		if (peer) {
			if (debug)
				ast_verbose("Found peer '%s'\n", peer->name);
			/* Take the peer */
			ast_copy_flags(p, peer, SIP_FLAGS_TO_COPY);

			/* Copy SIP extensions profile to peer */
			if (p->sipoptions)
				peer->sipoptions = p->sipoptions;

			/* replace callerid if rpid found, and not restricted */
			if (!ast_strlen_zero(rpid_num) && ast_test_flag(p, SIP_TRUSTRPID)) {
				if (*calleridname)
					ast_copy_string(p->cid_name, calleridname, sizeof(p->cid_name));
				ast_copy_string(p->cid_num, rpid_num, sizeof(p->cid_num));
				ast_shrink_phone_number(p->cid_num);
			}
			if (p->rtp) {
				ast_log(LOG_DEBUG, "Setting NAT on RTP to %d\n", (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
				ast_rtp_setnat(p->rtp, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
				if (peer->rtcp_interval) {
				    ast_log(LOG_DEBUG, "Setting RTCP interval to %dms\n",
					peer->rtcp_interval);
				    ast_rtp_setrtcpinterval(p->rtp, peer->rtcp_interval);
				}
			}
			if (p->vrtp) {
				ast_log(LOG_DEBUG, "Setting NAT on VRTP to %d\n", (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
				ast_rtp_setnat(p->vrtp, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
			}
#if defined(T38_SUPPORT)
			if (p->udptl) {
				ast_log(LOG_DEBUG, "Setting NAT on UDPTL to %d\n", (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
				ast_udptl_setnat(p->udptl, (ast_test_flag(p, SIP_NAT) & SIP_NAT_ROUTE));
			}
#endif
			ast_copy_string(p->peersecret, peer->secret, sizeof(p->peersecret));
			p->peersecret[sizeof(p->peersecret)-1] = '\0';
			ast_copy_string(p->subscribecontext, peer->subscribecontext, sizeof(p->subscribecontext));
			ast_copy_string(p->peermd5secret, peer->md5secret, sizeof(p->peermd5secret));
			p->peermd5secret[sizeof(p->peermd5secret)-1] = '\0';
			p->callingpres = peer->callingpres;
			if (peer->maxms && peer->lastms)
				p->timer_t1 = peer->lastms < DEFAULT_T1MIN ? DEFAULT_T1MIN : peer->lastms;
			if (ast_test_flag(peer, SIP_INSECURE_INVITE)) {
				/* Pretend there is no required authentication */
				p->peersecret[0] = '\0';
				p->peermd5secret[0] = '\0';
			}
			if (!(res = check_auth(p, req, p->randdata, sizeof(p->randdata), peer->username, p->peersecret, p->peermd5secret, sipmethod, uri, reliable, ignore))) {
				ast_copy_flags(p, peer, SIP_FLAGS_TO_COPY);
				/* If we have a call limit, set flag */
				if (peer->call_limit)
					ast_set_flag(p, SIP_CALL_LIMIT);
				ast_copy_string(p->peername, peer->name, sizeof(p->peername));
				ast_copy_string(p->authname, peer->name, sizeof(p->authname));
				if (!ast_strlen_zero(peer->regname))
					ast_copy_string(p->regname, peer->regname, sizeof(p->regname));
				/* copy channel vars */
				for (v = peer->chanvars ; v ; v = v->next) {
					if ((tmpvar = ast_variable_new(v->name, v->value))) {
						tmpvar->next = p->chanvars; 
						p->chanvars = tmpvar;
					}
				}
				if (mailbox)
					snprintf(mailbox, mailboxlen, ",%s,", peer->mailbox);
				if (!ast_strlen_zero(peer->username)) {
					ast_copy_string(p->username, peer->username, sizeof(p->username));
					/* Use the default username for authentication on outbound calls */
					ast_copy_string(p->authname, peer->username, sizeof(p->authname));
				}
				if (!ast_strlen_zero(peer->cid_num) && !ast_strlen_zero(p->cid_num))  {
					ast_copy_string(p->cid_num, peer->cid_num, sizeof(p->cid_num));
					ast_shrink_phone_number(p->cid_num);
				}
				if (!ast_strlen_zero(peer->cid_name) && !ast_strlen_zero(p->cid_name)) 
					ast_copy_string(p->cid_name, peer->cid_name, sizeof(p->cid_name));
				ast_copy_string(p->fullcontact, peer->fullcontact, sizeof(p->fullcontact));
				if (!ast_strlen_zero(peer->context))
					ast_copy_string(p->context, peer->context, sizeof(p->context));
				ast_copy_string(p->peersecret, peer->secret, sizeof(p->peersecret));
				ast_copy_string(p->peermd5secret, peer->md5secret, sizeof(p->peermd5secret));
				ast_copy_string(p->language, peer->language, sizeof(p->language));
				ast_copy_string(p->accountcode, peer->accountcode, sizeof(p->accountcode));
				p->prack_level = peer->prack_level;
				p->amaflags = peer->amaflags;
				p->callgroup = peer->callgroup;
				p->faxtxcodecs = peer->faxtxcodecs;
				p->modemtxcodecs = peer->modemtxcodecs;
				p->faxmethod = peer->faxmethod;
				p->pickupgroup = peer->pickupgroup;
				ast_codec_pref_remove2(&p->formats, ~peer->capability);
				p->usercapability = peer->capability;
				p->userprefs = peer->prefs;
				ast_copy_flags((&p->flags_page2),(&peer->flags_page2), SIP_PAGE2_G729_ANNEXB);
				ast_copy_flags(&p->flags_page2, &peer->flags_page2, 
					SIP_SESSION_TIMERS_FLAGS_TO_COPY);
				set_pvt_allowed_methods(p, req);
				if ((ast_test_flag(p, SIP_DTMF) == SIP_DTMF_RFC2833) || (ast_test_flag(p, SIP_DTMF) == SIP_DTMF_AUTO))
					p->noncodeccapability |= AST_RTP_DTMF;
				else
					p->noncodeccapability &= ~AST_RTP_DTMF;
#if defined(T38_SUPPORT)
				if (p->t38peercapability)
					p->t38jointcapability &= p->t38peercapability;
#endif
			}
			ASTOBJ_UNREF(peer,sip_destroy_peer);
		} else { 
			if (debug)
				ast_verbose("Found no matching peer or user for '%s:%d'\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr), ntohs(p->recv.sin_port));

			/* do we allow guests? */
			if (!global_allowguest)
				res = -1;  /* we don't want any guests, authentication will fail */
#ifdef OSP_SUPPORT			
			else if (global_allowguest == 2) {
				ast_copy_flags(p, &global_flags, SIP_OSPAUTH);
				res = check_auth(p, req, p->randdata, sizeof(p->randdata), "", "", "", sipmethod, uri, reliable, ignore); 
			}
#endif
		}

	}

	if (user)
		ASTOBJ_UNREF(user,sip_destroy_user);
	return res;
}

/*! \brief  check_user: Find user ---*/
static int check_user(struct sip_pvt *p, struct sip_request *req, int sipmethod, char *uri, int reliable, struct sockaddr_in *sin, int ignore)
{
	return check_user_full(p, req, sipmethod, uri, reliable, sin, ignore, NULL, 0);
}

/*! \brief  get_msg_text: Get text out of a SIP MESSAGE packet ---*/
static int get_msg_text(char *buf, int len, struct sip_request *req)
{
	int x;
	int y;

	buf[0] = '\0';
	y = len - strlen(buf) - 5;
	if (y < 0)
		y = 0;
	for (x=0;x<req->lines;x++) {
		strncat(buf, req->line[x], y); /* safe */
		y -= strlen(req->line[x]) + 1;
		if (y < 0)
			y = 0;
		if (y != 0)
			strcat(buf, "\n"); /* safe */
	}
	return 0;
}

/*! \brief receive_text_message: Receive SIP MESSAGE with textual (text/plain) content */
/* Handles both in-call and out-of-call messages */
static void receive_text_message(struct sip_pvt *p, struct sip_request *req)
{
	char buf[1024];
	struct ast_frame f;

	if (get_msg_text(buf, sizeof(buf), req)) {
		ast_log(LOG_WARNING, "Unable to retrieve text from %s\n", p->callid);
		transmit_response(p, "202 Accepted", req);
		ast_set_flag(p, SIP_NEEDDESTROY);
		return;
	}

	if (p->owner) {
		if (sip_debug_test_pvt(p))
			ast_verbose("Message received: '%s'\n", buf);
		memset(&f, 0, sizeof(f));
		f.frametype = AST_FRAME_TEXT;
		f.subclass = 0;
		f.offset = 0;
		f.data = buf;
		f.datalen = strlen(buf);
		ast_queue_frame(p->owner, &f);
		transmit_response(p, "202 Accepted", req); /* We respond 202 accepted, since we relay the message */
	} else { /* Message outside of a call */
		manager_event_message(p->peername, get_header(req, "Content-Type"), "", 
			buf, strlen(buf));
		transmit_response(p, "200 OK", req);
	}
	ast_set_flag(p, SIP_NEEDDESTROY);
	return;
}

/*! \brief receive_sms_message: Receive SIP MESSAGE with binary (application/vnd.3gpp.sms) content */
/* Handles out-of-call messages (doesn't bridge content to bridge peer) */
static void receive_sms_message(struct sip_pvt *p, struct sip_request *req)
{
	char *ip_sm_gw = get_header(req, "P-Asserted-Identity");
	manager_event_message(p->peername, get_header(req, "Content-Type"),
		get_in_brackets(ip_sm_gw), req->body, req->body_len);
	transmit_response(p, "200 OK", req);
	ast_set_flag(p, SIP_NEEDDESTROY);
	return;
}

/*! \brief  receive_message: Receive SIP MESSAGE method messages ---*/
/*	We handle messages both in-call and out-of-call */
/*	Reference: RFC 3428 */
static void receive_message(struct sip_pvt *p, struct sip_request *req, int ignore, struct sockaddr_in *sin, char *e)
{
	char *content_type;

	if (!p->owner)
	{
		set_pvt_allowed_methods(p, req);
		int res = check_user(p, req, SIP_MESSAGE, e, 1, sin, ignore);
		/* Authentication failed */
		if (res < 0)
		{
		  transmit_response(p, "403 Forbidden", req);
		  ast_set_flag(p, SIP_NEEDDESTROY);	
		}
		/* Peer will retry with authentication (407 response sent) */
		if (res > 0)
		  return;
	}

	content_type = get_header(req, "Content-Type");
	if (!strcmp(content_type, "text/plain")) /* text/plain attachment */
		receive_text_message(p, req);
	else if (!strcmp(content_type, "application/vnd.3gpp.sms"))
		receive_sms_message(p, req);
	else {
		transmit_response(p, "415 Unsupported Media Type", req); /* Good enough, or? */
		ast_set_flag(p, SIP_NEEDDESTROY);
	}
}

/*! \brief  sip_show_inuse: CLI Command to show calls within limits set by 
      call_limit ---*/
static int sip_show_inuse(int fd, int argc, char *argv[]) {
#define FORMAT  "%-25.25s %-15.15s %-15.15s \n"
#define FORMAT2 "%-25.25s %-15.15s %-15.15s \n"
	char ilimits[40];
	char iused[40];
	int showall = 0;

	if (argc < 3) 
		return RESULT_SHOWUSAGE;

	if (argc == 4 && !strcmp(argv[3],"all")) 
			showall = 1;
	
	ast_cli(fd, FORMAT, "* User name", "In use", "Limit");
	ASTOBJ_CONTAINER_TRAVERSE(&userl, 1, do {
		ASTOBJ_RDLOCK(iterator);
		if (iterator->call_limit)
			snprintf(ilimits, sizeof(ilimits), "%d", iterator->call_limit);
		else 
			ast_copy_string(ilimits, "N/A", sizeof(ilimits));
		snprintf(iused, sizeof(iused), "%d", iterator->inUse);
		if (showall || iterator->call_limit)
			ast_cli(fd, FORMAT2, iterator->name, iused, ilimits);
		ASTOBJ_UNLOCK(iterator);
	} while (0) );

	ast_cli(fd, FORMAT, "* Peer name", "In use", "Limit");

	ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
		ASTOBJ_RDLOCK(iterator);
		if (iterator->call_limit)
			snprintf(ilimits, sizeof(ilimits), "%d", iterator->call_limit);
		else 
			ast_copy_string(ilimits, "N/A", sizeof(ilimits));
		snprintf(iused, sizeof(iused), "%d", iterator->inUse);
		if (showall || iterator->call_limit)
			ast_cli(fd, FORMAT2, iterator->name, iused, ilimits);
		ASTOBJ_UNLOCK(iterator);
	} while (0) );

	return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief  nat2str: Convert NAT setting to text string */
static char *nat2str(int nat)
{
	switch(nat) {
	case SIP_NAT_NEVER:
		return "No";
	case SIP_NAT_ROUTE:
		return "Route";
	case SIP_NAT_ALWAYS:
		return "Always";
	case SIP_NAT_RFC3581:
		return "RFC3581";
	default:
		return "Unknown";
	}
}

/* Session-Timer Modes */
static const char *stmode2str(enum st_mode m)
{
	switch(m) {
	case SESSION_TIMER_MODE_ACCEPT:
		return "Accept";
	case SESSION_TIMER_MODE_ORIGINATE:
		return "Originate";
	case SESSION_TIMER_MODE_REFUSE:
		return "Refuse";
	default:
		return "Unknown";
	}
}

static enum st_mode str2stmode(const char *s)
{
	if (!strcasecmp(s, "Accept"))
		return SESSION_TIMER_MODE_ACCEPT;
	else if (!strcasecmp(s, "Originate"))
		return SESSION_TIMER_MODE_ORIGINATE;
	else if (!strcasecmp(s, "Refuse"))
		return SESSION_TIMER_MODE_REFUSE;
	else
		return -1;
}

/* Session-Timer Refreshers */
static const char *strefresher2str(enum st_refresher r)
{
	switch(r) {
	case SESSION_TIMER_REFRESHER_AUTO:
		return "auto";
	case SESSION_TIMER_REFRESHER_LOCAL:
		return "local";
	case SESSION_TIMER_REFRESHER_REMOTE:
		return "remote";
	default:
		return "Unknown";
	}
}

static enum st_refresher str2strefresher(const char *s)
{
	if (!strcasecmp(s, "auto"))
		return SESSION_TIMER_REFRESHER_AUTO;
	else if (!strcasecmp(s, "local"))
		return SESSION_TIMER_REFRESHER_LOCAL;
	else if (!strcasecmp(s, "remote"))
		return SESSION_TIMER_REFRESHER_REMOTE;
	else
		return -1;
}

static const char *strefresher2header(enum st_refresher r, int uas)
{
	if (r == SESSION_TIMER_REFRESHER_REMOTE)
		return uas ? "uac" : "uas";
	else
		return uas ? "uas" : "uac";
}

/*! \brief  peer_status: Report Peer status in character string */
/* 	returns 1 if peer is online, -1 if unmonitored */
static int peer_status(struct sip_peer *peer, char *status, int statuslen)
{
	int res = 0;
	if (peer->maxms) {
		if (peer->lastms < 0) {
			ast_copy_string(status, "UNREACHABLE", statuslen);
		} else if (peer->lastms > peer->maxms) {
			snprintf(status, statuslen, "LAGGED (%d ms)", peer->lastms);
			res = 1;
		} else if (peer->lastms) {
			snprintf(status, statuslen, "OK (%d ms)", peer->lastms);
			res = 1;
		} else {
			ast_copy_string(status, "UNKNOWN", statuslen);
		}
	} else { 
		ast_copy_string(status, "Unmonitored", statuslen);
		/* Checking if port is 0 */
		res = -1;
	}
	return res;
}
                           
/*! \brief  sip_show_users: CLI Command 'SIP Show Users' ---*/
static int sip_show_users(int fd, int argc, char *argv[])
{
	regex_t regexbuf;
	int havepattern = 0;

#define FORMAT  "%-25.25s  %-15.15s  %-15.15s  %-15.15s  %-5.5s%-10.10s\n"

	switch (argc) {
	case 5:
		if (!strcasecmp(argv[3], "like")) {
			if (regcomp(&regexbuf, argv[4], REG_EXTENDED | REG_NOSUB))
				return RESULT_SHOWUSAGE;
			havepattern = 1;
		} else
			return RESULT_SHOWUSAGE;
	case 3:
		break;
	default:
		return RESULT_SHOWUSAGE;
	}

	ast_cli(fd, FORMAT, "Username", "Secret", "Accountcode", "Def.Context", "ACL", "NAT");
	ASTOBJ_CONTAINER_TRAVERSE(&userl, 1, do {
		ASTOBJ_RDLOCK(iterator);

		if (havepattern && regexec(&regexbuf, iterator->name, 0, NULL, 0)) {
			ASTOBJ_UNLOCK(iterator);
			continue;
		}

		ast_cli(fd, FORMAT, iterator->name, 
			iterator->secret, 
			iterator->accountcode,
			iterator->context,
			iterator->ha ? "Yes" : "No",
			nat2str(ast_test_flag(iterator, SIP_NAT)));
		ASTOBJ_UNLOCK(iterator);
	} while (0)
	);

	if (havepattern)
		regfree(&regexbuf);

	return RESULT_SUCCESS;
#undef FORMAT
}

static char mandescr_show_peers[] = 
"Description: Lists SIP peers in text format with details on current status.\n"
"Variables: \n"
"  ActionID: <id>	Action ID for this transaction. Will be returned.\n";

static int _sip_show_peers(int fd, int *total, struct mansession *s, struct message *m, int argc, char *argv[]);

/*! \brief  manager_sip_show_peers: Show SIP peers in the manager API ---*/
/*    Inspired from chan_iax2 */
static int manager_sip_show_peers( struct mansession *s, struct message *m )
{
	char *id = astman_get_header(m,"ActionID");
	char *a[] = { "sip", "show", "peers" };
	char idtext[256] = "";
	int total = 0;

	if (!ast_strlen_zero(id))
		snprintf(idtext,256,"ActionID: %s\r\n",id);

	astman_send_ack(s, m, "Peer status list will follow");
	/* List the peers in separate manager events */
	_sip_show_peers(s->fd, &total, s, m, 3, a);
	/* Send final confirmation */
	ast_cli(s->fd,
	"Event: PeerlistComplete\r\n"
	"ListItems: %d\r\n"
	"%s"
	"\r\n", total, idtext);
	return 0;
}

static int manager_sip_get_stats(struct mansession *s, struct message *m)
{
	rtp_stats_t result = {};
	char *peername = astman_get_header(m, "Peer");
	struct sip_pvt *p;
	char msg[256] = "";
	struct sip_peer *peer = find_peer(peername, NULL, 0);

	if (!peer)
	{
		astman_send_error(s, m, "Peer not found");
		return 0;
	}

	result = peer->rtp_stats;

	ast_mutex_lock(&iflock);
	for (p = iflist; p; p = p->next)
	{
		if (!strcmp(p->username, peer->username) && p->rtp)
			rtp_stats_accumulate(&result, p->rtp);
	}
	ast_mutex_unlock(&iflock);

	snprintf(msg, sizeof(msg), "\r\n"
		"RxPackets: %d\r\n"
		"TxPackets: %d\r\n"
		"RxOctets: %d\r\n"
		"TxOctets: %d\r\n"
		"RxPacketsLost: %d\r\n",
		result.rx_packets, result.tx_packets, result.rx_octets,
		result.tx_octets, result.rx_packets_lost);

	astman_send_ack(s, m, msg);	

	ASTOBJ_UNREF(peer, sip_destroy_peer);

	return 0;
}

static int manager_sip_reset_stats(struct mansession *s, struct message *m)
{
	char *peername = astman_get_header(m, "Peer");
	struct sip_peer *peer = find_peer(peername, NULL, 0);

	if (!peer)
	{
		astman_send_error(s, m, "Peer not found");
		return 0;
	}
	
	memset(&peer->rtp_stats,0, sizeof(rtp_stats_t));
        peer->last_down_timestamp = 0;
        peer->total_down_time = 0;

	astman_send_ack(s, m, "Statistics deleted");

	ASTOBJ_UNREF(peer, sip_destroy_peer);
	return 0;
}

/*! \brief  sip_show_peers: CLI Show Peers command */
static int sip_show_peers(int fd, int argc, char *argv[])
{
	return _sip_show_peers(fd, NULL, NULL, NULL, argc, argv);
}

/*! \brief  _sip_show_peers: Execute sip show peers command */
static int _sip_show_peers(int fd, int *total, struct mansession *s, struct message *m, int argc, char *argv[])
{
	regex_t regexbuf;
	int havepattern = 0;

#define FORMAT2 "%-25.25s  %-15.15s %-3.3s %-3.3s %-3.3s %-8s %-12s %-10s\n"
#define FORMAT  "%-25.25s  %-15.15s %-3.3s %-3.3s %-3.3s %-8d %-12s %-10s\n"

	char name[256];
	char iabuf[INET_ADDRSTRLEN];
	int total_peers = 0;
	int peers_online = 0;
	int peers_offline = 0;
	char *id;
	char idtext[256] = "";

	if (s) {	/* Manager - get ActionID */
		id = astman_get_header(m,"ActionID");
		if (!ast_strlen_zero(id))
			snprintf(idtext,256,"ActionID: %s\r\n",id);
	}

	switch (argc) {
	case 5:
		if (!strcasecmp(argv[3], "like")) {
			if (regcomp(&regexbuf, argv[4], REG_EXTENDED | REG_NOSUB))
				return RESULT_SHOWUSAGE;
			havepattern = 1;
		} else
			return RESULT_SHOWUSAGE;
	case 3:
		break;
	default:
		return RESULT_SHOWUSAGE;
	}

	if (!s) { /* Normal list */
		ast_cli(fd, FORMAT2, "Name/username", "Host", "Dyn", "Nat", "ACL", "Port", "Status", "Template");
	} 
	
	ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
		char status[20] = "";
		char srch[2000];
		char pstatus;
		char templ[10] = "";
		
		ASTOBJ_RDLOCK(iterator);

		if (havepattern && regexec(&regexbuf, iterator->name, 0, NULL, 0)) {
			ASTOBJ_UNLOCK(iterator);
			continue;
		}

		if (!ast_strlen_zero(iterator->username) && !s)
			snprintf(name, sizeof(name), "%s/%s", iterator->name, iterator->username);
		else
			ast_copy_string(name, iterator->name, sizeof(name));

		pstatus = peer_status(iterator, status, sizeof(status));
		if (pstatus) 	
			peers_online++;
		else	{
			if (pstatus == 0)
				peers_offline++;
			else {	/* Unmonitored */
				/* Checking if port is 0 */
				if ( ntohs(iterator->addr.sin_port) == 0 ) {
					peers_offline++;
				} else {
					peers_online++;
				}
			}
		}			

		if (ast_test_flag(&(iterator->flags_page2), SIP_PAGE2_FROM_TEMPLATE))
			strcpy(templ, "FromTempl");
		else if (ast_test_flag(&(iterator->flags_page2), SIP_PAGE2_TEMPLATE))
			strcpy(templ, "Templ");

		snprintf(srch, sizeof(srch), FORMAT, name,
			iterator->addr.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), iterator->addr.sin_addr) : "(Unspecified)",
			ast_test_flag(iterator, SIP_DYNAMIC) ? " D " : "   ", 	/* Dynamic or not? */
			(ast_test_flag(iterator, SIP_NAT) & SIP_NAT_ROUTE) ? " N " : "   ",	/* NAT=yes? */
			iterator->ha ? " A " : "   ", 	/* permit/deny */
			ntohs(iterator->addr.sin_port), status, templ);

		if (!s)  {/* Normal CLI list */
			ast_cli(fd, FORMAT, name, 
			iterator->addr.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), iterator->addr.sin_addr) : "(Unspecified)",
			ast_test_flag(iterator, SIP_DYNAMIC) ? " D " : "   ",  /* Dynamic or not? */
			(ast_test_flag(iterator, SIP_NAT) & SIP_NAT_ROUTE) ? " N " : "   ",	/* NAT=yes? */
			iterator->ha ? " A " : "   ",       /* permit/deny */
			
			ntohs(iterator->addr.sin_port), status, templ);
		} else {	/* Manager format */
			/* The names here need to be the same as other channels */
			ast_cli(fd, 
			"Event: PeerEntry\r\n%s"
			"Channeltype: SIP\r\n"
			"ObjectName: %s\r\n"
			"ChanObjectType: peer\r\n"	/* "peer" or "user" */
			"IPaddress: %s\r\n"
			"IPport: %d\r\n"
			"Dynamic: %s\r\n"
			"Natsupport: %s\r\n"
			"ACL: %s\r\n"
			"Status: %s\r\n"
			"Template: %s\r\n\r\n",
			idtext,
			iterator->name, 
			iterator->addr.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), iterator->addr.sin_addr) : "-none-",
			ntohs(iterator->addr.sin_port), 
			ast_test_flag(iterator, SIP_DYNAMIC) ? "yes" : "no",  /* Dynamic or not? */
			(ast_test_flag(iterator, SIP_NAT) & SIP_NAT_ROUTE) ? "yes" : "no",	/* NAT=yes? */
			iterator->ha ? "yes" : "no",       /* permit/deny */
			status,
			templ);
		}

		ASTOBJ_UNLOCK(iterator);

		total_peers++;
	} while(0) );

	if (!s) {
		ast_cli(fd,"%d sip peers [%d online , %d offline]\n",total_peers,peers_online,peers_offline);
	}

	if (havepattern)
		regfree(&regexbuf);

	if (total)
		*total = total_peers;
	

	return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief  sip_show_objects: List all allocated SIP Objects ---*/
static int sip_show_objects(int fd, int argc, char *argv[])
{
	char tmp[256];
	if (argc != 3)
		return RESULT_SHOWUSAGE;
	ast_cli(fd, "-= User objects: %d static, %d realtime =-\n\n", suserobjs, ruserobjs);
	ASTOBJ_CONTAINER_DUMP(fd, tmp, sizeof(tmp), &userl);
	ast_cli(fd, "-= Peer objects: %d static, %d realtime, %d autocreate =-\n\n", speerobjs, rpeerobjs, apeerobjs);
	ASTOBJ_CONTAINER_DUMP(fd, tmp, sizeof(tmp), &peerl);
	ast_cli(fd, "-= Registry objects: %d =-\n\n", regobjs);
	ASTOBJ_CONTAINER_DUMP(fd, tmp, sizeof(tmp), &regl);
	return RESULT_SUCCESS;
}
/*! \brief  print_group: Print call group and pickup group ---*/
static void  print_group(int fd, unsigned int group, int crlf) 
{
	char buf[256];
	ast_cli(fd, crlf ? "%s\r\n" : "%s\n", ast_print_group(buf, sizeof(buf), group) );
}

/*! \brief  dtmfmode2str: Convert DTMF mode to printable string ---*/
static const char *dtmfmode2str(int mode)
{
	switch (mode) {
	case SIP_DTMF_RFC2833:
		return "rfc2833";
	case SIP_DTMF_INFO:
		return "info";
	case SIP_DTMF_INBAND:
		return "inband";
	case SIP_DTMF_AUTO:
		return "auto";
	}
	return "<error>";
}

/*! \brief  insecure2str: Convert Insecure setting to printable string ---*/
static const char *insecure2str(int port, int invite)
{
	if (port && invite)
		return "port,invite";
	else if (port)
		return "port";
	else if (invite)
		return "invite";
	else
		return "no";
}

/*! \brief  sip_prune_realtime: Remove temporary realtime objects from memory (CLI) ---*/
static int sip_prune_realtime(int fd, int argc, char *argv[])
{
	struct sip_peer *peer;
	struct sip_user *user;
	int pruneuser = 0;
	int prunepeer = 0;
	int multi = 0;
	char *name = NULL;
	regex_t regexbuf;

	switch (argc) {
	case 4:
		if (!strcasecmp(argv[3], "user"))
			return RESULT_SHOWUSAGE;
		if (!strcasecmp(argv[3], "peer"))
			return RESULT_SHOWUSAGE;
		if (!strcasecmp(argv[3], "like"))
			return RESULT_SHOWUSAGE;
		if (!strcasecmp(argv[3], "all")) {
			multi = 1;
			pruneuser = prunepeer = 1;
		} else {
			pruneuser = prunepeer = 1;
			name = argv[3];
		}
		break;
	case 5:
		if (!strcasecmp(argv[4], "like"))
			return RESULT_SHOWUSAGE;
		if (!strcasecmp(argv[3], "all"))
			return RESULT_SHOWUSAGE;
		if (!strcasecmp(argv[3], "like")) {
			multi = 1;
			name = argv[4];
			pruneuser = prunepeer = 1;
		} else if (!strcasecmp(argv[3], "user")) {
			pruneuser = 1;
			if (!strcasecmp(argv[4], "all"))
				multi = 1;
			else
				name = argv[4];
		} else if (!strcasecmp(argv[3], "peer")) {
			prunepeer = 1;
			if (!strcasecmp(argv[4], "all"))
				multi = 1;
			else
				name = argv[4];
		} else
			return RESULT_SHOWUSAGE;
		break;
	case 6:
		if (strcasecmp(argv[4], "like"))
			return RESULT_SHOWUSAGE;
		if (!strcasecmp(argv[3], "user")) {
			pruneuser = 1;
			name = argv[5];
		} else if (!strcasecmp(argv[3], "peer")) {
			prunepeer = 1;
			name = argv[5];
		} else
			return RESULT_SHOWUSAGE;
		break;
	default:
		return RESULT_SHOWUSAGE;
	}

	if (multi && name) {
		if (regcomp(&regexbuf, name, REG_EXTENDED | REG_NOSUB))
			return RESULT_SHOWUSAGE;
	}

	if (multi) {
		if (prunepeer) {
			int pruned = 0;

			ASTOBJ_CONTAINER_WRLOCK(&peerl);
			ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
				ASTOBJ_RDLOCK(iterator);
				if (name && regexec(&regexbuf, iterator->name, 0, NULL, 0)) {
					ASTOBJ_UNLOCK(iterator);
					continue;
				};
				if (ast_test_flag((&iterator->flags_page2), SIP_PAGE2_RTCACHEFRIENDS)) {
					ASTOBJ_MARK(iterator);
					pruned++;
				}
				ASTOBJ_UNLOCK(iterator);
			} while (0) );
			if (pruned) {
				ASTOBJ_CONTAINER_PRUNE_MARKED(&peerl, sip_destroy_peer);
				ast_cli(fd, "%d peers pruned.\n", pruned);
			} else
				ast_cli(fd, "No peers found to prune.\n");
			ASTOBJ_CONTAINER_UNLOCK(&peerl);
		}
		if (pruneuser) {
			int pruned = 0;

			ASTOBJ_CONTAINER_WRLOCK(&userl);
			ASTOBJ_CONTAINER_TRAVERSE(&userl, 1, do {
				ASTOBJ_RDLOCK(iterator);
				if (name && regexec(&regexbuf, iterator->name, 0, NULL, 0)) {
					ASTOBJ_UNLOCK(iterator);
					continue;
				};
				if (ast_test_flag((&iterator->flags_page2), SIP_PAGE2_RTCACHEFRIENDS)) {
					ASTOBJ_MARK(iterator);
					pruned++;
				}
				ASTOBJ_UNLOCK(iterator);
			} while (0) );
			if (pruned) {
				ASTOBJ_CONTAINER_PRUNE_MARKED(&userl, sip_destroy_user);
				ast_cli(fd, "%d users pruned.\n", pruned);
			} else
				ast_cli(fd, "No users found to prune.\n");
			ASTOBJ_CONTAINER_UNLOCK(&userl);
		}
	} else {
		if (prunepeer) {
			if ((peer = ASTOBJ_CONTAINER_FIND_UNLINK(&peerl, name))) {
				if (!ast_test_flag((&peer->flags_page2), SIP_PAGE2_RTCACHEFRIENDS)) {
					ast_cli(fd, "Peer '%s' is not a Realtime peer, cannot be pruned.\n", name);
					ASTOBJ_CONTAINER_LINK(&peerl, peer);
				} else
					ast_cli(fd, "Peer '%s' pruned.\n", name);
				ASTOBJ_UNREF(peer, sip_destroy_peer);
			} else
				ast_cli(fd, "Peer '%s' not found.\n", name);
		}
		if (pruneuser) {
			if ((user = ASTOBJ_CONTAINER_FIND_UNLINK(&userl, name))) {
				if (!ast_test_flag((&user->flags_page2), SIP_PAGE2_RTCACHEFRIENDS)) {
					ast_cli(fd, "User '%s' is not a Realtime user, cannot be pruned.\n", name);
					ASTOBJ_CONTAINER_LINK(&userl, user);
				} else
					ast_cli(fd, "User '%s' pruned.\n", name);
				ASTOBJ_UNREF(user, sip_destroy_user);
			} else
				ast_cli(fd, "User '%s' not found.\n", name);
		}
	}

	return RESULT_SUCCESS;
}

/*! \brief  print_codec_to_cli: Print codec list from preference to CLI/manager */
static void print_codec_to_cli(int fd, struct ast_codec_pref *pref) 
{
    	char buf[512];
	ast_codec_pref_dump(buf, sizeof(buf), pref);
	ast_cli(fd, "%s", buf);
}

static const char *domain_mode_to_text(const enum domain_mode mode)
{
	switch (mode) {
	case SIP_DOMAIN_AUTO:
		return "[Automatic]";
	case SIP_DOMAIN_CONFIG:
		return "[Configured]";
	}

	return "";
}

/*! \brief  sip_show_domains: CLI command to list local domains */
#define FORMAT "%-40.40s %-20.20s %-16.16s\n"
static int sip_show_domains(int fd, int argc, char *argv[])
{
	struct domain *d;

	if (AST_LIST_EMPTY(&domain_list)) {
		ast_cli(fd, "SIP Domain support not enabled.\n\n");
		return RESULT_SUCCESS;
	} else {
		ast_cli(fd, FORMAT, "Our local SIP domains:", "Context", "Set by");
		AST_LIST_LOCK(&domain_list);
		AST_LIST_TRAVERSE(&domain_list, d, list)
			ast_cli(fd, FORMAT, d->domain, ast_strlen_zero(d->context) ? "(default)": d->context,
				domain_mode_to_text(d->mode));
		AST_LIST_UNLOCK(&domain_list);
		ast_cli(fd, "\n");
		return RESULT_SUCCESS;
	}
}
#undef FORMAT

static char mandescr_show_peer[] = 
"Description: Show one SIP peer with details on current status.\n"
"  The XML format is under development, feedback welcome! /oej\n"
"Variables: \n"
"  Peer: <name>           The peer name you want to check.\n"
"  ActionID: <id>	  Optional action ID for this AMI transaction.\n";

static int _sip_show_peer(int type, int fd, struct mansession *s, struct message *m, int argc, char *argv[]);

/*! \brief  manager_sip_show_peer: Show SIP peers in the manager API  ---*/
static int manager_sip_show_peer( struct mansession *s, struct message *m )
{
	char *id = astman_get_header(m,"ActionID");
	char *a[4];
	char *peer;
	int ret;

	peer = astman_get_header(m,"Peer");
	if (ast_strlen_zero(peer)) {
		astman_send_error(s, m, "Peer: <name> missing.\n");
		return 0;
	}
	a[0] = "sip";
	a[1] = "show";
	a[2] = "peer";
	a[3] = peer;

	if (!ast_strlen_zero(id))
		ast_cli(s->fd, "ActionID: %s\r\n",id);
	ret = _sip_show_peer(1, s->fd, s, m, 4, a );
	ast_cli( s->fd, "\r\n\r\n" );
	return ret;
}

static int manager_sip_send_message( struct mansession *s, struct message *m )
{
	int len;
	char *buf;

	char *peername = astman_get_header(m, "Peer");
	char *smsc = astman_get_header(m, "SMSC");
	char *ip_sm_gw = astman_get_header(m, "IP-SM-GW");
	char *content_type = astman_get_header(m, "Content-Type");
	char *content = astman_get_header(m, "Content");

	if (!peername || !(smsc || ip_sm_gw) || !content_type || !content)
	{
		astman_send_error(s, m, "Error in parameters");
		return -1;
	}

	len = strlen(content);
	buf = calloc(1, len/2);

	len = ast_hexdecode(content, buf, len/2);
	sip_sendmessage(peername, smsc, ip_sm_gw, content_type, buf, len);
	free(buf);

	astman_send_ack(s, m, "sip message");
	return 0;
}

static int manager_sip_get_time_since_last_answer(struct mansession *s,
    struct message *m)
{
        char buff[20];
	u32 i = -1;

	if (last_answered_call_timestamp)
	    i = get_time_from_boot() - last_answered_call_timestamp;

	snprintf(buff, sizeof(buff), "%u", i);
        astman_send_response(s, m, buff, NULL);
	return 0;
}

static int manager_sip_get_time_since_last_rtp(struct mansession *s,
    struct message *m)
{
        char buff[20];

	snprintf(buff, sizeof(buff), "%u", ast_rtp_get_time_since_last());
        astman_send_response(s, m, buff, NULL);
	return 0;
}

static int manager_sip_is_server_na(struct mansession *s, struct message *m)
{
        char buff[20];
	int is_na = 0;
	
	ASTOBJ_CONTAINER_TRAVERSE(&regl, !is_na, do {
		ASTOBJ_RDLOCK(iterator);
		is_na = iterator->server_na;
		ASTOBJ_UNLOCK(iterator);
	} while(0));

	snprintf(buff, sizeof(buff), "%u", is_na);
        astman_send_response(s, m, buff, NULL);
	return 0;
}

/*--- sip_fixup_codecs: try to fixup codec list
*/
static void sip_fixup_codecs(struct ast_channel *chan, const struct ast_codec_pref *peer_codecs)
{
	struct sip_pvt *p = chan->tech_pvt;
	int codecs_to_preserve;
	
	ast_mutex_lock(&p->lock);
	if (!ast_test_flag(p, SIP_OUTGOING)) {
		/* This is incoming call so its codec list is to be reordered */
		codecs_to_preserve = ast_codec_pref_bits(&chan->nativeformats);
		memcpy(&chan->nativeformats, peer_codecs, sizeof(chan->nativeformats));
	}
	else {
		codecs_to_preserve = ast_codec_pref_bits(peer_codecs);
	}	
	ast_codec_pref_remove2(&chan->nativeformats, ~codecs_to_preserve);
	memcpy(&p->formats, &chan->nativeformats, sizeof(chan->nativeformats));
	ast_codec_pref_remove2(&p->formats, ~p->usercapability);
	if (AST_STATE_UP == chan->_state) {
		transmit_reinvite_with_sdp(p, FALSE);
	}
	ast_mutex_unlock(&p->lock);
}

/*! \brief  sip_show_peer: Show one peer in detail ---*/
static int sip_show_peer(int fd, int argc, char *argv[])
{
	return _sip_show_peer(0, fd, NULL, NULL, argc, argv);
}

static int _sip_show_peer(int type, int fd, struct mansession *s, struct message *m, int argc, char *argv[])
{
	char status[30] = "";
	char cbuf[256];
	char iabuf[INET_ADDRSTRLEN];
	struct sip_peer *peer;
	char codec_buf[512];
	struct ast_codec_pref *pref;
	struct ast_variable *v;
	struct sip_auth *auth;
	int x = 0, codec = 0, load_realtime = 0;

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	load_realtime = (argc == 5 && !strcmp(argv[4], "load")) ? 1 : 0;
	peer = find_peer(argv[3], NULL, load_realtime);
	if (s) { 	/* Manager */
		if (peer)
			ast_cli(s->fd, "Response: Success\r\n");
		else {
			snprintf (cbuf, sizeof(cbuf), "Peer %s not found.\n", argv[3]);
			astman_send_error(s, m, cbuf);
			return 0;
		}
	}
	if (peer && type==0 ) { /* Normal listing */
		ast_cli(fd,"\n\n");
		ast_cli(fd, "  * Name       : %s\n", peer->name);
		ast_cli(fd, "  Secret       : %s\n", ast_strlen_zero(peer->secret)?"<Not set>":"<Set>");
		ast_cli(fd, "  MD5Secret    : %s\n", ast_strlen_zero(peer->md5secret)?"<Not set>":"<Set>");
		auth = peer->auth;
		while(auth) {
			ast_cli(fd, "  Realm-auth   : Realm %-15.15s User %-10.20s ", auth->realm, auth->username);
			ast_cli(fd, "%s\n", !ast_strlen_zero(auth->secret)?"<Secret set>":(!ast_strlen_zero(auth->md5secret)?"<MD5secret set>" : "<Not set>"));
			auth = auth->next;
		}
		ast_cli(fd, "  Context      : %s\n", peer->context);
		ast_cli(fd, "  Subscr.Cont. : %s\n", ast_strlen_zero(peer->subscribecontext)?"<Not set>":peer->subscribecontext);
		ast_cli(fd, "  Language     : %s\n", peer->language);
		if (!ast_strlen_zero(peer->accountcode))
			ast_cli(fd, "  Accountcode  : %s\n", peer->accountcode);
		ast_cli(fd, "  AMA flags    : %s\n", ast_cdr_flags2str(peer->amaflags));
		ast_cli(fd, "  CallingPres  : %s\n", ast_describe_caller_presentation(peer->callingpres));
		if (!ast_strlen_zero(peer->fromuser))
			ast_cli(fd, "  FromUser     : %s\n", peer->fromuser);
		if (!ast_strlen_zero(peer->fromdomain))
			ast_cli(fd, "  FromDomain   : %s\n", peer->fromdomain);
		ast_cli(fd, "  Callgroup    : ");
		print_group(fd, peer->callgroup, 0);
		ast_cli(fd, "  Pickupgroup  : ");
		print_group(fd, peer->pickupgroup, 0);
		ast_cli(fd, "  Mailbox      : %s\n", peer->mailbox);
		ast_cli(fd, "  RemoteMailbox: %s\n", peer->remotemailbox);
		ast_cli(fd, "  VM Extension : %s\n", peer->vmexten);
		ast_cli(fd, "  LastMsgsSent : %d\n", peer->lastmsgssent);
		ast_cli(fd, "  Call limit   : %d\n", peer->call_limit);
		ast_cli(fd, "  Dynamic      : %s\n", (ast_test_flag(peer, SIP_DYNAMIC)?"Yes":"No"));
		ast_cli(fd, "  Callerid     : %s\n", ast_callerid_merge(cbuf, sizeof(cbuf), peer->cid_name, peer->cid_num, "<unspecified>"));
		ast_cli(fd, "  Expire       : %d\n", peer->expire);
		ast_cli(fd, "  Insecure     : %s\n", insecure2str(ast_test_flag(peer, SIP_INSECURE_PORT), ast_test_flag(peer, SIP_INSECURE_INVITE)));
		ast_cli(fd, "  Nat          : %s\n", nat2str(ast_test_flag(peer, SIP_NAT)));
		ast_cli(fd, "  ACL          : %s\n", (peer->ha?"Yes":"No"));
		ast_cli(fd, "  CanReinvite  : %s\n", (ast_test_flag(peer, SIP_CAN_REINVITE)?"Yes":"No"));
		ast_cli(fd, "  PromiscRedir : %s\n", (ast_test_flag(peer, SIP_PROMISCREDIR)?"Yes":"No"));
		ast_cli(fd, "  User=Phone   : %s\n", (ast_test_flag(peer, SIP_USEREQPHONE)?"Yes":"No"));
		ast_cli(fd, "  Trust RPID   : %s\n", (ast_test_flag(peer, SIP_TRUSTRPID) ? "Yes" : "No"));
		ast_cli(fd, "  Send RPID    : %s\n", (ast_test_flag(peer, SIP_SENDRPID) ? "Yes" : "No"));

		/* - is enumerated */
		ast_cli(fd, "  DTMFmode     : %s\n", dtmfmode2str(ast_test_flag(peer, SIP_DTMF)));
		ast_cli(fd, "  LastMsg      : %d\n", peer->lastmsg);
		ast_cli(fd, "  ToHost       : %s\n", peer->tohost);
		ast_cli(fd, "  ToPort       : %d\n", peer->toport);
		ast_cli(fd, "  Addr->IP     : %s Port %d\n",  peer->addr.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), peer->addr.sin_addr) : "(Unspecified)", ntohs(peer->addr.sin_port));
		ast_cli(fd, "  Defaddr->IP  : %s Port %d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), peer->defaddr.sin_addr), ntohs(peer->defaddr.sin_port));
		ast_cli(fd, "  Def. Username: %s\n", peer->username);
		ast_cli(fd, "  SIP Options  : ");
		if (peer->sipoptions) {
			for (x=0 ; (x < (sizeof(sip_options) / sizeof(sip_options[0]))); x++) {
				if (peer->sipoptions & sip_options[x].id)
					ast_cli(fd, "%s ", sip_options[x].text);
			}
		} else
			ast_cli(fd, "(none)");

		ast_cli(fd, "\n");
		ast_cli(fd, "  Codecs       : ");
		ast_getformatname_multiple(codec_buf, sizeof(codec_buf) -1, peer->capability);
		ast_cli(fd, "%s\n", codec_buf);
		ast_cli(fd, "  Codec Order  : ");
		print_codec_to_cli(fd, &peer->prefs);
		ast_cli(fd, "\n");

		ast_cli(fd, "  Status       : ");
		peer_status(peer, status, sizeof(status));
		ast_cli(fd, "%s\n",status);
 		ast_cli(fd, "  Useragent    : %s\n", peer->useragent);
 		ast_cli(fd, "  Reg. Contact : %s\n", peer->fullcontact);
		if (peer->chanvars) {
 			ast_cli(fd, "  Variables    :\n");
			for (v = peer->chanvars ; v ; v = v->next)
 				ast_cli(fd, "                 %s = %s\n", v->name, v->value);
		}

		ast_cli(fd, "  Sess-Timers  : %s\n", stmode2str(peer->stimer.st_mode_oper));
		ast_cli(fd, "  Sess-Refresh : %s\n", strefresher2str(peer->stimer.st_ref));
		ast_cli(fd, "  Sess-Expires : %d secs\n", peer->stimer.st_max_se);
		ast_cli(fd, "  Min-Sess     : %d secs\n", peer->stimer.st_min_se);
		ast_cli(fd, "  SrvDownTime  : %d secs\n", peer->total_down_time);
		ast_cli(fd,"\n");
		ASTOBJ_UNREF(peer,sip_destroy_peer);
	} else  if (peer && type == 1) { /* manager listing */
		char *actionid = astman_get_header(m,"ActionID");

		ast_cli(fd, "Channeltype: SIP\r\n");
		if (actionid)
			ast_cli(fd, "ActionID: %s\r\n", actionid);
		ast_cli(fd, "ObjectName: %s\r\n", peer->name);
		ast_cli(fd, "ChanObjectType: peer\r\n");
		ast_cli(fd, "SecretExist: %s\r\n", ast_strlen_zero(peer->secret)?"N":"Y");
		ast_cli(fd, "MD5SecretExist: %s\r\n", ast_strlen_zero(peer->md5secret)?"N":"Y");
		ast_cli(fd, "Context: %s\r\n", peer->context);
		ast_cli(fd, "Language: %s\r\n", peer->language);
		if (!ast_strlen_zero(peer->accountcode))
			ast_cli(fd, "Accountcode: %s\r\n", peer->accountcode);
		ast_cli(fd, "AMAflags: %s\r\n", ast_cdr_flags2str(peer->amaflags));
		ast_cli(fd, "CID-CallingPres: %s\r\n", ast_describe_caller_presentation(peer->callingpres));
		if (!ast_strlen_zero(peer->fromuser))
			ast_cli(fd, "SIP-FromUser: %s\r\n", peer->fromuser);
		if (!ast_strlen_zero(peer->fromdomain))
			ast_cli(fd, "SIP-FromDomain: %s\r\n", peer->fromdomain);
		ast_cli(fd, "Callgroup: ");
		print_group(fd, peer->callgroup, 1);
		ast_cli(fd, "Pickupgroup: ");
		print_group(fd, peer->pickupgroup, 1);
		ast_cli(fd, "VoiceMailbox: %s\r\n", peer->mailbox);
		ast_cli(fd, "RemoteVoiceMailbox: %s\r\n", peer->remotemailbox);
		ast_cli(fd, "LastMsgsSent: %d\r\n", peer->lastmsgssent);
		ast_cli(fd, "Call limit: %d\r\n", peer->call_limit);
		ast_cli(fd, "Dynamic: %s\r\n", (ast_test_flag(peer, SIP_DYNAMIC)?"Y":"N"));
		ast_cli(fd, "Callerid: %s\r\n", ast_callerid_merge(cbuf, sizeof(cbuf), peer->cid_name, peer->cid_num, ""));
		ast_cli(fd, "RegExpire: %ld seconds\r\n", ast_sched_when(sched,peer->expire));
		ast_cli(fd, "SIP-AuthInsecure: %s\r\n", insecure2str(ast_test_flag(peer, SIP_INSECURE_PORT), ast_test_flag(peer, SIP_INSECURE_INVITE)));
		ast_cli(fd, "SIP-NatSupport: %s\r\n", nat2str(ast_test_flag(peer, SIP_NAT)));
		ast_cli(fd, "ACL: %s\r\n", (peer->ha?"Y":"N"));
		ast_cli(fd, "SIP-CanReinvite: %s\r\n", (ast_test_flag(peer, SIP_CAN_REINVITE)?"Y":"N"));
		ast_cli(fd, "SIP-PromiscRedir: %s\r\n", (ast_test_flag(peer, SIP_PROMISCREDIR)?"Y":"N"));
		ast_cli(fd, "SIP-UserPhone: %s\r\n", (ast_test_flag(peer, SIP_USEREQPHONE)?"Y":"N"));
		ast_cli(fd, "SIP-Sess-Timers: %s\r\n", stmode2str(peer->stimer.st_mode_oper));
		ast_cli(fd, "SIP-Sess-Refresh: %s\r\n", strefresher2str(peer->stimer.st_ref));
		ast_cli(fd, "SIP-Sess-Expires: %d\r\n", peer->stimer.st_max_se);
		ast_cli(fd, "SIP-Sess-Min: %d\r\n", peer->stimer.st_min_se);

		/* - is enumerated */
		ast_cli(fd, "SIP-DTMFmode %s\r\n", dtmfmode2str(ast_test_flag(peer, SIP_DTMF)));
		ast_cli(fd, "SIPLastMsg: %d\r\n", peer->lastmsg);
		ast_cli(fd, "ToHost: %s\r\n", peer->tohost);
		ast_cli(fd, "ToPort: %d\r\n", peer->toport);
		ast_cli(fd, "Address-IP: %s\r\nAddress-Port: %d\r\n",  peer->addr.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), peer->addr.sin_addr) : "", ntohs(peer->addr.sin_port));
		ast_cli(fd, "Default-addr-IP: %s\r\nDefault-addr-port: %d\r\n", ast_inet_ntoa(iabuf, sizeof(iabuf), peer->defaddr.sin_addr), ntohs(peer->defaddr.sin_port));
		ast_cli(fd, "Default-Username: %s\r\n", peer->username);
		ast_cli(fd, "Codecs: ");
		ast_getformatname_multiple(codec_buf, sizeof(codec_buf) -1, peer->capability);
		ast_cli(fd, "%s\r\n", codec_buf);
		ast_cli(fd, "CodecOrder: ");
		pref = &peer->prefs;
		for(x = 0; x < 32 ; x++) {
			codec = ast_codec_pref_index_audio(pref,x);
			if (!codec)
				break;
			ast_cli(fd, "%s", ast_getformatname(codec));
			if (x < 31 && ast_codec_pref_index_audio(pref,x+1))
				ast_cli(fd, ",");
		}
		for(x = 0; x < 32 ; x++) {
			codec = ast_codec_pref_index_video(pref,x);
			if (!codec)
				break;
			ast_cli(fd, "%s", ast_getformatname(codec));
			if (x < 31 && ast_codec_pref_index_video(pref,x+1))
				ast_cli(fd, ",");
		}

		ast_cli(fd, "\r\n");
		ast_cli(fd, "Status: ");
		peer_status(peer, status, sizeof(status));
		ast_cli(fd, "%s\r\n", status);
 		ast_cli(fd, "SIP-Useragent: %s\r\n", peer->useragent);
 		ast_cli(fd, "Reg-Contact : %s\r\n", peer->fullcontact);
		if (peer->chanvars) {
			for (v = peer->chanvars ; v ; v = v->next) {
 				ast_cli(fd, "ChanVariable:\n");
 				ast_cli(fd, " %s,%s\r\n", v->name, v->value);
			}
		}

		ASTOBJ_UNREF(peer,sip_destroy_peer);

	} else {
		ast_cli(fd,"Peer %s not found.\n", argv[3]);
		ast_cli(fd,"\n");
	}

	return RESULT_SUCCESS;
}

/*! \brief  sip_show_user: Show one user in detail ---*/
static int sip_show_user(int fd, int argc, char *argv[])
{
	char cbuf[256];
	struct sip_user *user;
	struct ast_variable *v;
	int load_realtime = 0;

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	/* Load from realtime storage? */
	load_realtime = (argc == 5 && !strcmp(argv[4], "load")) ? 1 : 0;

	user = find_user(argv[3], load_realtime);
	if (user) {
		ast_cli(fd,"\n\n");
		ast_cli(fd, "  * Name       : %s\n", user->name);
		ast_cli(fd, "  Secret       : %s\n", ast_strlen_zero(user->secret)?"<Not set>":"<Set>");
		ast_cli(fd, "  MD5Secret    : %s\n", ast_strlen_zero(user->md5secret)?"<Not set>":"<Set>");
		ast_cli(fd, "  Context      : %s\n", user->context);
		ast_cli(fd, "  Language     : %s\n", user->language);
		if (!ast_strlen_zero(user->accountcode))
			ast_cli(fd, "  Accountcode  : %s\n", user->accountcode);
		ast_cli(fd, "  AMA flags    : %s\n", ast_cdr_flags2str(user->amaflags));
		ast_cli(fd, "  CallingPres  : %s\n", ast_describe_caller_presentation(user->callingpres));
		ast_cli(fd, "  Call limit   : %d\n", user->call_limit);
		ast_cli(fd, "  Callgroup    : ");
		print_group(fd, user->callgroup, 0);
		ast_cli(fd, "  Pickupgroup  : ");
		print_group(fd, user->pickupgroup, 0);
		ast_cli(fd, "  Callerid     : %s\n", ast_callerid_merge(cbuf, sizeof(cbuf), user->cid_name, user->cid_num, "<unspecified>"));
		ast_cli(fd, "  ACL          : %s\n", (user->ha?"Yes":"No"));
		ast_cli(fd, "  Insecure     : %s\n", insecure2str(ast_test_flag(user, SIP_INSECURE_PORT), ast_test_flag(user, SIP_INSECURE_INVITE)));
 		ast_cli(fd, "  Sess-Timers  : %s\n", stmode2str(user->stimer.st_mode_oper));
 		ast_cli(fd, "  Sess-Refresh : %s\n", strefresher2str(user->stimer.st_ref));
 		ast_cli(fd, "  Sess-Expires : %d secs\n", user->stimer.st_max_se);
 		ast_cli(fd, "  Sess-Min-SE  : %d secs\n", user->stimer.st_min_se);

		ast_cli(fd, "  Codec Order  : ");
		print_codec_to_cli(fd, &user->prefs);
		ast_cli(fd, "\n");

		if (user->chanvars) {
 			ast_cli(fd, "  Variables    :\n");
			for (v = user->chanvars ; v ; v = v->next)
 				ast_cli(fd, "                 %s = %s\n", v->name, v->value);
		}
		ast_cli(fd,"\n");
		ASTOBJ_UNREF(user,sip_destroy_user);
	} else {
		ast_cli(fd,"User %s not found.\n", argv[3]);
		ast_cli(fd,"\n");
	}

	return RESULT_SUCCESS;
}

/* Input is intended to be: either fd==-1 or output==NULL .
 * output == NULL is for use of sip_show_registry.
 * output != NULL will result in filling output with pointer to an allocated
 * string with output of sip_show_registry (caller must free *output).
 */
static int sip_show_registry_output(char** output, int fd)
{
#define FORMAT3 "%s %s %d %s %d\n"
#define FORMAT2 "%-30.30s  %-30.30s  %8.8s %-20.20s %8.8s\n"
#define FORMAT  "%-30.30s  %-30.30s  %8d %-20.20s %8d\n"
	char host[512];
	char out[4096] = ""; 
	int len = 0;

	if (!output)
	        ast_cli(fd, FORMAT2, "Host", "Username", "Refresh", "State", "N/A");
	else
	{
	        len += snprintf(out + len, sizeof(out)-len, FORMAT2, "Host", "Username", "Refresh", "State", "N/A");
	}

	ASTOBJ_CONTAINER_TRAVERSE(&regl, 1, do {
		ASTOBJ_RDLOCK(iterator);
		snprintf(host, sizeof(host), "%s:%d", iterator->hostname, iterator->portno ? iterator->portno : DEFAULT_SIP_PORT);
		if (!output)
		        ast_cli(fd, FORMAT, host, iterator->username, iterator->refresh, regstate2str(iterator->regstate), iterator->server_na);
		else
		{
		        len += snprintf(out + len, sizeof(out)-len, FORMAT3, host, iterator->username, iterator->refresh, regstate2str(iterator->regstate),
			    iterator->server_na);
		}
		ASTOBJ_UNLOCK(iterator);
	} while(0));

        if (output)
	        *output = strdup(out);

	return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
#undef FORMAT3
}

/*! \brief find_reg_by_contact: Find a registry by contact and backup/primary */
static struct sip_registry *find_reg_by_contact(char *contact, int backup)
{
	struct sip_registry *found = NULL;
	ASTOBJ_CONTAINER_TRAVERSE(&regl, !found, do { 
		ASTOBJ_RDLOCK(iterator); 
		if (!strcmp(iterator->contact, contact) && iterator->backup==backup) {
		found = ASTOBJ_REF(iterator);
		} 
		ASTOBJ_UNLOCK(iterator); \
		} while (0));
	return found;
}

/*! \brief  sip_show_registry: Show SIP Registry (registrations with other SIP proxies ---*/
static int sip_show_registry(int fd, int argc, char *argv[])
{
	if (argc != 3)
		return RESULT_SHOWUSAGE;
        return sip_show_registry_output(NULL, fd);
}


/*! \brief  sip_show_settings: List global settings for the SIP channel ---*/
static int sip_show_settings(int fd, int argc, char *argv[])
{
	char tmp[BUFSIZ];
	int realtimepeers = 0;
	int realtimeusers = 0;

	realtimepeers = ast_check_realtime("sippeers");
	realtimeusers = ast_check_realtime("sipusers");

	if (argc != 3)
		return RESULT_SHOWUSAGE;
	ast_cli(fd, "\n\nGlobal Settings:\n");
	ast_cli(fd, "----------------\n");
	ast_cli(fd, "  SIP Port:               %d\n", ntohs(bindaddr.sin_port));
	ast_cli(fd, "  Bindaddress:            %s\n", ast_inet_ntoa(tmp, sizeof(tmp), bindaddr.sin_addr));
	ast_cli(fd, "  Videosupport:           %s\n", videosupport ? "Yes" : "No");
	ast_cli(fd, "  AutoCreatePeer:         %s\n", autocreatepeer ? "Yes" : "No");
	ast_cli(fd, "  Allow unknown access:   %s\n", global_allowguest ? "Yes" : "No");
	ast_cli(fd, "  Allow P2P calls:        %s\n", global_allow_p2p_calls ? "Yes" : "No");
	ast_cli(fd, "  Promsic. redir:         %s\n", ast_test_flag(&global_flags, SIP_PROMISCREDIR) ? "Yes" : "No");
	ast_cli(fd, "  SIP domain support:     %s\n", AST_LIST_EMPTY(&domain_list) ? "No" : "Yes");
	ast_cli(fd, "  Call to non-local dom.: %s\n", allow_external_domains ? "Yes" : "No");
	ast_cli(fd, "  URI user is phone no:   %s\n", ast_test_flag(&global_flags, SIP_USEREQPHONE) ? "Yes" : "No");
	ast_cli(fd, "  Our auth realm          %s\n", global_realm);
	ast_cli(fd, "  Realm. auth:            %s\n", authl ? "Yes": "No");
	ast_cli(fd, "  User Agent:             %s\n", default_useragent);
	ast_cli(fd, "  MWI checking interval:  %d secs\n", global_mwitime);
	ast_cli(fd, "  Reg. context:           %s\n", ast_strlen_zero(regcontext) ? "(not set)" : regcontext);
	ast_cli(fd, "  Caller ID:              %s\n", default_callerid);
	ast_cli(fd, "  From: Domain:           %s\n", default_fromdomain);
	ast_cli(fd, "  Record SIP history:     %s\n", recordhistory ? "On" : "Off");
	ast_cli(fd, "  Call Events:            %s\n", callevents ? "On" : "Off");
	ast_cli(fd, "  IP ToS:                 0x%x\n", tos);
	ast_cli(fd, "  SO_MARK:                0x%x\n", so_mark);
#ifdef OSP_SUPPORT
	ast_cli(fd, "  OSP Support:            Yes\n");
#else
	ast_cli(fd, "  OSP Support:            No\n");
#endif
	if (!realtimepeers && !realtimeusers)
		ast_cli(fd, "  SIP realtime:           Disabled\n" );
	else
		ast_cli(fd, "  SIP realtime:           Enabled\n" );
	ast_cli(fd, "  SMS over IP:            %s\n", ast_test_flag(&global_flags_page2, SIP_PAGE2_FEATURE_3GPP_SMS) ? "Yes" : "No");

	ast_cli(fd, "\nGlobal Signalling Settings:\n");
	ast_cli(fd, "---------------------------\n");
	ast_cli(fd, "  Codecs:                 ");
	print_codec_to_cli(fd, &global_prefs);
	ast_cli(fd, "\n");
	ast_cli(fd, "  High Quality Calls Max: %d\n", global_highqualitycalls.max);
        ast_cli(fd, "  High Quality Calls Cur: %d\n", global_highqualitycalls.current);
	ast_cli(fd, "  Relax DTMF:             %s\n", relaxdtmf ? "Yes" : "No");
	ast_cli(fd, "  Compact SIP headers:    %s\n", compactheaders ? "Yes" : "No");
	ast_cli(fd, "  RTP Timeout:            %d %s\n", global_rtptimeout, global_rtptimeout ? "" : "(Disabled)" );
	ast_cli(fd, "  RTP Hold Timeout:       %d %s\n", global_rtpholdtimeout, global_rtpholdtimeout ? "" : "(Disabled)");
	ast_cli(fd, "  MWI NOTIFY mime type:   %s\n", default_notifymime);
	ast_cli(fd, "  DNS SRV lookup:         %s\n", srvlookup ? "Yes" : "No");
	ast_cli(fd, "  Pedantic SIP support:   %s\n", pedanticsipchecking ? "Yes" : "No");
	ast_cli(fd, "  Reg. max duration:      %d secs\n", max_expiry);
	ast_cli(fd, "  Reg. default duration:  %d secs\n", default_expiry);
	ast_cli(fd, "  Reg. default period:    %d secs\n", default_reg_period);
	ast_cli(fd, "  Reg. default space:    %d secs\n", default_regspacing);
	ast_cli(fd, "  Outbound reg. timeout:  %d secs\n", global_reg_timeout);
	ast_cli(fd, "  Outbound reg. attempts: %d\n", global_regattempts_max);
	ast_cli(fd, "  Notify ringing state:   %s\n", global_notifyringing ? "Yes" : "No");
	ast_cli(fd, "  Session Timers:         %s\n", stmode2str(global_st_mode));
	ast_cli(fd, "  Session Refresher:      %s\n", strefresher2str (global_st_refresher));
	ast_cli(fd, "  Session Expires:        %d secs\n", global_max_se);
	ast_cli(fd, "  Session Min-SE:         %d secs\n", global_min_se);

	ast_cli(fd, "\nDefault Settings:\n");
	ast_cli(fd, "-----------------\n");
	ast_cli(fd, "  Context:                %s\n", default_context);
	ast_cli(fd, "  Nat:                    %s\n", nat2str(ast_test_flag(&global_flags, SIP_NAT)));
	ast_cli(fd, "  DTMF:                   %s\n", dtmfmode2str(ast_test_flag(&global_flags, SIP_DTMF)));
	ast_cli(fd, "  Qualify:                %d\n", default_qualify);
	ast_cli(fd, "  Use ClientCode:         %s\n", ast_test_flag(&global_flags, SIP_USECLIENTCODE) ? "Yes" : "No");
	ast_cli(fd, "  Progress inband:        %s\n", (ast_test_flag(&global_flags, SIP_PROG_INBAND) == SIP_PROG_INBAND_NEVER) ? "Never" : (ast_test_flag(&global_flags, SIP_PROG_INBAND) == SIP_PROG_INBAND_NO) ? "No" : "Yes" );
	ast_cli(fd, "  Language:               %s\n", ast_strlen_zero(default_language) ? "(Defaults to English)" : default_language);
	ast_cli(fd, "  Musicclass:             %s\n", global_musicclass);
	ast_cli(fd, "  Voice Mail Extension:   %s\n", global_vmexten);

	
	if (realtimepeers || realtimeusers) {
		ast_cli(fd, "\nRealtime SIP Settings:\n");
		ast_cli(fd, "----------------------\n");
		ast_cli(fd, "  Realtime Peers:         %s\n", realtimepeers ? "Yes" : "No");
		ast_cli(fd, "  Realtime Users:         %s\n", realtimeusers ? "Yes" : "No");
		ast_cli(fd, "  Cache Friends:          %s\n", ast_test_flag(&global_flags_page2, SIP_PAGE2_RTCACHEFRIENDS) ? "Yes" : "No");
		ast_cli(fd, "  Update:                 %s\n", ast_test_flag(&global_flags_page2, SIP_PAGE2_RTUPDATE) ? "Yes" : "No");
		ast_cli(fd, "  Ignore Reg. Expire:     %s\n", ast_test_flag(&global_flags_page2, SIP_PAGE2_IGNOREREGEXPIRE) ? "Yes" : "No");
		ast_cli(fd, "  Auto Clear:             %d\n", global_rtautoclear);
	}
	ast_cli(fd, "\n----\n");
	return RESULT_SUCCESS;
}

/*! \brief  subscription_type2str: Show subscription type in string format */
static const char *subscription_type2str(enum subscriptiontype subtype) {
	int i;

	for (i = 1; (i < (sizeof(subscription_types) / sizeof(subscription_types[0]))); i++) {
		if (subscription_types[i].type == subtype) {
			return subscription_types[i].text;
		}
	}
	return subscription_types[0].text;
}

/*! \brief  find_subscription_type: Find subscription type in array */
static const struct cfsubscription_types *find_subscription_type(enum subscriptiontype subtype) {
	int i;

	for (i = 1; (i < (sizeof(subscription_types) / sizeof(subscription_types[0]))); i++) {
		if (subscription_types[i].type == subtype) {
			return &subscription_types[i];
		}
	}
	return &subscription_types[0];
}

/* Forward declaration */
static int __sip_show_channels(int fd, int argc, char *argv[], int subscriptions);

/*! \brief  sip_show_channels: Show active SIP channels ---*/
static int sip_show_channels(int fd, int argc, char *argv[])  
{
        return __sip_show_channels(fd, argc, argv, 0);
}
 
/*! \brief  sip_show_subscriptions: Show active SIP subscriptions ---*/
static int sip_show_subscriptions(int fd, int argc, char *argv[])
{
        return __sip_show_channels(fd, argc, argv, 1);
}

static int __sip_show_channels(int fd, int argc, char *argv[], int subscriptions)
{
#define FORMAT3 "%-15.15s  %-10.10s  %-11.11s  %-15.15s  %-13.13s  %-15.15s\n"
#define FORMAT2 "%-15.15s  %-10.10s  %-11.11s  %-11.11s  %-4.4s  %-7.7s  %-15.15s %-6.6s\n"
#define FORMAT  "%-15.15s  %-10.10s  %-11.11s  %5.5d/%5.5d  %-4.4s  %-3.3s %-3.3s  %-15.15s %-6d\n"
	struct sip_pvt *cur;
	char iabuf[INET_ADDRSTRLEN];
	int numchans = 0;
	if (argc != 3)
		return RESULT_SHOWUSAGE;
	ast_mutex_lock(&iflock);
	cur = iflist;
	if (!subscriptions)
		ast_cli(fd, FORMAT2, "Peer", "User/ANR", "Call ID", "Seq (Tx/Rx)", "Format", "Hold", "Last Message", "HighQ");
	else
		ast_cli(fd, FORMAT3, "Peer", "User", "Call ID", "Extension", "Last state", "Type");
	while (cur) {
		if (cur->subscribed == NONE && !subscriptions) {
			ast_cli(fd, FORMAT, ast_inet_ntoa(iabuf, sizeof(iabuf), cur->sa.sin_addr), 
				ast_strlen_zero(cur->username) ? ( ast_strlen_zero(cur->cid_num) ? "(None)" : cur->cid_num ) : cur->username, 
				cur->callid, 
				cur->ocseq, cur->icseq, 
				ast_getformatname(cur->owner ? ast_codec_pref_index_audio(&cur->owner->nativeformats, 0) : 0), 
				ast_test_flag(cur, SIP_CALL_ONHOLD) ? "Yes" : "No",
				ast_test_flag(cur, SIP_NEEDDESTROY) ? "(d)" : "",
				cur->lastmsg, cur->quality_meter);
			numchans++;
		}
		if (cur->subscribed != NONE && subscriptions) {
			ast_cli(fd, FORMAT3, ast_inet_ntoa(iabuf, sizeof(iabuf), cur->sa.sin_addr),
			        ast_strlen_zero(cur->username) ? ( ast_strlen_zero(cur->cid_num) ? "(None)" : cur->cid_num ) : cur->username, 
			        cur->callid, cur->exten, ast_extension_state2str(cur->laststate), 
			        subscription_type2str(cur->subscribed));
			numchans++;
		}
		cur = cur->next;
	}
	ast_mutex_unlock(&iflock);
	if (!subscriptions)
		ast_cli(fd, "%d active SIP channel%s\n", numchans, (numchans != 1) ? "s" : "");
	else
		ast_cli(fd, "%d active SIP subscription%s\n", numchans, (numchans != 1) ? "s" : "");
	return RESULT_SUCCESS;
#undef FORMAT
#undef FORMAT2
#undef FORMAT3
}

/*! \brief  complete_sipch: Support routine for 'sip show channel' CLI ---*/
static char *complete_sipch(char *line, char *word, int pos, int state)
{
	int which=0;
	struct sip_pvt *cur;
	char *c = NULL;

	ast_mutex_lock(&iflock);
	cur = iflist;
	while(cur) {
		if (!strncasecmp(word, cur->callid, strlen(word))) {
			if (++which > state) {
				c = strdup(cur->callid);
				break;
			}
		}
		cur = cur->next;
	}
	ast_mutex_unlock(&iflock);
	return c;
}

/*! \brief  complete_sip_peer: Do completion on peer name ---*/
static char *complete_sip_peer(char *word, int state, int flags2)
{
	char *result = NULL;
	int wordlen = strlen(word);
	int which = 0;

	ASTOBJ_CONTAINER_TRAVERSE(&peerl, !result, do {
		/* locking of the object is not required because only the name and flags are being compared */
		if (!strncasecmp(word, iterator->name, wordlen)) {
			if (flags2 && !ast_test_flag((&iterator->flags_page2), flags2))
				continue;
			if (++which > state) {
				result = strdup(iterator->name);
			}
		}
	} while(0) );
	return result;
}

/*! \brief  complete_sip_show_peer: Support routine for 'sip show peer' CLI ---*/
static char *complete_sip_show_peer(char *line, char *word, int pos, int state)
{
	if (pos == 3)
		return complete_sip_peer(word, state, 0);

	return NULL;
}

/*! \brief  complete_sip_debug_peer: Support routine for 'sip debug peer' CLI ---*/
static char *complete_sip_debug_peer(char *line, char *word, int pos, int state)
{
	if (pos == 3)
		return complete_sip_peer(word, state, 0);

	return NULL;
}

/*! \brief  complete_sip_user: Do completion on user name ---*/
static char *complete_sip_user(char *word, int state, int flags2)
{
	char *result = NULL;
	int wordlen = strlen(word);
	int which = 0;

	ASTOBJ_CONTAINER_TRAVERSE(&userl, !result, do {
		/* locking of the object is not required because only the name and flags are being compared */
		if (!strncasecmp(word, iterator->name, wordlen)) {
			if (flags2 && !ast_test_flag(&(iterator->flags_page2), flags2))
				continue;
			if (++which > state) {
				result = strdup(iterator->name);
			}
		}
	} while(0) );
	return result;
}

/*! \brief  complete_sip_show_user: Support routine for 'sip show user' CLI ---*/
static char *complete_sip_show_user(char *line, char *word, int pos, int state)
{
	if (pos == 3)
		return complete_sip_user(word, state, 0);

	return NULL;
}

/*! \brief  complete_sipnotify: Support routine for 'sip notify' CLI ---*/
static char *complete_sipnotify(char *line, char *word, int pos, int state)
{
	char *c = NULL;

	if (pos == 2) {
		int which = 0;
		char *cat;

		/* do completion for notify type */

		if (!notify_types)
			return NULL;
		
		cat = ast_category_browse(notify_types, NULL);
		while(cat) {
			if (!strncasecmp(word, cat, strlen(word))) {
				if (++which > state) {
					c = strdup(cat);
					break;
				}
			}
			cat = ast_category_browse(notify_types, cat);
		}
		return c;
	}

	if (pos > 2)
		return complete_sip_peer(word, state, 0);

	return NULL;
}

/*! \brief  complete_sip_prune_realtime_peer: Support routine for 'sip prune realtime peer' CLI ---*/
static char *complete_sip_prune_realtime_peer(char *line, char *word, int pos, int state)
{
	if (pos == 4)
		return complete_sip_peer(word, state, SIP_PAGE2_RTCACHEFRIENDS);
	return NULL;
}

/*! \brief  complete_sip_prune_realtime_user: Support routine for 'sip prune realtime user' CLI ---*/
static char *complete_sip_prune_realtime_user(char *line, char *word, int pos, int state)
{
	if (pos == 4)
		return complete_sip_user(word, state, SIP_PAGE2_RTCACHEFRIENDS);

	return NULL;
}

/*! \brief  sip_show_channel: Show details of one call ---*/
static int sip_show_channel(int fd, int argc, char *argv[])
{
	struct sip_pvt *cur;
	struct sockaddr_in sockaddr;
	char iabuf[INET_ADDRSTRLEN];
	size_t len;
	int found = 0;
	char buf[512];

	if (argc != 4)
		return RESULT_SHOWUSAGE;
	len = strlen(argv[3]);
	ast_mutex_lock(&iflock);
	cur = iflist;
	while(cur) {
		if (!strncasecmp(cur->callid, argv[3],len)) {
			ast_cli(fd,"\n");
			if (cur->subscribed != NONE)
				ast_cli(fd, "  * Subscription (type: %s)\n", subscription_type2str(cur->subscribed));
			else
				ast_cli(fd, "  * SIP Call\n");
			ast_cli(fd, "  Direction:              %s\n", ast_test_flag(cur, SIP_OUTGOING)?"Outgoing":"Incoming");
			ast_cli(fd, "  Call-ID:                %s\n", cur->callid);
			ast_cli(fd, "  Non-Codec Capability:   %d\n", cur->noncodeccapability);
			ast_cli(fd, "  Formats:                %s\n", cur->owner ? ast_codec_pref_dump(buf, sizeof(buf), &cur->owner->nativeformats) : "Unknown");
			ast_cli(fd, "  Theoretical Address:    %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), cur->sa.sin_addr), ntohs(cur->sa.sin_port));
			ast_cli(fd, "  Received Address:       %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), cur->recv.sin_addr), ntohs(cur->recv.sin_port));
			ast_cli(fd, "  NAT Support:            %s\n", nat2str(ast_test_flag(cur, SIP_NAT)));
			ast_cli(fd, "  Audio IP:               %s %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), cur->redirip.sin_addr.s_addr ? cur->redirip.sin_addr : cur->ourip), cur->redirip.sin_addr.s_addr ? "(Outside bridge)" : "(local)" );
			ast_cli(fd, "  Our Tag:                %s\n", cur->tag);
			ast_cli(fd, "  Their Tag:              %s\n", cur->theirtag);
			ast_cli(fd, "  SIP User agent:         %s\n", cur->useragent);
			if (!ast_strlen_zero(cur->username))
				ast_cli(fd, "  Username:               %s\n", cur->username);
			if (!ast_strlen_zero(cur->peername))
				ast_cli(fd, "  Peername:               %s\n", cur->peername);
			if (!ast_strlen_zero(cur->uri))
				ast_cli(fd, "  Original uri:           %s\n", cur->uri);
			if (!ast_strlen_zero(cur->cid_num))
				ast_cli(fd, "  Caller-ID:              %s\n", cur->cid_num);
			ast_cli(fd, "  Need Destroy:           %d\n", ast_test_flag(cur, SIP_NEEDDESTROY));
			ast_cli(fd, "  Last Message:           %s\n", cur->lastmsg);
			ast_cli(fd, "  Promiscuous Redir:      %s\n", ast_test_flag(cur, SIP_PROMISCREDIR) ? "Yes" : "No");
			ast_cli(fd, "  Route:                  %s\n", cur->route ? cur->route->hop : "N/A");
			ast_cli(fd, "  DTMF Mode:              %s\n", dtmfmode2str(ast_test_flag(cur, SIP_DTMF)));
			ast_cli(fd, "  SIP Options:            ");
			if (cur->sipoptions) {
				int x;
				for (x=0 ; (x < (sizeof(sip_options) / sizeof(sip_options[0]))); x++) {
					if (cur->sipoptions & sip_options[x].id)
						ast_cli(fd, "%s ", sip_options[x].text);
				}
			} else
				ast_cli(fd, "(none)");
			ast_cli(fd, "\n");
			if (cur->rtp) {
			    ast_rtp_get_peer(cur->rtp, &sockaddr);
			    ast_cli(fd, "  FarEndIPAddress:        %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), sockaddr.sin_addr));
			    ast_cli(fd, "  FarEndUDPPort:          %d\n", sockaddr.sin_port);
			    ast_rtp_get_us(cur->rtp, &sockaddr);
			    ast_cli(fd, "  LocalUDPPort:           %d\n", sockaddr.sin_port);
			    ast_cli(fd, "  Quality:                %s\n", ast_rtp_get_quality(cur->rtp));
			}
			if (cur->owner)
                        	ast_cli(fd,"  Packetization Period:   %d\n", cur->owner->ptime);
			if (!cur->stimer)
				ast_cli(fd, "  Session-Timer:          Uninitialized\n");
			else {
				ast_cli(fd, "  Session-Timer:          %s\n", cur->stimer->st_active ? "Active" : "Inactive");
 				if (cur->stimer->st_active == TRUE) {
 					ast_cli(fd, "  S-Timer Interval:       %d\n", cur->stimer->st_interval);
					ast_cli(fd, "  S-Timer Min-SE:         %d\n", cur->stimer->st_min_se);
 					ast_cli(fd, "  S-Timer Refresher:      %s\n", strefresher2str(cur->stimer->st_ref));
 					ast_cli(fd, "  S-Timer Expirys:        %d\n", cur->stimer->st_expirys);
 					ast_cli(fd, "  S-Timer Sched Id:       %d\n", cur->stimer->st_schedid);
 					ast_cli(fd, "  S-Timer Peer Sts:       %s\n", cur->stimer->st_active_peer_ua ? "Active" : "Inactive");
 					ast_cli(fd, "  S-Timer Cached Min-SE:  %d\n", cur->stimer->st_cached_min_se);
 					ast_cli(fd, "  S-Timer Cached SE:      %d\n", cur->stimer->st_cached_max_se);
 					ast_cli(fd, "  S-Timer Cached Ref:     %s\n", strefresher2str(cur->stimer->st_cached_ref));
 					ast_cli(fd, "  S-Timer Cached Mode:    %s\n", stmode2str(cur->stimer->st_cached_mode));
 				}
			}

			ast_cli(fd, "\n\n");
			found++;
		}
		cur = cur->next;
	}
	ast_mutex_unlock(&iflock);
	if (!found) 
		ast_cli(fd, "No such SIP Call ID starting with '%s'\n", argv[3]);
	return RESULT_SUCCESS;
}

/*! \brief  sip_show_history: Show history details of one call ---*/
static int sip_show_history(int fd, int argc, char *argv[])
{
	struct sip_pvt *cur;
	struct sip_history *hist;
	size_t len;
	int x;
	int found = 0;

	if (argc != 4)
		return RESULT_SHOWUSAGE;
	if (!recordhistory)
		ast_cli(fd, "\n***Note: History recording is currently DISABLED.  Use 'sip history' to ENABLE.\n");
	len = strlen(argv[3]);
	ast_mutex_lock(&iflock);
	cur = iflist;
	while(cur) {
		if (!strncasecmp(cur->callid, argv[3], len)) {
			ast_cli(fd,"\n");
			if (cur->subscribed != NONE)
				ast_cli(fd, "  * Subscription\n");
			else
				ast_cli(fd, "  * SIP Call\n");
			x = 0;
			hist = cur->history;
			while(hist) {
				x++;
				ast_cli(fd, "%d. %s\n", x, hist->event);
				hist = hist->next;
			}
			if (!x)
				ast_cli(fd, "Call '%s' has no history\n", cur->callid);
			found++;
		}
		cur = cur->next;
	}
	ast_mutex_unlock(&iflock);
	if (!found) 
		ast_cli(fd, "No such SIP Call ID starting with '%s'\n", argv[3]);
	return RESULT_SUCCESS;
}

/*! \brief  dump_history: Dump SIP history to debug log file at end of 
  lifespan for SIP dialog */
void sip_dump_history(struct sip_pvt *dialog)
{
	int x;
	struct sip_history *hist;

	if (!dialog)
		return;

	ast_log(LOG_DEBUG, "\n---------- SIP HISTORY for '%s' \n", dialog->callid);
	if (dialog->subscribed)
		ast_log(LOG_DEBUG, "  * Subscription\n");
	else
		ast_log(LOG_DEBUG, "  * SIP Call\n");
	x = 0;
	hist = dialog->history;
	while(hist) {
		x++;
		ast_log(LOG_DEBUG, "  %d. %s\n", x, hist->event);
		hist = hist->next;
	}
	if (!x)
		ast_log(LOG_DEBUG, "Call '%s' has no history\n", dialog->callid);
	ast_log(LOG_DEBUG, "\n---------- END SIP HISTORY for '%s' \n", dialog->callid);
	
}


static int sip_simulate_dtmf_end(void *data)
{
	struct sip_pvt *p = data;

	if (p) {
		ast_mutex_lock(&p->lock);
		/* Don't hold pvt lock while trying to lock the channel */
		while(p->owner && ast_mutex_trylock(&p->owner->lock)) {
			ast_mutex_unlock(&p->lock);
			usleep(1);
			ast_mutex_lock(&p->lock);
		}

		if (p->owner) {
			struct ast_frame f = {
				.frametype = AST_FRAME_DTMF_END,
				.subclass = p->curDTMF,
				.samples = 0,
			};
			ast_queue_frame(p->owner, &f);
			ast_mutex_unlock(&p->owner->lock);
		}

		p->DTMFschedid = -1;
		ast_mutex_unlock(&p->lock);
	}

	return 0;
}

/*! \brief  handle_request_info: Receive SIP INFO Message ---*/
/*    Doesn't read the duration of the DTMF signal */
static void handle_request_info(struct sip_pvt *p, struct sip_request *req)
{
	char buf[1024];
	unsigned int event;
	char *c;
	
	/* Need to check the media/type */
	if (!strcasecmp(get_header(req, "Content-Type"), "application/dtmf-relay") ||
	    !strcasecmp(get_header(req, "Content-Type"), "application/vnd.nortelnetworks.digits")) {
		unsigned int duration = 0;

		/* Try getting the "signal=" part */
		if (ast_strlen_zero(c = get_sdp(req, "Signal", '=')) && ast_strlen_zero(c = get_sdp(req, "d", '='))) {
			ast_log(LOG_WARNING, "Unable to retrieve DTMF signal from INFO message from %s\n", p->callid);
			transmit_response(p, "200 OK", req); /* Should return error */
			return;
		} else {
			ast_copy_string(buf, c, sizeof(buf));
		}

		if (!ast_strlen_zero(c = get_sdp(req, "Duration", '=')))
			duration = atoi(c);
		if (!duration)
			duration = 100; /* 100 ms */
	
		if (!p->owner) {	/* not a PBX call */
			transmit_response(p, "481 Call leg/transaction does not exist", req);
			ast_set_flag(p, SIP_NEEDDESTROY);
			return;
		}

		if (ast_strlen_zero(buf)) {
			transmit_response(p, "200 OK", req);
			return;
		}

		if (buf[0] == '*')
			event = 10;
		else if (buf[0] == '#')
			event = 11;
		else if ((buf[0] >= 'A') && (buf[0] <= 'D'))
			event = 12 + buf[0] - 'A';
		else
			event = atoi(buf);
		if (event == 16) {
			/* send a FLASH event */
			struct ast_frame f = { AST_FRAME_CONTROL, AST_CONTROL_FLASH, };
			ast_queue_frame(p->owner, &f);
			if (sipdebug)
				ast_verbose("* DTMF-relay event received: FLASH\n");
		} else {
			/* send a DTMF event */
			struct ast_frame f = { AST_FRAME_DTMF_BEGIN, };
			if (event < 10) {
				f.subclass = '0' + event;
			} else if (event < 11) {
				f.subclass = '*';
			} else if (event < 12) {
				f.subclass = '#';
			} else if (event < 16) {
				f.subclass = 'A' + (event - 12);
			}
			ast_queue_frame(p->owner, &f);
			if (p->DTMFschedid > -1)
				ast_sched_del(sched, p->DTMFschedid);
			p->curDTMF = f.subclass;
			p->DTMFschedid = ast_sched_add(sched, duration, sip_simulate_dtmf_end, p);

			if (sipdebug)
				ast_verbose("* DTMF-relay event received: %c\n", f.subclass);
		}
		transmit_response(p, "200 OK", req);
		return;
	} else if (ast_test_flag(p, SIP_COMPAT) == SIP_COMPAT_BROADSOFT &&
		!strncasecmp(get_header(req, "Content-Type"), "application/broadsoft", 21)) {
		struct ast_frame f;
		char cid[256];

		if (strncasecmp(req->line[0], "play tone CallWaitingTone", 25)
			&& strncasecmp(req->line[0], "stop CallWaitingTone", 20)) {
			ast_log(LOG_WARNING, "Unable to retrieve tone from INFO message from %s\n", p->callid);
			transmit_response(p, "200 OK", req); /* Should return error */
			return;
		}
	
		if (!p->owner) {	/* not a PBX call */
			transmit_response(p, "481 Call leg/transaction does not exist", req);
			ast_set_flag(p, SIP_NEEDDESTROY);
			return;
		}

		memset(&f, 0, sizeof(f));
		f.frametype = AST_FRAME_CALLWAITING;
		if (!strncasecmp(req->line[0], "stop", 4))
			f.subclass = AST_CALLWAITING_STOP;
		else
		{
			char *name, *num;
			char tone[strlen("Bellcore-dr1")+1];

			c = strcasestr(req->line[0], "CallWaitingTone");
			c += strlen("CallWaitingTone");
			if (*c >= '1' && *c <= '8')
			{
			    snprintf(tone, 13, "Bellcore-dr%c", *c);
			    pbx_builtin_setvar_helper(p->owner, "_ALERTINFO", tone);
			}
			else
			    pbx_builtin_setvar_helper(p->owner, "_ALERTINFO", NULL);

			f.subclass = AST_CALLWAITING_START;
			name = get_sdp(req, "Calling-Name", ':');
			num = get_sdp(req, "Calling-Number", ':');
			snprintf(cid, sizeof(cid), "%s <%s>", name, num);
			f.data = cid;
			f.datalen = sizeof(cid);
		}
		ast_queue_frame(p->owner, &f);

		transmit_response(p, "200 OK", req);
		return;
	} else if (!strcasecmp(get_header(req, "Content-Type"), "application/media_control+xml")) {
		/* Eh, we'll just assume it's a fast picture update for now */
		if (p->owner)
			ast_queue_control(p->owner, AST_CONTROL_VIDUPDATE);
		transmit_response(p, "200 OK", req);
		return;
	} else if (!ast_strlen_zero(c = get_header(req, "X-ClientCode"))) {
		/* Client code (from SNOM phone) */
		if (ast_test_flag(p, SIP_USECLIENTCODE)) {
			if (p->owner && p->owner->cdr)
				ast_cdr_setuserfield(p->owner, c);
			if (p->owner && ast_bridged_channel(p->owner) && ast_bridged_channel(p->owner)->cdr)
				ast_cdr_setuserfield(ast_bridged_channel(p->owner), c);
			transmit_response(p, "200 OK", req);
		} else {
			transmit_response(p, "403 Unauthorized", req);
		}
		return;
	}
	/* Other type of INFO message, not really understood by Asterisk */
	/* if (get_msg_text(buf, sizeof(buf), req)) { */

	ast_log(LOG_WARNING, "Unable to parse INFO message from %s. Content %s\n", p->callid, buf);
	transmit_response(p, "415 Unsupported media type", req);
	return;
}

/*! \brief  sip_do_debug: Enable SIP Debugging in CLI ---*/
static int sip_do_debug_ip(int fd, int argc, char *argv[])
{
	struct hostent *hp;
	struct ast_hostent ahp;
	char iabuf[INET_ADDRSTRLEN];
	int port = 0;
	char *p, *arg;

	if (argc != 4)
		return RESULT_SHOWUSAGE;
	arg = argv[3];
	p = strstr(arg, ":");
	if (p) {
		*p = '\0';
		p++;
		port = atoi(p);
	}
	hp = ast_gethostbyname(arg, &ahp);
	if (hp == NULL)  {
		return RESULT_SHOWUSAGE;
	}
	debugaddr.sin_family = AF_INET;
	memcpy(&debugaddr.sin_addr, hp->h_addr, sizeof(debugaddr.sin_addr));
	debugaddr.sin_port = htons(port);
	if (port == 0)
		ast_cli(fd, "SIP Debugging Enabled for IP: %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), debugaddr.sin_addr));
	else
		ast_cli(fd, "SIP Debugging Enabled for IP: %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), debugaddr.sin_addr), port);
	sipdebug |= SIP_DEBUG_CONSOLE;
	return RESULT_SUCCESS;
}

/*! \brief  sip_do_debug_peer: Turn on SIP debugging with peer mask */
static int sip_do_debug_peer(int fd, int argc, char *argv[])
{
	struct sip_peer *peer;
	char iabuf[INET_ADDRSTRLEN];
	if (argc != 4)
		return RESULT_SHOWUSAGE;
	peer = find_peer(argv[3], NULL, 1);
	if (peer) {
		if (peer->addr.sin_addr.s_addr) {
			debugaddr.sin_family = AF_INET;
			memcpy(&debugaddr.sin_addr, &peer->addr.sin_addr, sizeof(debugaddr.sin_addr));
			debugaddr.sin_port = peer->addr.sin_port;
			ast_cli(fd, "SIP Debugging Enabled for IP: %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), debugaddr.sin_addr), ntohs(debugaddr.sin_port));
			sipdebug |= SIP_DEBUG_CONSOLE;
		} else
			ast_cli(fd, "Unable to get IP address of peer '%s'\n", argv[3]);
		ASTOBJ_UNREF(peer,sip_destroy_peer);
	} else
		ast_cli(fd, "No such peer '%s'\n", argv[3]);
	return RESULT_SUCCESS;
}

/*! \brief  sip_do_debug: Turn on SIP debugging (CLI command) */
static int sip_do_debug(int fd, int argc, char *argv[])
{
	int oldsipdebug = sipdebug & SIP_DEBUG_CONSOLE;
	if (argc != 2) {
		if (argc != 4) 
			return RESULT_SHOWUSAGE;
		else if (strncmp(argv[2], "ip\0", 3) == 0)
			return sip_do_debug_ip(fd, argc, argv);
		else if (strncmp(argv[2], "peer\0", 5) == 0)
			return sip_do_debug_peer(fd, argc, argv);
		else return RESULT_SHOWUSAGE;
	}
	sipdebug |= SIP_DEBUG_CONSOLE;
	memset(&debugaddr, 0, sizeof(debugaddr));
	if (oldsipdebug)
		ast_cli(fd, "SIP Debugging re-enabled\n");
	else
		ast_cli(fd, "SIP Debugging enabled\n");
	return RESULT_SUCCESS;
}

/*! \brief  sip_notify: Send SIP notify to peer */
static int sip_notify(int fd, int argc, char *argv[])
{
	struct ast_variable *varlist;
	int i;

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	if (!notify_types) {
		ast_cli(fd, "No %s file found, or no types listed there\n", notify_config);
		return RESULT_FAILURE;
	}

	varlist = ast_variable_browse(notify_types, argv[2]);

	if (!varlist) {
		ast_cli(fd, "Unable to find notify type '%s'\n", argv[2]);
		return RESULT_FAILURE;
	}

	for (i = 3; i < argc; i++) {
		struct sip_pvt *p;
		struct sip_request req;
		struct ast_variable *var;

		p = sip_alloc(NULL, NULL, 0, SIP_NOTIFY);
		if (!p) {
			ast_log(LOG_WARNING, "Unable to build sip pvt data for notify\n");
			return RESULT_FAILURE;
		}

		if (create_addr(p, argv[i])) {
			/* Maybe they're not registered, etc. */
			sip_destroy(p);
			ast_cli(fd, "Could not create address for '%s'\n", argv[i]);
			continue;
		}

		initreqprep(&req, p, SIP_NOTIFY);

		for (var = varlist; var; var = var->next)
			add_header(&req, var->name, var->value);

		add_blank_header(&req);
		/* Recalculate our side, and recalculate Call ID */
		if (ast_sip_ouraddrfor(&p->sa.sin_addr, &p->ourip))
			memcpy(&p->ourip, &__ourip, sizeof(p->ourip));
		build_via(p, p->via, sizeof(p->via));
		build_callid(p->callid, sizeof(p->callid), p->ourip, p->fromdomain);
		ast_cli(fd, "Sending NOTIFY of type '%s' to '%s'\n", argv[2], argv[i]);
		transmit_sip_request(p, &req, 0);
		sip_scheddestroy(p, 15000);
	}

	return RESULT_SUCCESS;
}
/*! \brief  sip_do_history: Enable SIP History logging (CLI) ---*/
static int sip_do_history(int fd, int argc, char *argv[])
{
	if (argc != 2) {
		return RESULT_SHOWUSAGE;
	}
	recordhistory = 1;
	ast_cli(fd, "SIP History Recording Enabled (use 'sip show history')\n");
	return RESULT_SUCCESS;
}

/*! \brief  sip_no_history: Disable SIP History logging (CLI) ---*/
static int sip_no_history(int fd, int argc, char *argv[])
{
	if (argc != 3) {
		return RESULT_SHOWUSAGE;
	}
	recordhistory = 0;
	ast_cli(fd, "SIP History Recording Disabled\n");
	return RESULT_SUCCESS;
}

/*! \brief  sip_no_debug: Disable SIP Debugging in CLI ---*/
static int sip_no_debug(int fd, int argc, char *argv[])

{
	if (argc != 3)
		return RESULT_SHOWUSAGE;
	sipdebug &= ~SIP_DEBUG_CONSOLE;
	ast_cli(fd, "SIP Debugging Disabled\n");
	return RESULT_SUCCESS;
}

static int reply_digest(struct sip_pvt *p, struct sip_request *req, char *header, int sipmethod, char *digest, int digest_len);

/*--- do_subscribe_auth: Authenticate for outbound subscription ---*/
static int do_subscribe_auth(struct sip_pvt *p, struct sip_request *req, char *header, char *respheader) 
{
	char digest[1024];
	p->authtries++;
	memset(digest,0,sizeof(digest));
	if (reply_digest(p, req, header, SIP_SUBSCRIBE, digest, sizeof(digest))) {
		/* There's nothing to use for authentication */
 		/* No digest challenge in request */
 		if (sip_debug_test_pvt(p) && p->subscription)
 			ast_verbose("No authentication challenge, sending blank subscription to domain/host name %s\n", p->subscription->hostname);
 			/* No old challenge */
		return -1;
	}
 	if (sip_debug_test_pvt(p) && p->subscription)
 		ast_verbose("Responding to challenge, subscription to domain/host name %s\n", p->subscription->hostname);
	return transmit_subscribe(p->subscription, SIP_SUBSCRIBE, digest, respheader); 
}

/*! \brief  do_register_auth: Authenticate for outbound registration ---*/
static int do_register_auth(struct sip_pvt *p, struct sip_request *req, char *header, char *respheader) 
{
	char digest[1024];
	p->authtries++;
	memset(digest,0,sizeof(digest));
	if (reply_digest(p, req, header, SIP_REGISTER, digest, sizeof(digest))) {
		/* There's nothing to use for authentication */
 		/* No digest challenge in request */
 		if (sip_debug_test_pvt(p) && p->registry)
 			ast_verbose("No authentication challenge, sending blank registration to domain/host name %s\n", p->registry->hostname);
 			/* No old challenge */
		return -1;
	}
	if (recordhistory) {
		char tmp[80];
		snprintf(tmp, sizeof(tmp), "Try: %d", p->authtries);
		append_history(p, "RegistryAuth", tmp);
	}
 	if (sip_debug_test_pvt(p) && p->registry)
 		ast_verbose("Responding to challenge, registration to domain/host name %s\n", p->registry->hostname);
	return transmit_register(p->registry, SIP_REGISTER, digest, respheader,
	    0); 
}

/*! \brief  do_proxy_auth: Add authentication on outbound SIP packet ---*/
static int do_proxy_auth(struct sip_pvt *p, struct sip_request *req, char *header, char *respheader, int sipmethod, int init) 
{
	char digest[1024];

	if (!p->options) {
		p->options = calloc(1, sizeof(*p->options));
		if (!p->options) {
			ast_log(LOG_ERROR, "Out of memory\n");
			return -2;
		}
	}

	p->authtries++;
	if (option_debug > 1)
		ast_log(LOG_DEBUG, "Auth attempt %d on %s\n", p->authtries, sip_methods[sipmethod].text);
	memset(digest, 0, sizeof(digest));
	if (reply_digest(p, req, header, sipmethod, digest, sizeof(digest) )) {
		/* No way to authenticate */
		return -1;
	}
	/* Now we have a reply digest */
	p->options->auth = digest;
	p->options->authheader = respheader;
	if (sipmethod == SIP_PRACK)
		return transmit_prack(p, 0, NULL);
	return transmit_invite(p, sipmethod, sipmethod == SIP_INVITE, init); 
}

/*--- reply_digest: reply to authentication for outbound
 *    registrations/subscriptions. This is used for register= servers in sip.conf, 
 *    SIP proxies we register with  for receiving calls from, or subscribe=
 *    servers in sip.conf, SIP proxies we subscribe with for recieving
 *    notifications from.  */
static int reply_digest(struct sip_pvt *p, struct sip_request *req,
	char *header, int sipmethod,  char *digest, int digest_len)
{
	char tmp[512];
	char *c;
	char oldnonce[256];

	/* table of recognised keywords, and places where they should be copied */
	const struct x {
		const char *key;
		char *dst;
		int dstlen;
	} *i, keys[] = {
		{ "realm=", p->realm, sizeof(p->realm) },
		{ "nonce=", p->nonce, sizeof(p->nonce) },
		{ "opaque=", p->opaque, sizeof(p->opaque) },
		{ "qop=", p->qop, sizeof(p->qop) },
		{ "domain=", p->domain, sizeof(p->domain) },
		{ NULL, NULL, 0 },
	};

	ast_copy_string(tmp, get_header(req, header), sizeof(tmp));
	if (ast_strlen_zero(tmp)) 
		return -1;
	if (strncasecmp(tmp, "Digest ", strlen("Digest "))) {
		ast_log(LOG_WARNING, "missing Digest.\n");
		return -1;
	}
	c = tmp + strlen("Digest ");
	for (i = keys; i->key != NULL; i++)
		i->dst[0] = '\0';	/* init all to empty strings */
	ast_copy_string(oldnonce, p->nonce, sizeof(oldnonce));
	while (c && *(c = ast_skip_blanks(c))) {	/* lookup for keys */
		for (i = keys; i->key != NULL; i++) {
			char *src, *separator;
			if (strncasecmp(c, i->key, strlen(i->key)) != 0)
				continue;
			/* Found. Skip keyword, take text in quotes or up to the separator. */
			c += strlen(i->key);
			if (*c == '\"') {
				src = ++c;
				separator = "\"";
			} else {
				src = c;
				separator = ",";
			}
			strsep(&c, separator); /* clear separator and move ptr */
			ast_copy_string(i->dst, src, i->dstlen);
			break;
		}
		if (i->key == NULL) /* not found, try ',' */
			strsep(&c, ",");
	}
	/* Reset nonce count */
	if (strcmp(p->nonce, oldnonce)) 
		p->noncecount = 0;

	/* Save auth data for following registrations */
	if (p->registry) {
		struct sip_registry *r = p->registry;

		if (strcmp(r->nonce, p->nonce)) {
			ast_copy_string(r->realm, p->realm, sizeof(r->realm));
			ast_copy_string(r->nonce, p->nonce, sizeof(r->nonce));
			ast_copy_string(r->domain, p->domain, sizeof(r->domain));
			ast_copy_string(r->opaque, p->opaque, sizeof(r->opaque));
			ast_copy_string(r->qop, p->qop, sizeof(r->qop));
			r->noncecount = 0;
		}
	}
	return build_reply_digest(p, sipmethod, digest, digest_len); 
}

/*! \brief  build_reply_digest:  Build reply digest ---*/
/*      Build digest challenge for authentication of peers (for registration) 
	and users (for calls). Also used for authentication of CANCEL and BYE */
/*	Returns -1 if we have no auth */
static int build_reply_digest(struct sip_pvt *p, int method, char* digest, int digest_len)
{
	char a1[256];
	char a2[256];
	char a1_hash[256];
	char a2_hash[256];
	char resp[256];
	char resp_hash[256];
	char uri[256];
	char cnonce[80];
	char iabuf[INET_ADDRSTRLEN];
	char *username;
	char *secret;
	char *md5secret;
	struct sip_auth *auth = (struct sip_auth *) NULL;	/* Realm authentication */

	if (!ast_strlen_zero(p->domain))
		ast_copy_string(uri, p->domain, sizeof(uri));
	else if (!ast_strlen_zero(p->uri))
		ast_copy_string(uri, p->uri, sizeof(uri));
	else
		snprintf(uri, sizeof(uri), "sip:%s@%s",p->username, ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr));

	snprintf(cnonce, sizeof(cnonce), "%08x", thread_safe_rand());

 	/* Check if we have separate auth credentials */
 	if ((auth = find_realm_authentication(authl, p->realm))) {
 		username = auth->username;
 		secret = auth->secret;
 		md5secret = auth->md5secret;
		if (sipdebug)
 			ast_log(LOG_DEBUG,"Using realm %s authentication for call %s\n", p->realm, p->callid);
 	} else {
 		/* No authentication, use peer or register= config */
 		username = p->authname;
 		secret =  p->peersecret;
 		md5secret = p->peermd5secret;
 	}
	if (ast_strlen_zero(username))	/* We have no authentication */
		return -1;
 

 	/* Calculate SIP digest response */
 	snprintf(a1,sizeof(a1),"%s:%s:%s", username, p->realm, secret);
	snprintf(a2,sizeof(a2),"%s:%s", sip_methods[method].text, uri);
	if (!ast_strlen_zero(md5secret))
		ast_copy_string(a1_hash, md5secret, sizeof(a1_hash));
	else
		ast_md5_hash(a1_hash,a1);
	ast_md5_hash(a2_hash,a2);

	p->noncecount++;
	if (!ast_strlen_zero(p->qop))
		snprintf(resp,sizeof(resp),"%s:%s:%08x:%s:%s:%s", a1_hash, p->nonce, p->noncecount, cnonce, "auth", a2_hash);
	else
		snprintf(resp,sizeof(resp),"%s:%s:%s", a1_hash, p->nonce, a2_hash);
	ast_md5_hash(resp_hash, resp);
	/* XXX We hard code our qop to "auth" for now.  XXX */
	if (!ast_strlen_zero(p->qop))
		snprintf(digest, digest_len, "Digest username=\"%s\", realm=\"%s\", algorithm=MD5, uri=\"%s\", nonce=\"%s\", response=\"%s\", opaque=\"%s\", qop=auth, cnonce=\"%s\", nc=%08x", username, p->realm, uri, p->nonce, resp_hash, p->opaque, cnonce, p->noncecount);
	else
		snprintf(digest, digest_len, "Digest username=\"%s\", realm=\"%s\", algorithm=MD5, uri=\"%s\", nonce=\"%s\", response=\"%s\", opaque=\"%s\"", username, p->realm, uri, p->nonce, resp_hash, p->opaque);

	return 0;
}
	
static char show_domains_usage[] = 
"Usage: sip show domains\n"
"       Lists all configured SIP local domains.\n"
"       Asterisk only responds to SIP messages to local domains.\n";

static char notify_usage[] =
"Usage: sip notify <type> <peer> [<peer>...]\n"
"       Send a NOTIFY message to a SIP peer or peers\n"
"       Message types are defined in sip_notify.conf\n";

static char show_users_usage[] = 
"Usage: sip show users [like <pattern>]\n"
"       Lists all known SIP users.\n"
"       Optional regular expression pattern is used to filter the user list.\n";

static char show_user_usage[] =
"Usage: sip show user <name> [load]\n"
"       Lists all details on one SIP user and the current status.\n"
"       Option \"load\" forces lookup of peer in realtime storage.\n";

static char show_inuse_usage[] = 
"Usage: sip show inuse [all]\n"
"       List all SIP users and peers usage counters and limits.\n"
"       Add option \"all\" to show all devices, not only those with a limit.\n";

static char show_channels_usage[] = 
"Usage: sip show channels\n"
"       Lists all currently active SIP channels.\n";

static char show_channel_usage[] = 
"Usage: sip show channel <channel>\n"
"       Provides detailed status on a given SIP channel.\n";

static char show_history_usage[] = 
"Usage: sip show history <channel>\n"
"       Provides detailed dialog history on a given SIP channel.\n";

static char show_peers_usage[] = 
"Usage: sip show peers [like <pattern>]\n"
"       Lists all known SIP peers.\n"
"       Optional regular expression pattern is used to filter the peer list.\n";

static char show_peer_usage[] =
"Usage: sip show peer <name> [load]\n"
"       Lists all details on one SIP peer and the current status.\n"
"       Option \"load\" forces lookup of peer in realtime storage.\n";

static char prune_realtime_usage[] =
"Usage: sip prune realtime [peer|user] [<name>|all|like <pattern>]\n"
"       Prunes object(s) from the cache.\n"
"       Optional regular expression pattern is used to filter the objects.\n";

static char show_reg_usage[] =
"Usage: sip show registry\n"
"       Lists all registration requests and status.\n";

static char debug_usage[] = 
"Usage: sip debug\n"
"       Enables dumping of SIP packets for debugging purposes\n\n"
"       sip debug ip <host[:PORT]>\n"
"       Enables dumping of SIP packets to and from host.\n\n"
"       sip debug peer <peername>\n"
"       Enables dumping of SIP packets to and from host.\n"
"       Require peer to be registered.\n";

static char no_debug_usage[] = 
"Usage: sip no debug\n"
"       Disables dumping of SIP packets for debugging purposes\n";

static char no_history_usage[] = 
"Usage: sip no history\n"
"       Disables recording of SIP dialog history for debugging purposes\n";

static char history_usage[] = 
"Usage: sip history\n"
"       Enables recording of SIP dialog history for debugging purposes.\n"
"Use 'sip show history' to view the history of a call number.\n";

static char sip_reload_usage[] =
"Usage: sip reload\n"
"       Reloads SIP configuration from sip.conf\n";

static char show_subscriptions_usage[] =
"Usage: sip show subscriptions\n" 
"       Shows active SIP subscriptions for extension states\n";

static char show_objects_usage[] =
"Usage: sip show objects\n" 
"       Shows status of known SIP objects\n";

static char show_settings_usage[] = 
"Usage: sip show settings\n"
"       Provides detailed list of the configuration of the SIP channel.\n";



static char sip_hangup_active_calls_usage[] = 
"Usage: sip hangup_active_calls\n"
"       Hangup all active sip calls.\n";

static char sip_sendmessage_cli_usage[] = 
"Usage: sip message <peer> <smsc> <ip-sm-gw> <type> <content>\n"
"       Content should be hex encoded for binary support.\n";

static char sip_unregister_usage[] = 
"Usage: sip unregister <line>\n"
"       Unregister from SIP proxy of that line.\n";

static char sip_template_register_forbid_usage[] =
"Usage: sip template_register_forbid <forbid>\n"
"	Indicate if template registration need to be forbidden.\n";

/*! \brief  func_header_read: Read SIP header (dialplan function) */
static char *func_header_read(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len) 
{
	struct sip_pvt *p;
	char *content;
	
 	if (!data) {
		ast_log(LOG_WARNING, "This function requires a header name.\n");
		return NULL;
	}

	ast_mutex_lock(&chan->lock);
	if (chan->type != channeltype) {
		ast_log(LOG_WARNING, "This function can only be used on SIP channels.\n");
		ast_mutex_unlock(&chan->lock);
		return NULL;
	}

	p = chan->tech_pvt;

	/* If there is no private structure, this channel is no longer alive */
	if (!p) {
		ast_mutex_unlock(&chan->lock);
		return NULL;
	}

	content = get_header(&p->initreq, data);

	if (ast_strlen_zero(content)) {
		ast_mutex_unlock(&chan->lock);
		return NULL;
	}

	ast_copy_string(buf, content, len);
	ast_mutex_unlock(&chan->lock);

	return buf;
}


static struct ast_custom_function sip_header_function = {
	.name = "SIP_HEADER",
	.synopsis = "Gets or sets the specified SIP header",
	.syntax = "SIP_HEADER(<name>)",
	.read = func_header_read,
};

/*! \brief  function_check_sipdomain: Dial plan function to check if domain is local */
static char *func_check_sipdomain(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len)
{
	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "CHECKSIPDOMAIN requires an argument - A domain name\n");
		return buf;
	}
	if (check_sip_domain(data, NULL, 0))
		ast_copy_string(buf, data, len);
	else
		buf[0] = '\0';
	return buf;
}

static struct ast_custom_function checksipdomain_function = {
	.name = "CHECKSIPDOMAIN",
	.synopsis = "Checks if domain is a local domain",
	.syntax = "CHECKSIPDOMAIN(<domain|IP>)",
	.read = func_check_sipdomain,
	.desc = "This function checks if the domain in the argument is configured\n"
		"as a local SIP domain that this Asterisk server is configured to handle.\n"
		"Returns the domain name if it is locally handled, otherwise an empty string.\n"
		"Check the domain= configuration in sip.conf\n",
};


/*! \brief  function_sippeer: ${SIPPEER()} Dialplan function - reads peer data */
static char *function_sippeer(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len)
{
	char *ret = NULL;
	struct sip_peer *peer;
	char *peername, *colname;
	char iabuf[INET_ADDRSTRLEN];

	if (!(peername = ast_strdupa(data))) {
		ast_log(LOG_ERROR, "Memory Error!\n");
		return ret;
	}

	if ((colname = strchr(peername, ':'))) {
		*colname = '\0';
		colname++;
	} else {
		colname = "ip";
	}
	if (!(peer = find_peer(peername, NULL, 1)))
		return ret;

	if (!strcasecmp(colname, "ip")) {
		ast_copy_string(buf, peer->addr.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), peer->addr.sin_addr) : "", len);
	} else  if (!strcasecmp(colname, "status")) {
		peer_status(peer, buf, sizeof(buf));
	} else  if (!strcasecmp(colname, "language")) {
		ast_copy_string(buf, peer->language, len);
	} else  if (!strcasecmp(colname, "regexten")) {
		ast_copy_string(buf, peer->regexten, len);
	} else  if (!strcasecmp(colname, "limit")) {
		snprintf(buf, len, "%d", peer->call_limit);
	} else  if (!strcasecmp(colname, "curcalls")) {
		snprintf(buf, len, "%d", peer->inUse);
	} else  if (!strcasecmp(colname, "useragent")) {
		ast_copy_string(buf, peer->useragent, len);
	} else  if (!strcasecmp(colname, "mailbox")) {
		ast_copy_string(buf, peer->mailbox, len);
	} else  if (!strcasecmp(colname, "remotemailbox")) {
		ast_copy_string(buf, peer->remotemailbox, len);
	} else  if (!strcasecmp(colname, "context")) {
		ast_copy_string(buf, peer->context, len);
	} else  if (!strcasecmp(colname, "expire")) {
		snprintf(buf, len, "%d", peer->expire);
	} else  if (!strcasecmp(colname, "dynamic")) {
		ast_copy_string(buf, (ast_test_flag(peer, SIP_DYNAMIC) ? "yes" : "no"), len);
	} else  if (!strcasecmp(colname, "callerid_name")) {
		ast_copy_string(buf, peer->cid_name, len);
	} else  if (!strcasecmp(colname, "callerid_num")) {
		ast_copy_string(buf, peer->cid_num, len);
	} else  if (!strcasecmp(colname, "codecs")) {
		ast_getformatname_multiple(buf, len -1, peer->capability);
	} else  if (!strncasecmp(colname, "codec[", 6)) {
		char *codecnum, *ptr;
		int index = 0, codec = 0;
		
		codecnum = strchr(colname, '[');
		*codecnum = '\0';
		codecnum++;
		if ((ptr = strchr(codecnum, ']'))) {
			*ptr = '\0';
		}
		index = atoi(codecnum);
		if((codec = ast_codec_pref_index_audio(&peer->prefs, index))) {
			ast_copy_string(buf, ast_getformatname(codec), len);
		}
	}
	ret = buf;

	ASTOBJ_UNREF(peer, sip_destroy_peer);

	return ret;
}

/* Structure to declare a dialplan function: SIPPEER */
struct ast_custom_function sippeer_function = {
	.name = "SIPPEER",
	.synopsis = "Gets SIP peer information",
	.syntax = "SIPPEER(<peername>[:item])",
	.read = function_sippeer,
	.desc = "Valid items are:\n"
	"- ip (default)          The IP address.\n"
	"- mailbox               The configured mailbox.\n"
	"- remotemailbox         The configured remote mailbox.\n"
	"- context               The configured context.\n"
	"- expire                The epoch time of the next expire.\n"
	"- dynamic               Is it dynamic? (yes/no).\n"
	"- callerid_name         The configured Caller ID name.\n"
	"- callerid_num          The configured Caller ID number.\n"
	"- codecs                The configured codecs.\n"
	"- status                Status (if qualify=yes).\n"
	"- regexten              Registration extension\n"
	"- limit                 Call limit (call-limit)\n"
	"- curcalls              Current amount of calls \n"
	"                        Only available if call-limit is set\n"
	"- language              Default language for peer\n"
	"- useragent             Current user agent id for peer\n"
	"- codec[x]              Preferred codec index number 'x' (beginning with zero).\n"
	"\n"
};

/*! \brief  function_sipchaninfo_read: ${SIPCHANINFO()} Dialplan function - reads sip channel data */
static char *function_sipchaninfo_read(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len) 
{
	struct sip_pvt *p;
	char iabuf[INET_ADDRSTRLEN];

	*buf = 0;
	
 	if (!data) {
		ast_log(LOG_WARNING, "This function requires a parameter name.\n");
		return NULL;
	}

	ast_mutex_lock(&chan->lock);
	if (chan->type != channeltype) {
		ast_log(LOG_WARNING, "This function can only be used on SIP channels.\n");
		ast_mutex_unlock(&chan->lock);
		return NULL;
	}

/* 	ast_verbose("function_sipchaninfo_read: %s\n", data); */
	p = chan->tech_pvt;

	/* If there is no private structure, this channel is no longer alive */
	if (!p) {
		ast_mutex_unlock(&chan->lock);
		return NULL;
	}

	if (!strcasecmp(data, "peerip")) {
		ast_copy_string(buf, p->sa.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr) : "", len);
	} else  if (!strcasecmp(data, "recvip")) {
		ast_copy_string(buf, p->recv.sin_addr.s_addr ? ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr) : "", len);
	} else  if (!strcasecmp(data, "from")) {
		ast_copy_string(buf, p->from, len);
	} else  if (!strcasecmp(data, "uri")) {
		ast_copy_string(buf, p->uri, len);
	} else  if (!strcasecmp(data, "useragent")) {
		ast_copy_string(buf, p->useragent, len);
	} else  if (!strcasecmp(data, "peername")) {
		ast_copy_string(buf, p->peername, len);
	} else {
		ast_mutex_unlock(&chan->lock);
		return NULL;
	}
	ast_mutex_unlock(&chan->lock);

	return buf;
}

/* Structure to declare a dialplan function: SIPCHANINFO */
static struct ast_custom_function sipchaninfo_function = {
	.name = "SIPCHANINFO",
	.synopsis = "Gets the specified SIP parameter from the current channel",
	.syntax = "SIPCHANINFO(item)",
	.read = function_sipchaninfo_read,
	.desc = "Valid items are:\n"
	"- peerip                The IP address of the peer.\n"
	"- recvip                The source IP address of the peer.\n"
	"- from                  The URI from the From: header.\n"
	"- uri                   The URI from the Contact: header.\n"
	"- useragent             The useragent.\n"
	"- peername              The name of the peer.\n"
};

static int parse_retry_after(struct sip_request *req)
{
	char *retry_after = get_header(req, "Retry-After");
	int result;

	if (ast_strlen_zero(retry_after))
		return -1; /* header not present */

	/* Disregard comments and duration for now */
	if ((result = atoi(retry_after)) < 1) {
		ast_log(LOG_WARNING, "Invalid Retry-After value\n");
		return -1;
	}

	ast_log(LOG_DEBUG, "Retry-After value extracted: %d\n", result);
	return result;
}

/*! \brief  parse_moved_contact: Parse 302 Moved temporalily response */
static void parse_moved_contact(struct sip_pvt *p, struct sip_request *req)
{
	char tmp[256];
	char *s, *e;
	ast_copy_string(tmp, get_header(req, "Contact"), sizeof(tmp));
	s = get_in_brackets(tmp);
	e = strchr(s, ';');
	if (e)
		*e = '\0';
	if (ast_test_flag(p, SIP_PROMISCREDIR)) {
		if (!strncasecmp(s, "sip:", 4))
			s += 4;
		e = strchr(s, '/');
		if (e)
			*e = '\0';
		ast_log(LOG_DEBUG, "Found promiscuous redirection to 'SIP/%s'\n", s);
		if (p->owner) {
#if 1
			/* Asterisk will call sip_request_call() to forward the call.
			 * 'call_forward' is the only mean to pass information about the new
			 * call. In the ATA model it's important that we use a specific
			 * user/password set for the new call, so we have to force
			 * sip_request_call() to use our current peer also for the new call.
			 * This hack should not be used once we allow the user to configure
			 * username/password per realm. */
			if (p->peername[0])
			{
				e = strchr(s, '@');
				if (e)
					*e = '\0';
				snprintf(p->owner->call_forward, sizeof(p->owner->call_forward), "SIP/%s@%s", s, p->peername);
			}
			else
#endif
				snprintf(p->owner->call_forward, sizeof(p->owner->call_forward), "SIP/%s", s);
		}
	} else {
		e = strchr(tmp, '@');
		if (e)
			*e = '\0';
		e = strchr(tmp, '/');
		if (e)
			*e = '\0';
		if (!strncasecmp(s, "sip:", 4))
			s += 4;
		ast_log(LOG_DEBUG, "Found 302 Redirect to extension '%s'\n", s);
		if (p->owner)
			ast_copy_string(p->owner->call_forward, s, sizeof(p->owner->call_forward));
	}
}

/*! \brief  check_pendings: Check pending actions on SIP call ---*/
static void check_pendings(struct sip_pvt *p)
{
	int delay;

	/* Go ahead and send bye at this point */
	if (ast_test_flag(p, SIP_PENDINGBYE)) {
		transmit_request_with_auth(p, SIP_BYE, 0, 1, 1);
		ast_set_flag(p, SIP_NEEDDESTROY);	
		ast_clear_flag(p, SIP_NEEDREINVITE);	
	} else if (ast_test_flag(p, SIP_NEEDREINVITE)) {

		/* We send UPDATE if needed even if there is pending invite since we
		 * dont' send SDP */
		if (p->method == SIP_UPDATE)
			transmit_reinvite(p, 1, 0, SIP_UPDATE);
		else
		{
		    /* if we can't REINVITE, hold it for later */
		    if (p->waitid > 0)
			ast_log(LOG_DEBUG, "NOT Sending pending reinvite (yet) on '%s'\n", p->callid);
		    else
		    {
			ast_log(LOG_DEBUG, "Sending pending reinvite on '%s'\n", p->callid);
			/* Didn't get to reinvite yet, so do it now */
			switch (p->vbdmode) {
			case VBD_MODE_FAX:
			    if (p->faxmethod == FAX_T38_AUTO && p->t38state == T38_DISABLED) {
					delay = ast_test_flag(p, SIP_OUTGOING) ?
						modemfax_tx_reinvite_delay :
						modemfax_rx_reinvite_delay;
				
					ast_log(LOG_DEBUG, "schedule t38 re-INVITE, delay - %d\n", delay);

					if(delay)
					{
						ast_sched_add(sched, delay,
							sip_do_reinvite_t38, p);
					}
					else 
						sip_do_reinvite_t38(p);					
				}
				else if (p->faxmethod == FAX_PASSTHROUGH_AUTO && 
					(!(p->owner->readformat & p->faxtxcodecs) ||
					!(p->owner->writeformat & p->faxtxcodecs)))
				{
					delay = ast_test_flag(p, SIP_OUTGOING) ?
						modemfax_tx_reinvite_delay :
						modemfax_rx_reinvite_delay;

				
					ast_log(LOG_DEBUG, "schedule fax passthrough re-INVITE, delay - %d\n", delay);

					if(delay)
					{
						ast_sched_add(sched, delay,
							sip_do_reinvite_fax, p);
					} 
					else 
						sip_do_reinvite_fax(p);
			    }
			    break;
			case VBD_MODE_MODEM:
				if (p->owner && (!(p->owner->readformat & p->modemtxcodecs) ||
					!(p->owner->writeformat & p->modemtxcodecs))) 
				{
					delay = ast_test_flag(p, SIP_OUTGOING) ?
						modemfax_tx_reinvite_delay :
						modemfax_rx_reinvite_delay;
				
					ast_log(LOG_DEBUG, "schedule modem re-INVITE, delay - %d\n", delay);

					if(delay)
					{
						ast_sched_add(sched, delay,
							sip_do_reinvite_modem, p);
					}
					else 
						sip_do_reinvite_modem(p);
			    }
			    break;
			default:
			    transmit_reinvite_with_sdp(p, FALSE);
			    break;
			}
		    }
		    ast_clear_flag(p, SIP_NEEDREINVITE);	
		}
	}
}

/*! \brief Reset the NEEDREINVITE flag after waiting when we get 491 on a Re-invite
	to avoid race conditions between asterisk servers.
	Called from the scheduler.
*/
static int sip_reinvite_retry(void *data)
{
	struct sip_pvt *p = (struct sip_pvt *) data;
	struct ast_channel *owner;

	ast_mutex_lock(&p->lock);
	/* called from schedule thread which requires a lock */
	while ((owner = p->owner) && ast_mutex_trylock(&owner->lock)) {
		ast_mutex_unlock(&p->lock);
		usleep(1);
		ast_mutex_lock(&p->lock);
	}
	ast_set_flag(p, SIP_NEEDREINVITE);
	p->waitid = -1;
	check_pendings(p);
	ast_mutex_unlock(&p->lock);
	if (owner) {
		ast_mutex_unlock(&owner->lock);
	}
	return 0;
}

static void notify_manager_instant_dial(struct sip_pvt *p)
{
	if(instant_dial_enabled && !p->instant_dial_added &&
		ast_test_flag(&p->flags_page2, SIP_PAGE2_OUTGOING_CALL))
	{
		p->instant_dial_added = 1;
		manager_event(EVENT_FLAG_CALL, "AddInstantDial",
			"Number: %s\r\n",p->username); 
	}
}

static void sip_send_transparent_error_response(struct sip_pvt *p, struct sip_request *resp)
{
	ast_transparent_frame_payload payload;

	payload.code = AST_TRANSPARENT_SIP_RESPONSE;
	payload.data = (void *)resp;
	payload.data_size = sizeof(struct sip_request);
	ast_queue_control_data(p->owner, AST_CONTROL_TRANSPARENT_DATA, &payload, payload.data_size + 8);
}

/*! \brief  handle_response_invite: Handle SIP response in dialogue ---*/
static void handle_response_invite(struct sip_pvt *p, int resp, char *rest, struct sip_request *req, int ignore, int seqno)
{
	int outgoing = ast_test_flag(p, SIP_OUTGOING);
	struct ast_channel *bridgepeer = NULL;
	char *p_hdrval;
	int rtn;
	int reinvite = (p->owner && p->owner->_state == AST_STATE_UP);
	char *msg, *c;
	int sipmethod;
	struct sip_registry *reg;

	c = get_header(req, "Cseq");
	msg = strchr(c, ' ');
	if (!msg)
		msg = "";
	else
		msg++;
	sipmethod = find_sip_method(msg);

	if (option_debug > 3) {
		if (reinvite)
			ast_log(LOG_DEBUG, "SIP response %d to RE-invite on %s call %s\n", resp, outgoing ? "outgoing" : "incoming", p->callid);
		else
			ast_log(LOG_DEBUG, "SIP response %d to standard invite\n", resp);
	}

	if (ast_test_flag(p, SIP_ALREADYGONE)) { /* This call is already gone */
		ast_log(LOG_DEBUG, "Got response on call that is already terminated: %s (ignoring)\n", p->callid);
		return;
	}

	/* If this is a response to our initial INVITE, we need to set what we can use
	 * for this peer.
	 */
	if (!reinvite) 
		set_pvt_allowed_methods(p, req);		
	switch (resp) {
	case 100:	/* Trying */
                if ((reg = get_registry_for_sip(p)))
			set_server_na(reg, 0);
		if (!ast_test_flag(&p->flags_page2, SIP_PAGE2_INVITECANCELLED))
			sip_cancel_destroy(p);
		break;
	case 180:	/* 180 Ringing */
	case 181:
		if (!ast_test_flag(&p->flags_page2, SIP_PAGE2_INVITECANCELLED))
			sip_cancel_destroy(p);
		if (!ignore && p->owner) {
			ast_queue_control(p->owner, AST_CONTROL_RINGING);
			if (p->owner->_state != AST_STATE_UP)
				ast_setstate(p->owner, AST_STATE_RINGING);
		}
		if (!strcasecmp(get_header(req, "Content-Type"), "application/sdp")) {
			process_sdp(p, req);
			if (!ignore && p->owner) {
				/* Queue a progress frame only if we have SDP in 180 */
				ast_queue_control_data(p->owner, AST_CONTROL_PROGRESS, &p->formats, sizeof(p->formats));
			}
		}
		/* Parse contact header for continued conversation
		 * When we get 1xx or 200 OK, we know which device (and IP) to contact for this call
		 * This is important when we have a SIP proxy between us and the phone */
		if (outgoing) {
		    parse_ok_contact(p, req);

		    /* Save Record-Route for any later requests we make on this dialogue */
		    build_route(p, req, 1);
		}
		break;
	case 183:	/* Session progress */
		if (!ast_test_flag(&p->flags_page2, SIP_PAGE2_INVITECANCELLED))
			sip_cancel_destroy(p);
		if (!strcasecmp(get_header(req, "Content-Type"), "application/sdp")) {
			process_sdp(p, req);
		}
		if (!ignore && p->owner) {
			/* Queue a progress frame */
			ast_queue_control_data(p->owner, AST_CONTROL_PROGRESS, &p->formats, sizeof(p->formats));
		}
		/* Parse contact header for continued conversation 
		 * When we get 1xx or 200 OK, we know which device (and IP) to contact for this call
		 * This is important when we have a SIP proxy between us and the phone */
		if (outgoing) {
		    parse_ok_contact(p, req);

		    /* Save Record-Route for any later requests we make on this dialogue */
		    build_route(p, req, 1);
		}
		break;
	case 200:	/* 200 OK on invite - someone's answering our call */
		if (!ast_test_flag(&p->flags_page2, SIP_PAGE2_INVITECANCELLED))
			sip_cancel_destroy(p);
		p->authtries = 0;
		if (!strcasecmp(get_header(req, "Content-Type"), "application/sdp")) {
			process_sdp(p, req);
		}

		/* Parse contact header for continued conversation */
		/* When we get 200 OK, we know which device (and IP) to contact for this call */
		/* This is important when we have a SIP proxy between us and the phone */
		if (outgoing) {
			parse_ok_contact(p, req);

			/* Save Record-Route for any later requests we make on this dialogue */
			build_route(p, req, 1);
		}
		notify_manager_instant_dial(p);
#if defined(T38_SUPPORT)

		if (p->owner && (p->owner->_state == AST_STATE_UP) && (bridgepeer = ast_bridged_channel(p->owner))) { /* if this is a re-invite */ 
			struct sip_pvt *bridgepvt = NULL;

			if (!bridgepeer->tech) {
				ast_log(LOG_WARNING, "Ooooh.. no tech!  That's REALLY bad\n");
				break;
			}
			if (!strcasecmp(bridgepeer->tech->type,"SIP")) {
				bridgepvt = (struct sip_pvt*)(bridgepeer->tech_pvt);
				if (bridgepvt->udptl) {
					if (p->t38state == T38_PEER_REINVITE) { 
						/* This is 200 OK to re-invite where T38 was offered on channel so we need to send 200 OK with T38 the other side of the bridge */
						/* Send response with T38 SDP to the other side of the bridge */
						sip_handle_t38_reinvite(bridgepeer,p,0);
					} else if (p->t38state == T38_DISABLED && bridgepeer && (bridgepvt->t38state == T38_ENABLED)) { /* This is case of RTP re-invite after T38 session */
						ast_log(LOG_WARNING, "RTP re-inivte after T38 session not handled yet !\n");
						/* Insted of this we should somehow re-invite the other side of the bridge to RTP */
						ast_set_flag(p, SIP_NEEDDESTROY);
					}
				} else {
						ast_log(LOG_WARNING, "Strange... The other side of the bridge don't have udptl struct\n");
						ast_mutex_lock(&bridgepvt->lock);
						bridgepvt->t38state = T38_DISABLED;
						ast_mutex_unlock(&bridgepvt->lock);
						ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",bridgepvt->t38state, bridgepeer->tech->type);
						p->t38state = T38_DISABLED;
						ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
				}
			} else if (strcasecmp(bridgepeer->tech->type,"jdsp")) {
					/* Other side is not a SIP channel nor a JDSP channel*/ 
					ast_log(LOG_WARNING, "Strange... The other side of the bridge is not a SIP channel\n");
					p->t38state = T38_DISABLED;
					ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
			}
		}
		if ((p->t38state == T38_LOCAL_REINVITE) || (p->t38state == T38_LOCAL_DIRECT)) {
			/* If there was T38 reinvite and we are supposed to answer with 200 OK than this should set us to T38 negotiated mode */
			ast_set_flag(p->owner, AST_FLAG_T38);
			p->t38state = T38_ENABLED;
			p->vbdmode = VBD_MODE_FAX;
			ast_queue_control(p->owner, AST_CONTROL_T38);
			ast_log(LOG_DEBUG, "T38 changed state to %d on channel %s\n", p->t38state, p->owner ? p->owner->name : "<none>");
		}
#endif
		if (!ignore && p->owner) {
			if (p->owner->_state != AST_STATE_UP) {
#ifdef OSP_SUPPORT	
				time(&p->ospstart);
#endif
				set_template_caller_name(p);
				p->is_sdp_disabled = !has_sdp(req);
				p->owner->answer_data = &p->is_sdp_disabled;
				ast_queue_control(p->owner, AST_CONTROL_ANSWER);
				ast_rtp_stats_logging_start(p->rtp, 1, p->username);

			} else {	/* RE-invite */
				struct ast_frame af = { AST_FRAME_NULL, };
				ast_queue_frame(p->owner, &af);
			}
		} else {
			 /* It's possible we're getting an ACK after we've tried to disconnect
				  by sending CANCEL */
			/* THIS NEEDS TO BE CHECKED: OEJ */
			if (!ignore)
				ast_set_flag(p, SIP_PENDINGBYE);	
		}

		/* Check for Session-Timers related headers */
		check_supported_required(p, req, NULL, 0);
		if (st_get_mode(p, 0) != SESSION_TIMER_MODE_REFUSE && ast_test_flag(&p->flags_page2, SIP_PAGE2_OUTGOING_CALL) && !reinvite) {
			/* New dialog, "clear" our min_se */
			p->stimer->st_min_se = st_get_se(p, FALSE);
			p_hdrval = (char*)get_header(req, "Session-Expires");
			if (!ast_strlen_zero(p_hdrval)) {
				/* UAS supports Session-Timers */
				enum st_refresher tmp_st_ref = SESSION_TIMER_REFRESHER_AUTO;
				int tmp_st_interval = 0;
				rtn = parse_session_expires(p_hdrval, &tmp_st_interval, &tmp_st_ref, 0);
				if (rtn != 0) {
					ast_set_flag(p, SIP_PENDINGBYE);	
				}
				if (tmp_st_ref == SESSION_TIMER_REFRESHER_LOCAL || 
					tmp_st_ref == SESSION_TIMER_REFRESHER_REMOTE) {
					p->stimer->st_ref = tmp_st_ref;
				} 
				if (tmp_st_interval) {
					p->stimer->st_interval = tmp_st_interval;
				}
				p->stimer->st_active = TRUE;
				p->stimer->st_active_peer_ua = TRUE;
				start_session_timer(p);
			} else {
				/* UAS doesn't support Session-Timers */
				if ((ast_test_flag(&p->flags_page2,
					SIP_PAGE2_SESSION_TIMERS_FORCE) ||
					p->sipoptions & SIP_OPT_TIMER) && st_get_mode(p, 0) ==
					SESSION_TIMER_MODE_ORIGINATE)
				{
					p->stimer->st_ref = SESSION_TIMER_REFRESHER_LOCAL;
					p->stimer->st_active_peer_ua = FALSE;
					start_session_timer(p);
				}
			}
		}

		/* If I understand this right, the branch is different for a non-200 ACK only */
		if (sipmethod == SIP_INVITE) {
			ast_set_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
	                last_answered_call_timestamp = get_time_from_boot();
			transmit_request(p, SIP_ACK, seqno, 0, 1);
			manager_event(EVENT_FLAG_SYSTEM, "SessionChanged", 
			    "Established: 1\r\n"
			    "Peer: %s\r\n",
			    p->peername);
		}
		check_pendings(p);
		break;
	case 400:
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		break;
	case 407: /* Proxy authentication */
	case 401: /* Www auth */
		/* First we ACK */
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		if (p->options)
			p->options->auth_type = (resp == 401 ? WWW_AUTH : PROXY_AUTH);

		/* Then we AUTH */
		p->theirtag[0]='\0';	/* forget their old tag, so we don't match tags when getting response */
		if (!ignore) {
			char *authenticate = (resp == 401 ? "WWW-Authenticate" : "Proxy-Authenticate");
			char *authorization = (resp == 401 ? "Authorization" : "Proxy-Authorization");
			if ((p->authtries == MAX_AUTHTRIES) || do_proxy_auth(p, req, authenticate, authorization, p->method == SIP_UPDATE ? SIP_UPDATE : SIP_INVITE, 1)) {
				ast_log(LOG_NOTICE, "Failed to authenticate on %s to '%s'\n",
					p->method == SIP_UPDATE ? "UPDATE" : "INVITE",
					get_header(&p->initreq, "From"));
				ast_set_flag(p, SIP_NEEDDESTROY);	
				ast_set_flag(p, SIP_ALREADYGONE);	
				if (p->owner)
					ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
			}
		}
		break;
	case 403: /* Forbidden */
		/* First we ACK */
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		ast_log(LOG_WARNING, "Forbidden - wrong password on authentication for INVITE to '%s'\n", get_header(&p->initreq, "From"));
		if (!ignore && p->owner)
			ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
		ast_set_flag(p, SIP_NEEDDESTROY);	
		ast_set_flag(p, SIP_ALREADYGONE);	
		break;
	case 404: /* Not found */
		if (sipmethod == SIP_INVITE)
	    	transmit_request(p, SIP_ACK, seqno, 0, 0);
		if (p->owner && !ignore)
			ast_queue_control(p->owner, AST_CONTROL_UNALLOCATED);
		ast_set_flag(p, SIP_ALREADYGONE);	
		break;
	case 484: /* Address incomplete */
	case 480: /* Temporarily Unavailable */
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		if (p->owner && !ignore)
		{
			sip_send_transparent_error_response(p, req);

			ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
		}
		ast_set_flag(p, SIP_ALREADYGONE);	
		break;
	case 408:
	case 481: /* Call leg does not exist */
		/* Could be REFER or INVITE */
		ast_log(LOG_WARNING, "Re-invite to non-existing call leg on other UA "
			"or request timeout. SIP dialog '%s'. Giving up.\n", p->callid);

		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);

		if (p->owner && !ignore)
			ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
		ast_set_flag(p, SIP_NEEDDESTROY);	
		break;

	case 422: /* Session-Timers: Session interval too small */
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		p->theirtag[0] = '\0';
		proc_422_rsp(p, req);
		break;

	case 487: /* Cancelled transaction */
		/* We have sent CANCEL on an outbound INVITE 
			This transaction is already scheduled to be killed by sip_hangup().
		*/
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		if (p->owner && !ignore) {
			ast_queue_hangup(p->owner);
			append_history(p, "Hangup", "Got 487 on CANCEL request from us. Queued AST hangup request");
		} else if (!ignore) {
			update_call_counter(p, DEC_CALL_LIMIT);
 			append_history(p, "Hangup", "Got 487 on CANCEL request from us on call without owner. Killing this dialog.");
 			ast_set_flag(p, SIP_NEEDDESTROY);	
 			ast_set_flag(p, SIP_ALREADYGONE);
		}
		break;
	case 491: /* Pending */
		/* we have to wait a while, then retransmit */
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		if (p->owner && !ignore) {
			if (p->owner->_state != AST_STATE_UP) {
				ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
				ast_log(LOG_DEBUG, "received 491 response\n");
			} else {
				/* This is a re-invite that failed. */
				/* Reset the flag after a while
				 */
				int wait;
				/* RFC 3261, if owner of call, wait between 2.1 to 4 seconds,
				 * if not owner of call, wait 0 to 2 seconds */
				if (ast_test_flag(&p->flags_page2, SIP_PAGE2_OUTGOING_CALL))
					wait = 2100 + thread_safe_rand() % 2000;
				else
					wait = thread_safe_rand() % 2000;

				if (p->waitid != -1) {
					ast_log(LOG_DEBUG, "Reinvite race during existing reinvite race. Abandoning previous reinvite retry.\n");
					ast_sched_del(sched, p->waitid);
					p->waitid = -1;
				}

				/* A 491 response when t38state is T38_LOCAL_REINVITE means our
				 * reinvite wasn't accepted, set t38state back to DISABLED */
				p->t38state = T38_DISABLED;

				/* Reinvite race - our previous offer is invalid */
				ast_clear_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER);

				p->waitid = ast_sched_add(sched, wait, sip_reinvite_retry, p);
				ast_log(LOG_DEBUG, "Reinvite race. Waiting %d secs before retry\n", wait);
			}
		}
		break;
	case 405:
	case 501: /* Not implemented */
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		if (p->owner)
		{
			sip_send_transparent_error_response(p, req);

			ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
		}
		break;
	case 488:
		if (p->t38state == T38_LOCAL_REINVITE)
			p->t38state = T38_DISABLED;
		
		if (sipmethod == SIP_INVITE) {
			transmit_request(p, SIP_ACK, seqno, 0, 0);

			/* End call only for initial invite */
			if (!reinvite) {
				if (p->owner && !ignore)
				{
					sip_send_transparent_error_response(p, req);

					ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
				}
				ast_set_flag(p, SIP_NEEDDESTROY);	
				ast_set_flag(p, SIP_ALREADYGONE);
			}
		}
		break;
	case 486: /* Busy here */
		if (p->owner)
		{
			if (cCONFIG_RG_VODAFONE_NZ)
			{
				char state[] = "unobtainable";
				ast_queue_control_data(p->owner, AST_CONTROL_BUSY, state, sizeof(state));
			}
			else
				ast_queue_control(p->owner, AST_CONTROL_BUSY);		
		}
		if (sipmethod == SIP_INVITE)
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		ast_set_flag(p, SIP_ALREADYGONE);
		break;
	case 600: /* Busy everywhere */
		if (p->owner)
			ast_queue_control(p->owner, AST_CONTROL_BUSY);
		if (sipmethod == SIP_INVITE) 
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		ast_set_flag(p, SIP_ALREADYGONE);
		break;
	case 603: /* Decline */
		if (p->owner)
			ast_queue_control(p->owner, AST_CONTROL_DECLINE);
	case 413: /* Request entity too large */		
		if (sipmethod == SIP_INVITE) 
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		ast_set_flag(p, SIP_ALREADYGONE);
		break;
	case 500: /* Server internal error */
	case 503: /* Service unavailable */		
		if (p->owner)
		{
			sip_send_transparent_error_response(p, req);

			ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
		}
		if (sipmethod == SIP_INVITE) 
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		ast_set_flag(p, SIP_ALREADYGONE);
		break;
	}
}

/* \brief Handle SIP response in REFER transaction
	We've sent a REFER, now handle responses to it 
  */
static void handle_response_refer(struct sip_pvt *p, int resp, char *rest, struct sip_request *req, int seqno)
{
	char *auth = "Proxy-Authenticate";
	char *auth2 = "Proxy-Authorization";
	char iabuf[INET_ADDRSTRLEN];
	struct ast_channel *owner = p->owner;

	switch (resp) {
	case 202:   /* Transfer accepted */
		/* We need  to do something here */
		/* The transferee is now sending INVITE to target */
		/* Now wait for next message */
		if (option_debug > 2)
			ast_log(LOG_DEBUG, "Got 202 accepted on transfer\n");
		/* We should hang along, waiting for NOTIFY's here */
		break;

	case 401:   /* Not www-authorized on SIP method */
	case 407:   /* Proxy auth */
		if (ast_strlen_zero(p->authname)) {
			ast_log(LOG_WARNING, "Asked to authenticate REFER to %s:%d but we have no matching peer or realm auth!\n",
				ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr), ntohs(p->recv.sin_port));
			ast_set_flag(p, SIP_NEEDDESTROY);
		}
		if (resp == 401) {
			auth = "WWW-Authenticate";
			auth2 = "Authorization";
		}
		if ((p->authtries > 1) || do_proxy_auth(p, req, auth, auth2, SIP_REFER, 0)) {
			ast_log(LOG_NOTICE, "Failed to authenticate on REFER to '%s'\n", get_header(&p->initreq, "From"));
			ast_set_flag(p, SIP_NEEDDESTROY);
		}
		break;

	case 405:
	case 500:   /* Server error */
	case 501:   /* Method not implemented */
		/* Return to the current call onhold */
		/* Status flag needed to be reset */
		ast_log(LOG_NOTICE, "SIP transfer to %s failed, call miserably fails. \n", p->refer_to);
		ast_set_flag(p, SIP_NEEDDESTROY);
		break;
	case 603:   /* Transfer declined */
		ast_log(LOG_NOTICE, "SIP transfer to %s declined, call miserably fails. \n", p->refer_to);
		ast_set_flag(p, SIP_NEEDDESTROY);
		break;
	}

	/* JUNGO: Unless we're required to send another REFER, we're no longer
	 * needed in this call. */
	if (owner && resp != 401 && resp != 407)
		ast_softhangup(owner, AST_SOFTHANGUP_DEV);
}

/*--- handle_response_subscribe: Handle responses on SUBSCRIBE to services ---*/
static int handle_response_subscribe(struct sip_pvt *p, int resp, char *rest, struct sip_request *req, int ignore, int seqno)
{
	struct sip_subscription *s;
	s=p->subscription;

	switch (resp) {
	case 401:	/* Unauthorized */
		if ((p->authtries > 1) || do_subscribe_auth(p, req, "WWW-Authenticate", "Authorization")) {
			ast_log(LOG_NOTICE, "Failed to authenticate on SUBSCRIBE to '%s@%s' (Tries %d)\n", p->subscription->username, p->subscription->hostname, p->authtries);
			ast_set_flag(p, SIP_NEEDDESTROY);	
			}
		break; 
	case 403:	/* Forbidden */
		ast_log(LOG_WARNING, "Forbidden - wrong password on authentication for SUBSCRIBE for '%s' to '%s'\n", p->subscription->username, p->subscription->hostname);
		p->subscription->subattempts = global_subattempts_max+1;
		ast_sched_del(sched, s->timeout);
		ast_set_flag(p, SIP_NEEDDESTROY);	
		break; 
	case 404:	/* Not found */
		ast_log(LOG_WARNING, "Got 404 Not found on SIP subscribe to service %s@%s, giving up\n", p->subscription->username,p->subscription->hostname);
		p->subscription->subattempts = global_subattempts_max+1;
		ast_set_flag(p, SIP_NEEDDESTROY);	
		s->call = NULL;
		ast_sched_del(sched, s->timeout);
		break; 
	case 407:	/* Proxy auth */
	    	if ((p->authtries > 1) || do_subscribe_auth(p, req, "Proxy-Authenticate", "Proxy-Authorization")) {
			ast_log(LOG_NOTICE, "Failed to authenticate on SUBSCRIBE to '%s' (tries '%d')\n", get_header(&p->initreq, "From"), p->authtries);
			ast_set_flag(p, SIP_NEEDDESTROY);	
		}
		break; 
/*	case 479:	 SER: Not able to process the URI - address is wrong in register XXX see if i need it at all*/
/*		ast_log(LOG_WARNING, "Got error 479 on register to %s@%s, giving up (check config)\n", p->registry->username,p->registry->hostname);
		p->registry->regattempts = global_regattempts_max+1;
		ast_set_flag(p, SIP_NEEDDESTROY);	
		r->call = NULL;
		ast_sched_del(sched, r->timeout);
		break; */
	case 200:	/* 200 OK */
		if (!s) {
			ast_log(LOG_WARNING, "Got 200 OK on SUBSCRIBE that isn't a subscribe\n");
			ast_set_flag(p, SIP_NEEDDESTROY);	
			return 0;
		}
		int expires, expires_ms;
		set_pvt_allowed_methods(p, req);

		s->substate=SUB_STATE_SUBSCRIBED;
		manager_event(EVENT_FLAG_SYSTEM, "Subscription", "Channel: SIP\r\nDomain: %s\r\nStatus: %s\r\n", s->hostname, substate2str(s->substate));
		ast_log(LOG_DEBUG, "Subscription successful\n");
		if (s->timeout > -1) {
			ast_log(LOG_DEBUG, "Cancelling timeout %d\n", s->timeout);
			ast_sched_del(sched, s->timeout);
		}
		s->timeout=-1;
		s->call = NULL;
		p->subscription = NULL;
		/* Let this one hang around until we have all the responses */
		sip_scheddestroy(p, 32000);
		/* ast_set_flag(p, SIP_NEEDDESTROY);	*/

		/* set us up for re-subscribing */
		/* figure out how long we got subscribed for */
		if (s->expire > -1)
			ast_sched_del(sched, s->expire);
		expires = 0;
		/* Unlike register, we get the expiration period of this dialog
		 * only from the Expires header in the response. the expires
		 * field that we have in the Contact header is not relevent */
		if (!expires) 
			expires=atoi(get_header(req, "Expires"));
		if (!expires)
			expires=default_expiry;

		expires_ms = expires * 1000;
		if (expires <= EXPIRY_GUARD_LIMIT)
			expires_ms -= MAX((expires_ms * (100 - expires_renew_percentage) / 100),EXPIRY_GUARD_MIN);
		else
			expires_ms -= EXPIRY_GUARD_SECS * 1000;
		if (sipdebug)
			ast_log(LOG_NOTICE, "Outbound Subscription: Expiry for %s is %d sec (Scheduling resubscription in %d ms)\n", s->hostname, expires, expires_ms); 

		s->refresh= (int) expires_ms / 1000;

		/* Schedule re-subscription before we expire */
		s->expire=ast_sched_add(sched, expires_ms, sip_resubscribe, s); 
		ASTOBJ_UNREF(s, sip_subscription_destroy);
		/* response code to handle (?):
		 * 202 
		 * 4XX (also 481)
		 * */
	}
	return 1;
}

static void get_host_and_port_from_contact(char *contact, char *host, int size, int *port)
{
	char *tmpcontact = ast_strdupa(contact);
	char *n, *pt;

	/* Ditch arguments */
	n = strchr(tmpcontact, ';');
	if (n) 
		*n = '\0';

	/* Grab host */
	n = strchr(tmpcontact, '@');
	if (!n) 
		n = tmpcontact;
	else {
		*n = '\0';
		n++;
	}

	/* Grab Port */
	pt = strchr(n, ':');
	if (pt) {
		*pt = '\0';
		pt++;
		*port = atoi(pt);
	} else {
		/* Clean '>' suffix from URI */
		pt = strchr(n, '>');
		if (pt)
			*pt = '\0';
		*port = DEFAULT_SIP_PORT;
	}

	strncpy(host, n, size);
}

static void reg_add_exiting_bindings(struct sip_request *req, struct sip_pvt *p, struct sip_registry *r) 
{
	int start = 0;
	char *contact;

	while (!ast_strlen_zero((contact = __get_header(req, "Contact", &start))))
	{
		int port;
		char host[256];

		get_host_and_port_from_contact(contact, host, sizeof(host), &port);
		contact_list_add_entry(&r->contact_list, host, port);
	}
}

static void handle_response_register_resched(int resp, char *rest, struct sip_request *req, struct sip_pvt *p, struct sip_registry *r)
{
	int retry_time = -1;
	int (*func)(void *) = NULL;
	/* Vodafone Italy: trigger proxy failover if there is no valid 
	 * "Retry-After" header and we got any of the following responses
	 * on REGISTER: 403, 404, 408, 48X, 5XX */
	int proxy_failover_required = resp != 413 && resp != 600 && resp != 603;

	/* Cancel existing timer only if we are going to reschedule */
	if ((retry_time = parse_retry_after(req)) > -1 || proxy_failover_required) {
		SCHED_CANCEL(r->timeout);
		ast_set_flag(p, SIP_NEEDDESTROY);
		r->call = NULL;
	}

	if (retry_time > -1) {
		/* Got a Retry-After header. Don't count this attempt in the 
		 * regattempts counter and regard our state as "unregistered".
		 */
		r->regstate = REG_STATE_UNREGISTERED;
		retry_time *= 1000; /* ms */
		func = sip_timeout_redoregister;
		goto Exit;
	}

	r->regstate = REG_STATE_REJECTED;

	/* Vodafone Italy requirements (failover to secondary server) */
	if (proxy_failover_required) {
		if (resp == 403 && r->srv_failover) {
			sip_reg_timeout(r);
			/* sip_reg_timeout() sets server_na, but this is not the
			 * case for us */
			set_server_na(r, 0);
			goto Exit;
		}

		if (global_regattempts_max)
			p->registry->regattempts = global_regattempts_max+1;
		if (r->reg_backup || r->reg_primary) {
			retry_time = 1; /* ms */
			func = proxy_failover_timeout;
		}

		goto Exit;
	}

Exit:
	if (retry_time > -1)
		r->timeout = ast_sched_add(sched, retry_time, func, r);
	manager_event_sip_registry();
}

static int handle_response_register_fetch(struct sip_pvt *p, struct sip_request *req, struct sip_registry *r)
{
	switch(r->fetch_state)
	{
	case FETCH_BINDING_STATE_QUERY:
		reg_add_exiting_bindings(req, p, r);

		if (!r->contact_list)
			r->fetch_state = FETCH_BINDING_STATE_UNBOUND;
		else
			r->fetch_state = FETCH_BINDING_STATE_UNREGISTER;

		r->regstate = REG_STATE_UNREGISTERED;
		transmit_register(r, SIP_REGISTER, NULL, NULL, 0);
		return 1;

	case FETCH_BINDING_STATE_UNREGISTER:
		r->fetch_state = FETCH_BINDING_STATE_UNBOUND;
		r->regstate = REG_STATE_UNREGISTERED;
		manager_event_sip_registry();
		r->expiry = default_expiry;
		contact_list_free(&r->contact_list);
		transmit_register(r, SIP_REGISTER, NULL, NULL, 0);
		return 1;

	default:
		return 0;
	}
}

/*! \brief  handle_response_register: Handle responses on REGISTER to services ---*/
static int handle_response_register(struct sip_pvt *p, int resp, char *rest, struct sip_request *req, int ignore, int seqno)
{
	int expires, expires_ms, down_time_reset = 0;
	struct sip_registry *r;
	r=p->registry;
	set_server_na(r, 0);
	struct sockaddr_in sa;

	switch (resp) {
	case 401:	/* Unauthorized */
		if ((p->authtries == MAX_AUTHTRIES) || do_register_auth(p, req, "WWW-Authenticate", "Authorization")) {
			ast_log(LOG_NOTICE, "Failed to authenticate on REGISTER to '%s@%s' (Tries %d)\n", p->registry->username, p->registry->hostname, p->authtries);
			ast_set_flag(p, SIP_NEEDDESTROY);	
			}
		r->regstate = REG_STATE_REJECTED;
		manager_event_sip_registry();
		break;
	case 403:	/* Forbidden */
	case 404:	/* Not found */
	case 413:	/* Request Entity Too Large */
                down_time_reset = 1;
		/* Fallthrough */
	case 408:	/* Request Timeout */
	case 480:	/* Temporarily Unavailable */
	case 481:	/* Call/Transaction Does Not Exist */
	case 482:	/* Loop Detected */
	case 483:	/* Too Many Hops */
	case 484:	/* Address incomplete */
	case 485:	/* Ambiguous */
	case 486:	/* Busy here */
	case 487:	/* Call cancelled */
	case 488:	/* Not Acceptable Here */
	case 489:	/* Bad Event */
	case 500:	/* Server Internal Error */
	case 501:	/* Not Implemented */
	case 502:	/* Bad Gateway */
	case 503:	/* Service Unavailable */
	case 504:	/* Server Time-out */
	case 505:	/* Version Not Supported */
	case 513:	/* Message Too Large */
	case 580:	/* Precondition Failure */
	case 600:	/* Busy everywhere */
	case 603:	/* Declined transfer */
		ast_log(LOG_WARNING, "Got error %d (%s) on register to %s@%s\n", resp, rest, p->registry->username, p->registry->hostname);
		sip_peer_server_down_time_calc(r, down_time_reset);
		handle_response_register_resched(resp, rest, req, p, r);
		break;
	case 407:	/* Proxy auth */
		if ((p->authtries == MAX_AUTHTRIES) || do_register_auth(p, req, "Proxy-Authenticate", "Proxy-Authorization")) {
			ast_log(LOG_NOTICE, "Failed to authenticate on REGISTER to '%s' (tries '%d')\n", get_header(&p->initreq, "From"), p->authtries);
			ast_set_flag(p, SIP_NEEDDESTROY);	
		}
		r->regstate = REG_STATE_REJECTED;
		manager_event_sip_registry();
		break;
	case 200:	/* 200 OK */
		if (!r) {
			ast_log(LOG_WARNING, "Got 200 OK on REGISTER that isn't a register\n");
			ast_set_flag(p, SIP_NEEDDESTROY);	
			return 0;
		}
		SCHED_CANCEL(r->timeout);
		r->call = NULL;
		p->registry = NULL;
		sa = p->sa;
		/* Let this one hang around until we have all the responses */
		sip_scheddestroy(p, 32000);
		/* ast_set_flag(p, SIP_NEEDDESTROY);	*/

		/* set us up for re-registering */
		/* figure out how long we got registered for */
		SCHED_CANCEL(r->expire);
		parse_ok_authinfo(r, req);
		/* Found a decent proxy, mark it as healthy */
		if (r->srv_failover)
			r->uas_srv.healthy = 1;

		if (unregister_existing_bindings && r->ip_changed && handle_response_register_fetch(p, req, r))
		{
			ASTOBJ_UNREF(r, sip_registry_destroy);
			break;
		}

		/* handle un-register */
		if (r->expiry == 0)
		{
			r->regstate = REG_STATE_UNREGISTERED;
			manager_event_sip_registry();
			r->expiry = default_expiry;
		        if (!r->reg_primary)
			{
			    ast_log(LOG_DEBUG, "Recieved OK for un-REGISTER "
			        "to primary proxy.\n");
			    ASTOBJ_UNREF(r, sip_registry_destroy);
			    break;
			}

			ast_log(LOG_DEBUG, "Received OK for un-REGISTER to "
				"backup proxy. Schedule reregister to primary proxy "
				"in %dms\n", global_vf_1s_delay_dereg_backup);

			/* unregistred from backup. register to primary proxy. */
			SCHED_CANCEL(r->reg_primary->expire);
			r->reg_primary->expire = ast_sched_add(sched, global_vf_1s_delay_dereg_backup, 
				sip_expire_redoregister, r->reg_primary); 
			ASTOBJ_UNREF(r, sip_registry_destroy);
			break;
		}
		r->regstate = REG_STATE_REGISTERED;
		r->ip_changed = 0;
		manager_event_sip_registry();
		sip_peer_server_down_time_calc(r, 1);		
		r->regattempts = 0;
		ast_log(LOG_DEBUG, "Registration successful\n");

		/* according to section 6.13 of RFC, contact headers override
		   expires headers, so check those first */
		expires = 0;
		if (!ast_strlen_zero(get_header(req, "Contact"))) {
			char *contact = NULL;
			char *tmptmp = NULL;
			int start = 0;
			for(;;) {
				contact = __get_header(req, "Contact", &start);
				/* this loop ensures we get a contact header about our register request */
				if(!ast_strlen_zero(contact)) {
					char *tmpcontact = NULL;

					/* If the proxy doesn't omit the default SIP port, normalize
					 * contact address before comparing to our own contact address */
					tmpcontact = ast_strdupa(contact);
					if ((tmptmp = strstr(tmpcontact, ":5060")))
						*tmptmp = '>';

					if( (tmptmp=strstr(tmpcontact, p->our_contact))) {
						contact=tmptmp;
						break;
					}
				} else
					break;
			}
			tmptmp = strcasestr(contact, "expires=");
			if (tmptmp) {
				if (sscanf(tmptmp + 8, "%d;", &expires) != 1)
					expires = 0;
			}

		}

		if (r->reg_primary || r->reg_backup)
			update_peer_from_registry(r, sa);

		if (r->reg_primary && r->reg_primary->sched_recover_primary == -1)
		{
			/* On first success registration to backup proxy, set the 
			   recovery T2 timer. */
			r->reg_primary->sched_recover_primary = ast_sched_add(sched, 
				global_vf_t2_delay_recover_primary, recover_to_primary,
				r->reg_primary); 
			ast_log(LOG_DEBUG, "Schedule recovery to Primary Proxy: " 
				"'%s@%s' in %dms.\n", r->reg_primary->username, 
				r->reg_primary->hostname, global_vf_t2_delay_recover_primary);
		}
		if (r->reg_backup && r->reg_backup->is_backup_active)
		{
			/* If this is 200 OK for a recovery attempt to primary proxy,
			   hold back on primary proxy, stop the backup, and schedule 
			   De-Register to backup in 1S */
			ast_log(LOG_DEBUG, "Primary proxy is up again (200"
				" OK received for recovery). Schedule un-REGISTER to "
				"backup proxy in %dms\n", 
				global_vf_1s_delay_dereg_backup);

			r->reg_backup->is_backup_active = 0;
			reg_cancel_call(r->reg_backup);
			r->reg_backup->regstate = REG_STATE_UNREGISTERED;
		        manager_event_sip_registry();
			SCHED_CANCEL(r->reg_backup->timeout);
			SCHED_CANCEL(r->reg_backup->expire);
			r->reg_backup->expiry = 0;
			r->reg_backup->expire = ast_sched_add(sched, 
				global_vf_1s_delay_dereg_backup,
				sip_expire_redoregister, r->reg_backup); 
		}
		else
		{
			if (!expires) 
				expires=atoi(get_header(req, "expires"));
			if (!expires)
				expires=r->expiry;

			expires_ms = expires * 1000;
			expires_ms -= MAX((expires_ms * (100 - expires_renew_percentage) / 100),EXPIRY_GUARD_MIN);
			if (sipdebug)
				ast_log(LOG_NOTICE, "Outbound Registration: Expiry for %s is %d sec (Scheduling reregistration in %d s)\n", r->hostname, expires, expires_ms/1000); 

			r->refresh= (int) expires_ms / 1000;

			/* Schedule re-registration before we expire */
			r->expire=ast_sched_add(sched, r->reg_period ?
			    r->reg_period * 1000 : expires_ms,
			    sip_expire_redoregister, r); 
		}
		ASTOBJ_UNREF(r, sip_registry_destroy);
	}
	return 1;
}

/*! \brief  handle_response_peerpoke: Handle qualification responses (OPTIONS) */
static int handle_response_peerpoke(struct sip_pvt *p, int resp, char *rest, struct sip_request *req, int ignore, int seqno, int sipmethod)
{
	struct sip_peer *peer;
	int pingtime;
	struct timeval tv;

	if (resp != 100) {
		int statechanged = 0;
		int newstate = 0;
		peer = p->peerpoke;
		tv = ast_tvfromboot();
		pingtime = ast_tvdiff_ms(tv, peer->ps);
		if (pingtime < 1)
			pingtime = 1;
		if ((peer->lastms < 0)  || (peer->lastms > peer->maxms)) {
			if (pingtime <= peer->maxms) {
				ast_log(LOG_NOTICE, "Peer '%s' is now REACHABLE! (%dms / %dms)\n", peer->name, pingtime, peer->maxms);
				statechanged = 1;
				newstate = 1;
			}
		} else if ((peer->lastms > 0) && (peer->lastms <= peer->maxms)) {
			if (pingtime > peer->maxms) {
				ast_log(LOG_NOTICE, "Peer '%s' is now TOO LAGGED! (%dms / %dms)\n", peer->name, pingtime, peer->maxms);
				statechanged = 1;
				newstate = 2;
			}
		}
		if (!peer->lastms)
			statechanged = 1;
		peer->lastms = pingtime;
		peer->call = NULL;
		if (statechanged) {
			ast_device_state_changed("SIP/%s", peer->name);
			if (newstate == 2) {
				manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Lagged\r\nTime: %d\r\n", peer->name, pingtime);
			} else {
				manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Reachable\r\nTime: %d\r\n", peer->name, pingtime);
			}
		}

		if (peer->pokeexpire > -1)
			ast_sched_del(sched, peer->pokeexpire);
		if (sipmethod == SIP_INVITE)	/* Does this really happen? */
			transmit_request(p, SIP_ACK, seqno, 0, 0);
		ast_set_flag(p, SIP_NEEDDESTROY);	

		/* Try again eventually */
		if ((peer->lastms < 0)  || (peer->lastms > peer->maxms))
			peer->pokeexpire = ast_sched_add(sched, DEFAULT_FREQ_NOTOK, sip_poke_peer_s, peer);
		else
			peer->pokeexpire = ast_sched_add(sched, DEFAULT_FREQ_OK, sip_poke_peer_s, peer);
	}
	return 1;
}

/*! \brief  handle_response: Handle SIP response in dialogue ---*/
static void handle_response(struct sip_pvt *p, int resp, char *rest, struct sip_request *req, int ignore, int seqno)
{
	char *msg, *c;
	struct ast_channel *owner;
	char iabuf[INET_ADDRSTRLEN];
	int sipmethod;
	int res = 1;

	c = get_header(req, "Cseq");
	msg = strchr(c, ' ');
	if (!msg)
		msg = "";
	else
		msg++;
	sipmethod = find_sip_method(msg);

	owner = p->owner;
	if (owner) 
		owner->hangupcause = hangup_sip2cause(resp);

	/* Acknowledge whatever it is destined for */
	if ((resp >= 100) && (resp <= 199))
		res = __sip_semi_ack(p, seqno, 0, sipmethod);
	else
		res = __sip_ack(p, seqno, 0, sipmethod);
	
	if (res == -1)
	{
		/* RFC 3261 13.2.2.4 and 17.1.1.2 - We must re-send ACKs to re-transmitted final responses */
		if (sipmethod == SIP_INVITE && resp >= 200) 
			transmit_request(p, SIP_ACK, seqno, 0, resp < 300);

		ast_log(LOG_WARNING, "Ignoring retransmit response\n");
		return;
	}

	/* Get their tag if we haven't already */
	if ((ast_strlen_zero(p->theirtag) || (resp >= 200)) && sipmethod != SIP_CANCEL) {
		gettag(req, "To", p->theirtag, sizeof(p->theirtag));
	}
	if (p->peerpoke) {
		/* We don't really care what the response is, just that it replied back. 
		   Well, as long as it's not a 100 response...  since we might
		   need to hang around for something more "definitive" */

		res = handle_response_peerpoke(p, resp, rest, req, ignore, seqno, sipmethod);
	} else if (ast_test_flag(p, SIP_OUTGOING)) {
		/* Acknowledge sequence number */
		if (p->initid > -1) {
			/* Don't auto congest anymore since we've gotten something useful back */
			ast_sched_del(sched, p->initid);
			p->initid = -1;
		}
		switch(resp) {
		case 100:	/* 100 Trying */
			if (sipmethod == SIP_INVITE) 
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			break;
		case 183:	/* 183 Session Progress */
			if (sipmethod == SIP_INVITE) 
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			break;
		case 180:	/* 180 Ringing */
		case 181:
			if (sipmethod == SIP_INVITE) 
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			break;
		case 200:	/* 200 OK */
			p->authtries = 0;	/* Reset authentication counter */
			if (sipmethod == SIP_MESSAGE) {
				/* We successfully transmitted a message */
				ast_set_flag(p, SIP_NEEDDESTROY);	
			} else if (sipmethod == SIP_NOTIFY) {
				/* They got the notify, this is the end */
				if (p->owner) {
					ast_log(LOG_WARNING, "Notify answer on an owned channel?\n");
					ast_queue_hangup(p->owner);
				} else {
					if (p->subscribed == NONE) {
						ast_set_flag(p, SIP_NEEDDESTROY); 
					}
				}
			} else if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_REGISTER) {
				res = handle_response_register(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_BYE) {	
				ast_set_flag((&p->flags_page2), SIP_NEEDDESTROY);
				ast_clear_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
			} else if (sipmethod == SIP_SUBSCRIBE) {
			        ast_set_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
				res = handle_response_subscribe(p, resp, rest, req, ignore, seqno);
			}
			break;
		case 202:   /* Transfer accepted */
			if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else if (sipmethod == SIP_MESSAGE) {
				/* We successfully transmitted a message */
				ast_set_flag(p, SIP_NEEDDESTROY);	
			}
			break;
		case 400: /* Bad request */
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			}
			break;
		case 401: /* Not www-authorized on SIP method */
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else if (p->registry && sipmethod == SIP_REGISTER) {
				res = handle_response_register(p, resp, rest, req, ignore, seqno);
			} else if (p->subscription && sipmethod == SIP_SUBSCRIBE) {
				res = handle_response_subscribe(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_BYE || 
			    sipmethod == SIP_PRACK) {
				if (ast_strlen_zero(p->authname)) {
					ast_log(LOG_WARNING, "Asked to authenticate %s, to %s:%d but we have no matching peer!\n",
							msg, ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr), ntohs(p->recv.sin_port));
					ast_set_flag(p, SIP_NEEDDESTROY);	
				} else if ((p->authtries == MAX_AUTHTRIES) || do_proxy_auth(p, req, "WWW-Authenticate", "Authorization", sipmethod, 0)) {
					ast_log(LOG_NOTICE, "Failed to authenticate on %s to '%s'\n", msg, get_header(&p->initreq, "From"));
					ast_set_flag(p, SIP_NEEDDESTROY);	
				}
			} else {
				ast_log(LOG_WARNING, "Got authentication request (401) on unknown %s to '%s'\n", sip_methods[sipmethod].text, get_header(req, "To"));
				ast_set_flag(p, SIP_NEEDDESTROY);	
			}
			break;
		case 403: /* Forbidden - we failed authentication */
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else if (p->registry && sipmethod == SIP_REGISTER) {
				res = handle_response_register(p, resp, rest, req, ignore, seqno);
			} else {
				ast_log(LOG_WARNING, "Forbidden - wrong password on authentication for %s\n", msg);
			}
			break;
		case 404: /* Not found */
			if (p->registry && sipmethod == SIP_REGISTER) {
				res = handle_response_register(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (owner)
			    ast_queue_control(p->owner, AST_CONTROL_UNALLOCATED);
			break;
		case 407: /* Proxy auth required */
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else if (sipmethod == SIP_BYE || 
			    sipmethod == SIP_PRACK) {
				if (ast_strlen_zero(p->authname)) {
					ast_log(LOG_WARNING, "Asked to authenticate %s, to %s:%d but we have no matching peer!\n",
							msg, ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr), ntohs(p->recv.sin_port));
					ast_set_flag(p, SIP_NEEDDESTROY);	
				} else if ((p->authtries == MAX_AUTHTRIES) || do_proxy_auth(p, req, "Proxy-Authenticate", "Proxy-Authorization", sipmethod, 0)) {
					ast_log(LOG_NOTICE, "Failed to authenticate on %s to '%s'\n", msg, get_header(&p->initreq, "From"));
					ast_set_flag(p, SIP_NEEDDESTROY);	
				}
			} else if (p->registry && sipmethod == SIP_REGISTER) {
				res = handle_response_register(p, resp, rest, req, ignore, seqno);
			} else if (p->subscription && sipmethod == SIP_SUBSCRIBE) {
				res = handle_response_subscribe(p, resp, rest, req, ignore, seqno);
			} else	/* We can't handle this, giving up in a bad way */
				ast_set_flag(p, SIP_NEEDDESTROY);	

			break;
		case 408:	/* Request Timeout */
		case 481:	/* Call leg does not exist */
			if (sipmethod == SIP_INVITE || sipmethod == SIP_UPDATE)
			{	/* Re-invite failed */
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			}
			else if (p->registry && sipmethod == SIP_REGISTER)
			{
				handle_response_register(p, resp, rest, req, ignore, seqno);
			}
			else
			{			
				if (owner)
					ast_queue_hangup(p->owner);
				else
					ast_set_flag(p, SIP_NEEDDESTROY);
			}
			break;	
		case 422: /* Session-Timers: Session Interval Too Small */
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			}
			break;
		case 480: /* Temporarily Unavailable */
		case 484: /* Address incomplete */
			if (p->registry && sipmethod == SIP_REGISTER) {
				handle_response_register(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (owner)
				ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
			break;
		case 487: /* Call cancelled */
			if (p->registry && sipmethod == SIP_REGISTER) {
				handle_response_register(p, resp, rest, req, ignore, seqno);
				break;
			}
			/* Fallthrough */
		case 491: /* Pending */
			if (sipmethod == SIP_INVITE)
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			break;
		case 501: /* Not Implemented */
			if (p->registry && sipmethod == SIP_REGISTER) {
				handle_response_register(p, resp, rest, req, ignore, seqno);
				break;
			}
			/* Fallthrough */
		case 405:
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else
				ast_log(LOG_WARNING, "Host '%s' does not implement '%s'\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr), msg);
			break;
		case 600:	/* Busy everywhere */
		case 603:	/* Declined transfer */
			if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
				break;
			}
			/* Fallthrough */
		case 413:	/* Request entity too large */
		case 486:	/* Busy here */
		case 488:	/* Not Acceptable Here */
		case 500:	/* Server internal error */
		case 513:	/* Message Too Large */
			if (sipmethod == SIP_INVITE || sipmethod == SIP_UPDATE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
				break;
			}
			/* Fallthrough */
		case 482:	/* Loop Detected */
		case 483:	/* Too Many Hops */
		case 485:	/* Ambiguous */
		case 489:	/* Bad Event */
		case 502:	/* Bad Gateway */
		case 503:	/* Service unavailable */
		case 504:	/* Server Time-out */
		case 505:	/* Version Not Supported */
		case 580:	/* Precondition Failure */
			if (p->registry && sipmethod == SIP_REGISTER)
			{
				handle_response_register(p, resp, rest, req, ignore, seqno);
				break;
			}
			/* Fallthrough */
		default:
			if ((resp >= 300) && (resp < 700)) {
				if ((option_verbose > 2) && (resp != 487))
					ast_verbose(VERBOSE_PREFIX_3 "Got SIP response %d \"%s\" back from %s\n", resp, rest, ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr));
				ast_set_flag(p, SIP_ALREADYGONE);	
				if (p->rtp) {
					/* Immediately stop RTP */
					ast_rtp_stop(p->rtp);
				}
				if (p->vrtp) {
					/* Immediately stop VRTP */
					ast_rtp_stop(p->vrtp);
				}
#if defined(T38_SUPPORT)
				if (p->udptl) {
					/* Immediately stop T.38 UDPTL */
					ast_udptl_stop(p->udptl);
				}
#endif
				/* XXX Locking issues?? XXX */
				switch(resp) {
				case 300: /* Multiple Choices */
				case 301: /* Moved permenantly */
				case 302: /* Moved temporarily */
				case 305: /* Use Proxy */
					parse_moved_contact(p, req);
					/* Fall through */
				case 600: /* Busy everywhere */
					if (p->owner)
						ast_queue_control(p->owner, AST_CONTROL_BUSY);
					break;
				case 482: /* SIP is incapable of performing a hairpin call, which
				             is yet another failure of not having a layer 2 (again, YAY
							 IETF for thinking ahead).  So we treat this as a call
							 forward and hope we end up at the right place... */
					ast_log(LOG_DEBUG, "Hairpin detected, setting up call forward for what it's worth\n");
					if (p->owner)
						snprintf(p->owner->call_forward, sizeof(p->owner->call_forward), "Local/%s@%s", p->username, p->context);
					/* Fall through */
				case 488: /* Not acceptable here - codec error */
				case 404: /* Not Found */
				case 410: /* Gone */
				case 400: /* Bad Request */
				case 500: /* Server error */
					if (sipmethod == SIP_REFER) {
						handle_response_refer(p, resp, rest, req, seqno);
						break;
					}
					/* Fall through */
				case 503: /* Service Unavailable */
					if (owner)
					{
						sip_send_transparent_error_response(p, req);

						ast_queue_control(p->owner, AST_CONTROL_CONGESTION);
					}
					break;
				case 603: /* Decline */
					if (owner)
						ast_queue_control(p->owner, AST_CONTROL_DECLINE);
					break;
				default:
					/* Send hangup */	
					if (owner)
						ast_queue_hangup(p->owner);
					break;
				}
				/* ACK on invite */
				if (sipmethod == SIP_INVITE) 
					transmit_request(p, SIP_ACK, seqno, 0, 0);
				ast_set_flag(p, SIP_ALREADYGONE);	
				if (!p->owner)
					ast_set_flag(p, SIP_NEEDDESTROY);	
			} else if ((resp >= 100) && (resp < 200)) {
				if (sipmethod == SIP_INVITE) {
					if (!ast_test_flag(&p->flags_page2, SIP_PAGE2_INVITECANCELLED))
						sip_cancel_destroy(p);
					if (!ast_strlen_zero(get_header(req, "Content-Type")))
						process_sdp(p, req);
					if (p->owner) {
						/* Queue a progress frame */
						ast_queue_control_data(p->owner, AST_CONTROL_PROGRESS, &p->formats, sizeof(p->formats));
					}
				}
			} else
				ast_log(LOG_NOTICE, "Dont know how to handle a %d %s response from %s\n", resp, rest, p->owner ? p->owner->name : ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr));
		}
		/* Acknowledge whatever it is destined for */
		if ((resp >= 100) && (resp <= 199))
		{
			char *require = get_header(req, "Require");

			/* Is this a reliable provisional response? */
			if (strcasestr(require, "100rel") && (resp != 100))
			{
				/* We now know that the UAS supports PRACK, and has chosen to
				 * use reliable provisional responses (either because the UAC
				 * requires them, or because they are supported by both and the
				 * reliability is a Good Thing. We now expect to receive
				 * reliable provisional responses instead of provisional
				 * responses. */
				char *rseq_header = get_header(req, "RSeq"), *dummy;
				unsigned int rseq_no = strtoul(rseq_header, &dummy, 10);

				p->prack_level = PRACK_LEVEL_REQUIRE;

				if (rseq_no != 0) /* RSeq is not malformed */
				{
					/* Is this the first reliable response? */
					if (p->prack_expected_rseq == 0)
						p->prack_expected_rseq = rseq_no;

					/* Is this the expected response? */
					if (p->prack_expected_rseq == rseq_no)
					{
						++p->prack_expected_rseq;
						p->prack_rack = rseq_no;
						transmit_prack(p, 0, req);
					}
				}
			}
		}
	} else {	
		/* Responses to OUTGOING SIP requests on INCOMING calls 
		   get handled here. As well as out-of-call message responses */
		if (req->debug)
			ast_verbose("SIP Response message for INCOMING dialog %s arrived\n", msg);
		if (resp == 200) {
			/* Tags in early session is replaced by the tag in 200 OK, which is 
		  	the final reply to our INVITE */
			gettag(req, "To", p->theirtag, sizeof(p->theirtag));
		}

		switch(resp) {
		case 200:
			if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			} else if (sipmethod == SIP_CANCEL) {
				ast_log(LOG_DEBUG, "Got 200 OK on CANCEL\n");
			} else if (sipmethod == SIP_MESSAGE)
				/* We successfully transmitted a message */
				ast_set_flag(p, SIP_NEEDDESTROY);	
			break;
		case 202:   /* Transfer accepted */
			if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else if (sipmethod == SIP_MESSAGE) {
				/* We successfully transmitted a message */
				ast_set_flag(p, SIP_NEEDDESTROY);	
			}
			break;
		case 401:	/* www-auth */
		case 407:
			if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
			} else if (sipmethod == SIP_BYE || sipmethod == SIP_PRACK) {
				char *auth, *auth2;

				if (resp == 407) {
					auth = "Proxy-Authenticate";
					auth2 = "Proxy-Authorization";
				} else {
					auth = "WWW-Authenticate";
					auth2 = "Authorization";
				}
				if ((p->authtries == MAX_AUTHTRIES) || do_proxy_auth(p, req, auth, auth2, sipmethod, 0)) {
					ast_log(LOG_NOTICE, "Failed to authenticate on %s to '%s'\n", msg, get_header(&p->initreq, "From"));
					ast_set_flag(p, SIP_NEEDDESTROY);	
				}
			} else if (sipmethod == SIP_INVITE) {
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			}
			break;
		case 408:
		case 481:	/* Call leg does not exist */
			if (sipmethod == SIP_INVITE || sipmethod == SIP_UPDATE) {
				/* Re-invite failed */
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			}
			break;
		case 405:
		case 501: /* Not Implemented */
			if (sipmethod == SIP_INVITE) 
				handle_response_invite(p, resp, rest, req, ignore, seqno);
			else if (sipmethod == SIP_REFER) 
				handle_response_refer(p, resp, rest, req, seqno);
			break;
		case 603:	/* Declined transfer */
			if (sipmethod == SIP_REFER) {
				handle_response_refer(p, resp, rest, req, seqno);
				break;
			}
			/* Fallthrough */
		default:	/* Errors without handlers */
			if ((resp >= 100) && (resp < 200)) {
				if (sipmethod == SIP_INVITE) {	/* re-invite */
					sip_cancel_destroy(p);
				}
			}
			if ((resp >= 300) && (resp < 700)) {
				if ((option_verbose > 2) && (resp != 487))
					ast_verbose(VERBOSE_PREFIX_3 "Incoming call: Got SIP response %d \"%s\" back from %s\n", resp, rest, ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr));
				switch(resp) {
				case 488: /* Not acceptable here - codec error */
				case 603: /* Decline */
				case 500: /* Server error */
				case 503: /* Service Unavailable */

					if (sipmethod == SIP_INVITE) {	/* re-invite failed */
						sip_cancel_destroy(p);
					}
					break;
				}
			}
			break;
		}
	}
}

struct sip_dual {
	struct ast_channel *chan1;
	struct ast_channel *chan2;
	struct sip_request req;
};

/*! \brief  sip_park_thread: Park SIP call support function */
static void *sip_park_thread(void *stuff)
{
	struct ast_channel *chan1, *chan2;
	struct sip_dual *d;
	struct sip_request req;
	int ext;
	int res;
	d = stuff;
	chan1 = d->chan1;
	chan2 = d->chan2;
	copy_request(&req, &d->req);
	free(d);
	ast_mutex_lock(&chan1->lock);
	ast_do_masquerade(chan1);
	ast_mutex_unlock(&chan1->lock);
	res = ast_park_call(chan1, chan2, 0, &ext);
	/* Then hangup */
	ast_hangup(chan2);
	ast_log(LOG_DEBUG, "Parked on extension '%d'\n", ext);
	return NULL;
}

/*! \brief  sip_park: Park a call ---*/
static int sip_park(struct ast_channel *chan1, struct ast_channel *chan2, struct sip_request *req)
{
	struct sip_dual *d;
	struct ast_channel *chan1m, *chan2m;
	pthread_t th;
	chan1m = ast_channel_alloc(0);
	chan2m = ast_channel_alloc(0);
	if ((!chan2m) || (!chan1m)) {
		if (chan1m)
			ast_hangup(chan1m);
		if (chan2m)
			ast_hangup(chan2m);
		return -1;
	}
	snprintf(chan1m->name, sizeof(chan1m->name), "Parking/%s", chan1->name);
	/* Make formats okay */
	chan1m->readformat = chan1->readformat;
	chan1m->writeformat = chan1->writeformat;
	chan1m->ptime = chan1->ptime;
	ast_channel_masquerade(chan1m, chan1);
	/* Setup the extensions and such */
	ast_copy_string(chan1m->context, chan1->context, sizeof(chan1m->context));
	ast_copy_string(chan1m->exten, chan1->exten, sizeof(chan1m->exten));
	chan1m->priority = chan1->priority;
		
	/* We make a clone of the peer channel too, so we can play
	   back the announcement */
	snprintf(chan2m->name, sizeof (chan2m->name), "SIPPeer/%s",chan2->name);
	/* Make formats okay */
	chan2m->readformat = chan2->readformat;
	chan2m->writeformat = chan2->writeformat;
	chan2m->ptime = chan2->ptime;
	ast_channel_masquerade(chan2m, chan2);
	/* Setup the extensions and such */
	ast_copy_string(chan2m->context, chan2->context, sizeof(chan2m->context));
	ast_copy_string(chan2m->exten, chan2->exten, sizeof(chan2m->exten));
	chan2m->priority = chan2->priority;
	ast_mutex_lock(&chan2m->lock);
	if (ast_do_masquerade(chan2m)) {
		ast_log(LOG_WARNING, "Masquerade failed :(\n");
		ast_mutex_unlock(&chan2m->lock);
		ast_hangup(chan2m);
		return -1;
	}
	ast_mutex_unlock(&chan2m->lock);
	d = malloc(sizeof(struct sip_dual));
	if (d) {
		memset(d, 0, sizeof(*d));
		/* Save original request for followup */
		copy_request(&d->req, req);
		d->chan1 = chan1m;
		d->chan2 = chan2m;
		if (!ast_pthread_create(&th, NULL, sip_park_thread, d))
			return 0;
		free(d);
	}
	return -1;
}

/*! \brief  ast_quiet_chan: Turn off generator data */
static void ast_quiet_chan(struct ast_channel *chan) 
{
	if (chan && chan->_state == AST_STATE_UP) {
		if (chan->generatordata)
			ast_deactivate_generator(chan);
	}
}

/*! \brief  attempt_transfer: Attempt transfer of SIP call ---*/
static int attempt_transfer(struct sip_pvt *p1, struct sip_pvt *p2)
{
	int res = 0;
	struct ast_channel 
		*chana = NULL,
		*chanb = NULL,
		*bridgea = NULL,
		*bridgeb = NULL,
		*peera = NULL,
		*peerb = NULL,
		*peerc = NULL,
		*peerd = NULL;

	if (!p1->owner || !p2->owner) {
		ast_log(LOG_WARNING, "Transfer attempted without dual ownership?\n");
		return -1;
	}
	chana = p1->owner;
	chanb = p2->owner;
	bridgea = ast_bridged_channel(chana);
	bridgeb = ast_bridged_channel(chanb);
	
	if (bridgea) {
		peera = chana;
		peerb = chanb;
		peerc = bridgea;
		peerd = bridgeb;
	} else if (bridgeb) {
		peera = chanb;
		peerb = chana;
		peerc = bridgeb;
		peerd = bridgea;
	}
	
	if (peera && peerb && peerc && (peerb != peerc)) {
		ast_quiet_chan(peera);
		ast_quiet_chan(peerb);
		ast_quiet_chan(peerc);
		ast_quiet_chan(peerd);

		if (peera->cdr && peerb->cdr) {
			peerb->cdr = ast_cdr_append(peerb->cdr, peera->cdr);
		} else if (peera->cdr) {
			peerb->cdr = peera->cdr;
		}
		peera->cdr = NULL;

		if (peerb->cdr && peerc->cdr) {
			peerb->cdr = ast_cdr_append(peerb->cdr, peerc->cdr);
		} else if (peerc->cdr) {
			peerb->cdr = peerc->cdr;
		}
		peerc->cdr = NULL;
		
		ast_log(LOG_DEBUG, "XXXX Trying to masquerade %s and %s\n", peerb->name, peerc->name);
		if (ast_channel_masquerade(peerb, peerc)) {
			ast_log(LOG_WARNING, "Failed to masquerade %s into %s\n", peerb->name, peerc->name);
			res = -1;
		}
		return res;
	} else {
		ast_log(LOG_NOTICE, "Transfer attempted with no appropriate bridged calls to transfer\n");
		if (chana)
			ast_softhangup_nolock(chana, AST_SOFTHANGUP_DEV);
		if (chanb)
			ast_softhangup_nolock(chanb, AST_SOFTHANGUP_DEV);
		return -1;
	}
	return 0;
}

/*! \brief  gettag: Get tag from packet */
static char *gettag(struct sip_request *req, char *header, char *tagbuf, int tagbufsize) 
{

	char *thetag, *sep;
	

	if (!tagbuf)
		return NULL;
	tagbuf[0] = '\0'; 	/* reset the buffer */
	thetag = get_header(req, header);
	thetag = strcasestr(thetag, ";tag=");
	if (thetag) {
		thetag += 5;
		ast_copy_string(tagbuf, thetag, tagbufsize);
		sep = strchr(tagbuf, ';');
		if (sep)
			*sep = '\0';
	}
	return thetag;
}

/*! \brief  handle_request_options: Handle incoming OPTIONS request */
static int handle_request_options(struct sip_pvt *p, struct sip_request *req, int debug)
{
	int res;

	set_pvt_allowed_methods(p, req);
	res = get_destination(p, req);
	build_contact(p);
	/* XXX Should we authenticate OPTIONS? XXX */
	if (ast_strlen_zero(p->context))
		strcpy(p->context, default_context);
	if (res < 0)
		transmit_response_with_allow(p, "404 Not Found", req, 0);
	else if (res > 0)
		transmit_response_with_allow(p, "484 Address Incomplete", req, 0);
	else 
		transmit_response_with_allow(p, "200 OK", req, 0);
	/* Destroy if this OPTIONS was the opening request, but not if
	   it's in the middle of a normal call flow. */
	if (!p->lastinvite)
		ast_set_flag(p, SIP_NEEDDESTROY);	

	return res;
}

static int check_supported_required(struct sip_pvt *p, struct sip_request *req,
	char *unsupported, size_t unsupported_len)
{
	char* require;
	unsigned int required_profile = 0;

	/* Find out what they support */
	if (!p->sipoptions)
		parse_sip_options(p, get_header(req, "Supported"), NULL, 0);
	
	require = get_header(req, "Require");
	if (!ast_strlen_zero(require))
	{
		required_profile = parse_sip_options(NULL, require, unsupported, 
		    unsupported_len);
		if (unsupported && !ast_strlen_zero(unsupported))
		{
			ast_log(LOG_WARNING, "Received request with unsupported require "
				"tag: require:%s unsupported:%s\n", require,
				unsupported);
			return -1;			
		}
	}

	/* The option tags may be present in Supported: or Require: headers.
	Include the Require: option tags for further processing as well */
	p->sipoptions |= required_profile;
	p->reqsipoptions = required_profile;
	return 0;
}

/*! \brief  handle_request_invite: Handle incoming INVITE request */
static int handle_request_invite(struct sip_pvt *p, struct sip_request *req, int debug, int ignore, int seqno, struct sockaddr_in *sin, int *recount, char *e)
{
	int res = 1;
	struct ast_channel *c=NULL;
	int gotdest;
	struct ast_frame af = { AST_FRAME_NULL, };
	char *require = get_header(req, "Require");
	char unsupported [256] = { 0, };
	int reinvite = 0;
	char *p_replaces, *replace_id = NULL;
	int rtn;
	const char *p_uac_se_hdr;       /* UAC's Session-Expires header string                      */
	char *p_uac_min_se;       /* UAC's requested Min-SE interval (char string)            */
	int uac_max_se = -1;            /* UAC's Session-Expires in integer format                  */
	int uac_min_se = -1;            /* UAC's Min-SE in integer format                           */
	int st_active = FALSE;          /* Session-Timer on/off boolean                             */
	int st_interval = 0;            /* Session-Timer negotiated refresh interval                */
	int st_min_se = 0;
	enum st_refresher st_ref;       /* Session-Timer session refresher                          */
	int dlg_min_se = -1;
	st_ref = SESSION_TIMER_REFRESHER_AUTO;

	ast_log(LOG_DEBUG, "Received request. Method = %s, Case = %s, Ignore = %d\n", 
			req->method	== SIP_INVITE ? "SIP_INVITE" : "SIP_UPDATE", 
		ast_test_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER) ? 
		"offer" : "answer", ignore);

	/* Checking for channel's state when receving an update */
	if (req->method == SIP_UPDATE && !p->owner)
	{
		/* We should send a 481 response, we can't update the session without a 
		   channel */
		transmit_response(p, "481 Call Leg Does Not Exist", req);
		ast_set_flag(p, SIP_NEEDDESTROY);
		ast_log(LOG_DEBUG, "Got Update when channel still doesn't exist - %s\n",
			p->callid);
		return 0;
	}

	/* Checking for a received offer while having a pending offer */
	if (!ignore && (p->pendinginvite ||
		(ast_test_flag(&p->flags_page2, SIP_PAGE2_UNANSWERED_OFFER) && 
		 has_sdp(req)))) 
	{
		if (ast_test_flag(p, SIP_OUTGOING))
		{
			if (req->method == SIP_INVITE)
				p->glareinvite = seqno;
			transmit_response_reliable(p, "491 Request Pending", req, 1);
			ast_log(LOG_DEBUG, "Got offer on call where we already have pending offer, deferring that - %s\n", p->callid);
			/* Don't destroy dialog here */
			return 0;
		}
		else
		{
			transmit_response_with_warning(p, "500 internal error", req, 
					"399 Received an offer while parsing a previous offer");
			ast_log(LOG_DEBUG, "Received an offer while dealing with a previous offer - %s\n", p->callid);
			return 0;
		}
	}

	if (p->waitid != -1)
	{
	    ast_log(LOG_DEBUG, "We lost the reinvite race. Killing scheduled reinvite.\n");
	    ast_sched_del(sched, p->waitid);
	    p->waitid = -1;
	}

	if ((p_replaces = get_header(req, "Replaces"))) {
		if (ast_strlen_zero(p_replaces)) {
			p_replaces = NULL;
		} else {
			char *ptr;
			ast_log(LOG_DEBUG, "I SEE a Replaces [%s]\n", p_replaces);
			replace_id = ast_strdupa(p_replaces);
			if (strchr(replace_id, '%')) {
				sip_unescape_uri(replace_id);
			}
			if ((ptr = strchr(replace_id, ';'))) {
				*ptr = '\0';
			}
		}
	}

	if (check_supported_required(p, req, unsupported, sizeof(unsupported)))
	{
		transmit_response_with_unsupported(p, "420 Bad extension (unsupported)",
			req, unsupported);
		if (!p->lastinvite)
			ast_set_flag(p, SIP_NEEDDESTROY);
		return -1;
	}
	/* Check if this is a loop */
	/* This happens since we do not properly support SIP domain
	   handling yet... -oej */
	if (req->method == SIP_INVITE && ast_test_flag(p, SIP_OUTGOING) && p->owner && (p->owner->_state != AST_STATE_UP)) {
		/* This is a call to ourself.  Send ourselves an error code and stop
		   processing immediately, as SIP really has no good mechanism for
		   being able to call yourself */
		/* Loop can only occur while receiving an invite request */
		transmit_response(p, "482 Loop Detected", req);
		/* We do NOT destroy p here, so that our response will be accepted */
		return 0;
	}
	if (!ignore) {
		/* Use this as the basis */
		if (debug)
			ast_verbose("Using INVITE request as basis request - %s\n", p->callid);
		sip_cancel_destroy(p);
		
		if (req->method == SIP_INVITE)
		{
			/* This call is no longer outgoing if it ever was */
			ast_clear_flag(p, SIP_OUTGOING);
			/* This also counts as a pending invite */
			p->pendinginvite = seqno;
			copy_request(&p->initreq, req);
		}
		check_via(p, req);
		if (p->owner) {
			int delayed_codec_negotioation = 0;

			/* Handle SDP here if we already have an owner */
			if (!strcasecmp(get_header(req, "Content-Type"), "application/sdp")) {
				if (process_sdp(p, req)) {
					if (req->method == SIP_INVITE && ast_strlen_zero(get_sdp(req, "m", '=')))
						delayed_codec_negotioation = 1;
					else {
						transmit_response(p, "488 Not acceptable here", req);
						if (!p->lastinvite)
							ast_set_flag(p, SIP_NEEDDESTROY);	
						return -1;
					}
				}
			} else if (req->method == SIP_INVITE) {
				ast_log(LOG_DEBUG, "Hm....  No sdp for the moment\n");
				delayed_codec_negotioation = 1;
			}
			if (delayed_codec_negotioation) {
				/* An INVITE with no SDP or an INVITE with SDP that has no media
				 * indicates delayed codec negotiation: We will offer all of
				 * our supported capabilities in the 200 OK, and the remote
				 * party will state the chosen code in the ACK. */
				ast_codec_pref_append(&p->formats, p->usercapability);
			}
		}
	} else if (debug)
		ast_verbose("Ignoring this %s request\n", req->method == SIP_UPDATE ? "UPDATE" :
			"INVITE");
	if (!p->lastinvite && !ignore && !p->owner) {
		/* Handle authentication if this is our first invite */
		set_pvt_allowed_methods(p, req);
		res = check_user(p, req, SIP_INVITE, e, 1, sin, ignore);
		if (res) {
			if (res < 0) {
				ast_log(LOG_NOTICE, "Failed to authenticate user %s\n", get_header(req, "From"));
				if (ignore)
					transmit_response(p, "403 Forbidden", req);
				else
					transmit_response_reliable(p, "403 Forbidden", req, 1);
				ast_set_flag(p, SIP_NEEDDESTROY);	
				p->theirtag[0] = '\0'; /* Forget their to-tag, we'll get a new one */
			}
			return 0;
		}
		switch (p->prack_level) /* Use the global value here! */
		{
		case PRACK_LEVEL_NONE:
			/* Does the INVITE require PRACK? */
			if (strcasestr(require, "100rel"))
			{
				struct sip_request resp;
				int seqno = 0;

				respprep(&resp, p, "420 Bad Extension", req);
				add_header(&resp, "Unsupported", "100rel");
				add_header(&resp, "Content-Length", "0");
				add_blank_header(&resp);
				send_response(p, &resp, 0, seqno);
				ast_set_flag(p, SIP_NEEDDESTROY);
				return -1;
			}
			break;
		case PRACK_LEVEL_SUPPORTED:
		case PRACK_LEVEL_REQUIRE:
			/* Does the INVITE require PRACK? */
			if (strcasestr(require, "100rel") || p->sipoptions & SIP_OPT_100REL)
				p->prack_level = PRACK_LEVEL_REQUIRE;
			else p->prack_level = PRACK_LEVEL_NONE;
			break;
		}

		/* Process the SDP portion */
		if (!ast_strlen_zero(get_header(req, "Content-Type"))) {
			if (process_sdp(p, req)) {
				transmit_response(p, "488 Not acceptable here", req);
				/* Stay alive for another 5 seconds because the caller may send
				 * some ACKs that should be ignored (RFC 3261, sec 17.2.1) */
				sip_scheddestroy(p, 5000);
				return -1;
			}
		} else {
		        ast_codec_pref_append(&p->formats, p->usercapability);
			ast_log(LOG_DEBUG, "Hm....  No sdp for the moment\n");
		}
		/* Queue NULL frame to prod ast_rtp_bridge if appropriate */
		if (p->owner)
			ast_queue_frame(p->owner, &af);
		/* Initialize the context if it hasn't been already */
		if (ast_strlen_zero(p->context))
			strcpy(p->context, default_context);
		/* Check number of concurrent calls -vs- incoming limit HERE */
		ast_log(LOG_DEBUG, "Checking SIP call limits for device %s\n", p->username);
		res = update_call_counter(p, INC_CALL_LIMIT);
		if (res) {
			if (res < 0) {
				ast_log(LOG_NOTICE, "Failed to place call for user %s, too many calls\n", p->username);
				if (ignore)
					transmit_response(p, "480 Temporarily Unavailable (Call limit)", req);
				else
					transmit_response_reliable(p, "480 Temporarily Unavailable (Call limit) ", req, 1);
				ast_set_flag(p, SIP_NEEDDESTROY);	
			}
			return 0;
		}
		/* Get destination right away */
		gotdest = get_destination(p, NULL);

		get_rdnis(p, NULL);
		extract_uri(p, req);
		build_contact(p);

		if (!replace_id && gotdest) {
			if (gotdest < 0) {
				if (ignore)
					transmit_response(p, "404 Not Found", req);
				else
					transmit_response_reliable(p, "404 Not Found", req, 1);
				update_call_counter(p, DEC_CALL_LIMIT);
			} else {
				if (ignore)
					transmit_response(p, "484 Address Incomplete", req);
				else
					transmit_response_reliable(p, "484 Address Incomplete", req, 1);
				update_call_counter(p, DEC_CALL_LIMIT);
			}
			ast_set_flag(p, SIP_NEEDDESTROY);		
		} else {
			/* If no extension was specified, use the s one */
			if (ast_strlen_zero(p->exten))
				ast_copy_string(p->exten, "s", sizeof(p->exten));
			/* Initialize tag */	
			make_our_tag(p->tag, sizeof(p->tag));
			/* First invitation */
			c = sip_new(p, AST_STATE_DOWN, ast_strlen_zero(p->username) ? NULL : p->username, 1);
			set_template_caller_name(p);
			*recount = 1;
			/* Save Record-Route for any later requests we make on this dialogue */
			build_route(p, req, 0);
			if (c) {
			    	char *alert_info;

			    	if (((alert_info = get_header(req, "Alert-Info"))) && (!ast_strlen_zero(alert_info)))
				    pbx_builtin_setvar_helper(c, "_ALERTINFO", alert_info);
				else
				    pbx_builtin_setvar_helper(c, "_ALERTINFO", NULL);
				
				if (replace_id) {
					struct sip_pvt *refer_pvt;
					struct ast_channel *refer;
					struct ast_frame *f;
					
					if ((refer_pvt = get_sip_pvt_byid_locked(replace_id, req, NULL, p->theirtag)) && refer_pvt->owner) {
						ast_log(LOG_DEBUG, "XXXXXXXX I PARSED a Replaces [%s]\n", p_replaces);
						transmit_response(p, "100 Trying", req);
						refer = refer_pvt->owner;
						ast_mutex_unlock(&refer_pvt->owner->lock);
						ast_mutex_unlock(&refer_pvt->lock);
						ast_mutex_lock(&refer->lock);
						ast_channel_masquerade(refer, c );
						/* Unlock locks so ast_hangup can do its magic */
						ast_mutex_unlock(&p->lock);
						ast_hangup(c);
						ast_mutex_lock(&p->lock);
						if ((f = ast_read(refer))) {
							ast_log(LOG_DEBUG, "XXXXXXXX I DID a Replaces [%s]\n", p_replaces);
							ast_frfree(f);
							ast_setstate(refer, AST_STATE_UP);
						}
						ast_mutex_unlock(&refer->lock);
						c = refer;
					} else {
						transmit_response_with_allow(p, "481 Call/Transaction Does Not Exist", req, 0);
						return 0;
					}
				}
				/* Pre-lock the call */
				ast_mutex_lock(&c->lock);
			}
		}
		
	} else {
		if (option_debug > 1 && sipdebug)
			ast_log(LOG_DEBUG, "Got a SIP %s for call %s\n", req->method ==	SIP_UPDATE ?
				"update" : "re-invite", p->callid);
		if (!ignore)
			reinvite = 1;
		c = p->owner;
	}

	/* Allocate Session-Timers struct w/in the dialog */
	if (!p->stimer)
	    sip_st_alloc(p);

	/* Session-Timers */
	if ((p->sipoptions & SIP_OPT_TIMER) && !ast_strlen_zero(get_header(req, "Session-Expires"))) {
		/* The UAC has requested session-timers for this session. Negotiate
		the session refresh interval and who will be the refresher */
		ast_log(LOG_DEBUG, "Incoming INVITE with 'timer' option supported and \"Session-Expires\" header.\n");

		/* Parse the Session-Expires header */
		p_uac_se_hdr = get_header(req, "Session-Expires");
		rtn = parse_session_expires(p_uac_se_hdr, &uac_max_se, &st_ref, 1);
		if (rtn != 0) {
			transmit_response(p, "400 Session-Expires Invalid Syntax", req);
			if (!p->lastinvite) {
				sip_scheddestroy(p, 32000);
 			}
			return -1;
		}

		/* Parse the Min-SE header */
		p_uac_min_se = get_header(req, "Min-SE");
		if (!ast_strlen_zero(p_uac_min_se)) {
			rtn = parse_minse(p_uac_min_se, &uac_min_se); 
			if (rtn != 0) {
        			transmit_response(p, "400 Min-SE Invalid Syntax", req);
       	   			if (!p->lastinvite) {
					sip_scheddestroy(p, 32000);
				}
				return -1;
			}
		}

		dlg_min_se = st_get_se(p, FALSE);
		switch (st_get_mode(p, 1)) {
		case SESSION_TIMER_MODE_ACCEPT:
		case SESSION_TIMER_MODE_ORIGINATE:
			if (uac_max_se > 0 && uac_max_se < dlg_min_se) {
				transmit_response_with_minse(p, "422 Session Interval Too Small", req, dlg_min_se);
				if (!p->lastinvite) {
					sip_scheddestroy(p, 32000);
				}
				return -1;
			}

			p->stimer->st_active_peer_ua = TRUE;
			st_active = TRUE;
			if (st_ref == SESSION_TIMER_REFRESHER_AUTO) {
				st_ref = st_get_refresher(p);
			}

			if (uac_max_se > 0) {
				int dlg_max_se = st_get_se(p, TRUE);
				if (dlg_max_se >= uac_min_se) {
					st_interval = (uac_max_se < dlg_max_se) ? uac_max_se : dlg_max_se;
				} else {
					st_interval = uac_max_se;
				}
			} else {
				/* Set to default max value */
				st_interval = global_max_se;
			}

			st_min_se = MAX(uac_min_se, dlg_min_se);
					
			break;

		case SESSION_TIMER_MODE_REFUSE:
			if (p->reqsipoptions & SIP_OPT_TIMER) {
				char *require = get_header(req, "Require");
				transmit_response_with_unsupported(p, "420 Option Disabled", req, require);
				ast_log(LOG_WARNING,"Received SIP INVITE with supported but disabled option: %s\n", require);
				if (!p->lastinvite) {
					sip_scheddestroy(p, 32000);
				}
				return -1;
			}
			break;

		default:
			ast_log(LOG_ERROR, "Internal Error %d at %s:%d\n", st_get_mode(p, 1), __FILE__, __LINE__);
			break;
		}
	} else {
		/* The UAC did not request session-timers. If we dont force it or other
		 * side supports it then Asterisk (UAS) should decide (based on 
		 * session-timer-mode in sip.conf) whether to run session-timers for 
		 * this session or not. */
		if ((ast_test_flag(&p->flags_page2, SIP_PAGE2_SESSION_TIMERS_FORCE) ||
			p->sipoptions & SIP_OPT_TIMER) && 
			(st_get_mode(p, 1) == SESSION_TIMER_MODE_ORIGINATE))
		{
			st_active = TRUE;
			st_interval = st_get_se(p, TRUE);
			st_min_se = st_get_se(p, FALSE);
			st_ref = SESSION_TIMER_REFRESHER_LOCAL;
			p->stimer->st_active_peer_ua = FALSE;
		}
	}

	if (reinvite == 0 && !ignore) {
		/* Session-Timers: Start session refresh timer based on negotiation/config */
		if (st_active == TRUE) {
			p->stimer->st_active   = TRUE;
			p->stimer->st_interval = st_interval;
			p->stimer->st_min_se = MAX(p->stimer->st_min_se, st_min_se);
			p->stimer->st_ref      = st_ref;
			start_session_timer(p);
		}
	} else {
		if (p->stimer->st_active == TRUE) {
			/* Session-Timers:  A re-invite request sent within a dialog will serve as 
			a refresh request, no matter whether the re-invite was sent for refreshing 
			the session or modifying it.*/
			ast_log(LOG_DEBUG, "Restarting session-timers on a refresh - %s\n", p->callid);

			/* The UAC may be adjusting the session-timers mid-session */
			if (st_interval > 0) {
				p->stimer->st_interval = st_interval;
				p->stimer->st_ref      = st_ref;
			}
			if (st_min_se > 0) {
				p->stimer->st_min_se = MAX(p->stimer->st_min_se, st_min_se);
			}

			restart_session_timer(p);
			if (p->stimer->st_expirys > 0) {
				p->stimer->st_expirys--;
			}
		}
	}

	if (!ignore && p && req->method == SIP_INVITE)
		p->lastinvite = seqno;
	if (c) {
#ifdef OSP_SUPPORT
		ast_channel_setwhentohangup (c, p->osptimelimit);
#endif
		switch(c->_state) {
		case AST_STATE_DOWN:
			transmit_response(p, "100 Trying", req);
			ast_setstate(c, AST_STATE_RING);
			if (strcmp(p->exten, ast_pickup_ext())) {
				enum ast_pbx_result res;
				struct sip_user *user;

				/* This SIP belongs to an extension client,
				 * meaning a SIP extension client starts the PBX for an
				 * outgoing call. */ 
				if ((user = find_user(p->username, 1)))
				{
					if (!strcasecmp(p->cid_name, NETWORK_ANONYMOUS_CALLERID))
					{
						pbx_builtin_set_int_var_helper(c,
							"_EXT_CALLERID_RESTRICTED", 1);
					}

					ASTOBJ_UNREF(user, sip_destroy_user);
				}

				res = ast_pbx_start(c);

				switch (res) {
				case AST_PBX_FAILED:
					ast_log(LOG_WARNING, "Failed to start PBX :(\n");
					if (ignore)
						transmit_response(p, "503 Unavailable", req);
					else
						transmit_response_reliable(p, "503 Unavailable", req, 1);
					break;
				case AST_PBX_CALL_LIMIT:
					ast_log(LOG_WARNING, "Failed to start PBX (call limit reached) \n");
					if (ignore)
						transmit_response(p, "480 Temporarily Unavailable", req);
					else
						transmit_response_reliable(p, "480 Temporarily Unavailable", req, 1);
					break;
				case AST_PBX_SUCCESS:
					/* nothing to do */
					break;
				}

				if (res) {
					ast_log(LOG_WARNING, "Failed to start PBX :(\n");
					/* Unlock locks so ast_hangup can do its magic */
					ast_mutex_unlock(&c->lock);
					ast_mutex_unlock(&p->lock);
					ast_hangup(c);
					ast_mutex_lock(&p->lock);
					c = NULL;
				}
			} else {
				ast_mutex_unlock(&c->lock);
				if (ast_pickup_call(c)) {
					ast_log(LOG_NOTICE, "Nothing to pick up\n");
					if (ignore)
						transmit_response(p, "503 Unavailable", req);
					else
						transmit_response_reliable(p, "503 Unavailable", req, 1);
					ast_set_flag(p, SIP_ALREADYGONE);	
					/* Unlock locks so ast_hangup can do its magic */
					ast_mutex_unlock(&p->lock);
					ast_hangup(c);
					ast_mutex_lock(&p->lock);
					c = NULL;
				} else {
					ast_mutex_unlock(&p->lock);
					ast_setstate(c, AST_STATE_DOWN);
					ast_hangup(c);
					ast_mutex_lock(&p->lock);
					c = NULL;
				}
			}
			break;
		case AST_STATE_RING:
			if (req->method == SIP_INVITE)
				transmit_response(p, "100 Trying", req);
			else {
		 		if (has_sdp(req)) {
					transmit_response_with_sdp(p, "200 OK", req, 0, 
							p->session_modify == TRUE ? FALSE:TRUE);
				}
				else
					transmit_response(p, "200 OK", req);
			}
			break;
		case AST_STATE_RINGING:
			if (req->method == SIP_INVITE)
				transmit_response(p, "180 Ringing", req);
			else {
		 		if (has_sdp(req)) {
					transmit_response_with_sdp(p, "200 OK", req, 0, 
							p->session_modify == TRUE ? FALSE:TRUE);
				}
				else
					transmit_response(p, "200 OK", req);
			}
			break;
		case AST_STATE_UP:
#if defined(T38_SUPPORT)
			if (p->t38state == T38_PEER_REINVITE) {
	    		    struct ast_channel *bridgepeer = NULL;
			    struct sip_pvt *bridgepvt = NULL;
			    if ((bridgepeer=ast_bridged_channel(p->owner))) {
					/* We have a bridge, and this is re-invite to switchover to T38 so we send re-invite with T38 SDP, to other side of bridge*/
					/*! XXX: we should also check here does the other side supports t38 at all !!! XXX */  
					if (!strcasecmp(bridgepeer->tech->type,"SIP")) { /* If we are bridged to SIP channel */
						bridgepvt = (struct sip_pvt*)bridgepeer->tech_pvt;
						if (!(bridgepvt->t38state)) {
						    if (bridgepvt->udptl) { /* If everything is OK with other side's udptl struct */
							    /* Send re-invite to the bridged channel */ 
							    sip_handle_t38_reinvite(bridgepeer,p,1);
						    } else { /* Something is wrong with peers udptl struct */
							    ast_log(LOG_WARNING, "Strange... The other side of the bridge don't have udptl struct\n");
							    ast_mutex_lock(&bridgepvt->lock);
							    bridgepvt->t38state = T38_DISABLED;
							    ast_mutex_unlock(&bridgepvt->lock);
							    ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",bridgepvt->t38state, bridgepeer->name);
							    p->t38state = T38_DISABLED;
							    ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
	    						    if (ignore)
								    transmit_response(p, "415 Unsupported Media Type", req);
							    else
								    transmit_response_reliable(p, "415 Unsupported Media Type", req, 1);
							    ast_set_flag(p, SIP_NEEDDESTROY);
						    } 
						}
					} else if (!strcasecmp(bridgepeer->tech->type, "jdsp")) {
						p->vbdmode = VBD_MODE_FAX;
						ast_queue_control(p->owner, AST_CONTROL_T38);
						if (req->method == SIP_UPDATE && !has_sdp(req))
							transmit_response(p, "200 OK", req);
						else
							transmit_response_with_t38_sdp(p, "200 OK", req, 1, p->session_modify == TRUE ? FALSE:TRUE);
						p->t38state = T38_ENABLED;
						ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
					} else {
						/* Other side is neither a SIP nor a jdsp channel */
						if (ignore)
							transmit_response(p, "415 Unsupported Media Type", req);
						else 
	    						transmit_response_reliable(p, "415 Unsupported Media Type", req, 1);
						p->t38state = T38_DISABLED;
						ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
						ast_set_flag(p, SIP_NEEDDESTROY);		
					}	
			    } else {
				/* we are not bridged in a call */ 
					if (req->method == SIP_UPDATE && !has_sdp(req))
						transmit_response(p, "200 OK", req);
					else {
						transmit_response_with_t38_sdp(p, "200 OK", req, req->method == SIP_INVITE,
								p->session_modify == TRUE ? FALSE:TRUE);
					}
				p->t38state = T38_ENABLED;
				p->vbdmode = VBD_MODE_FAX;
				ast_queue_control(p->owner, AST_CONTROL_T38);
				ast_log(LOG_DEBUG,"T38 state changed to %d on channel %s\n",p->t38state, p->owner ? p->owner->name : "<none>");
			    }
			} else if (p->t38state == T38_DISABLED) { /* Channel doesn't have T38 offered or enabled */
					/* If we are bridged to a channel that has T38 enabled than this is a case of RTP re-invite after T38 session */
					/* so handle it here (re-invite other party to RTP) */
	    				struct ast_channel *bridgepeer = NULL;
					struct sip_pvt *bridgepvt = NULL;
					if ((bridgepeer=ast_bridged_channel(p->owner))) {
						if (!strcasecmp(bridgepeer->tech->type,"SIP"))
							bridgepvt = (struct sip_pvt*)bridgepeer->tech_pvt;
						
						if (bridgepvt && bridgepvt->t38state == T38_ENABLED) {
								ast_log(LOG_WARNING, "RTP re-inivte after T38 session not handled yet !\n");
								/* Insted of this we should somehow re-invite the other side of the bridge to RTP */
								if (ignore)
									transmit_response(p, "488 Not Acceptable Here (unsupported)", req);
								else
									transmit_response_reliable(p, "488 Not Acceptable Here (unsupported)", req, 1);
								ast_set_flag(p, SIP_NEEDDESTROY);
						} else {
							/* No bridged peer with T38 enabled*/
							if (req->method == SIP_UPDATE && !has_sdp(req))
								transmit_response(p, "200 OK", req);
							else {
								transmit_response_with_sdp(p, "200 OK", req, req->method == SIP_INVITE,
										p->session_modify == TRUE ? FALSE:TRUE);
							}
						}
					}
					else
					{
						if (req->method == SIP_UPDATE && !has_sdp(req))
							transmit_response(p, "200 OK", req);
						else {
							transmit_response_with_sdp(p, "200 OK", req, req->method == SIP_INVITE, 
									p->session_modify == TRUE ? FALSE:TRUE);
						}
					}
			} else if (p->t38state == T38_ENABLED) {
				if (req->method == SIP_UPDATE && !has_sdp(req))
					transmit_response(p, "200 OK", req);
				else {
					transmit_response_with_t38_sdp(p, "200 OK", req, req->method ==
							SIP_INVITE,	p->session_modify == TRUE ? FALSE:TRUE);
				}
			}
#else
			if (req->method == SIP_UPDATE && !has_sdp(req))
				transmit_response(p, "200 OK", req);
			else {
				transmit_response_with_sdp(p, "200 OK", req, req->method ==
					SIP_INVITE, p->session_modify == TRUE ? FALSE:TRUE);
			}
#endif
			break;
		default:
			ast_log(LOG_WARNING, "Don't know how to handle INVITE in state %d\n", c->_state);
			transmit_response(p, "100 Trying", req);
		}
	} else {
		if (p && !ast_test_flag(p, SIP_NEEDDESTROY) && !ignore) {
		    if (!ast_codec_pref_bits(&p->formats)) {
				if (ignore)
					transmit_response(p, "488 Not Acceptable Here (codec error)", req);
				else
					transmit_response_reliable(p, "488 Not Acceptable Here (codec error)", req, 1);
				ast_set_flag(p, SIP_NEEDDESTROY);	
			} else {
				ast_log(LOG_NOTICE, "Unable to create/find channel\n");
				if (ignore)
					transmit_response(p, "503 Unavailable", req);
				else
					transmit_response_reliable(p, "503 Unavailable", req, 1);
				ast_set_flag(p, SIP_NEEDDESTROY);	
			}
		}
	}
	return res;
}

/*! \brief  handle_request_refer: Handle incoming REFER request ---*/
static int handle_request_refer(struct sip_pvt *p, struct sip_request *req, int debug, int ignore, int seqno, int *nounlock)
{
	struct ast_channel *c=NULL;
	int res;
	struct ast_channel *transfer_to;

	if (option_debug > 2)
		ast_log(LOG_DEBUG, "SIP call transfer received for call %s (REFER)!\n", p->callid);
	if (ast_strlen_zero(p->context))
		strcpy(p->context, default_context);
	res = get_refer_info(p, req);
	if (res == SIP_RETVAL_IGNORE) {
		ignore = 1;
	} else if (res < 0)
		transmit_response_with_allow(p, "404 Not Found", req, 1);
	else if (res > 0)
		transmit_response_with_allow(p, "484 Address Incomplete", req, 1);
	else {
		int nobye = 0;
		if (!ignore) {
			if (p->refer_call) {
				ast_log(LOG_DEBUG,"202 Accepted (supervised)\n");
				attempt_transfer(p, p->refer_call);
				if (p->refer_call->owner)
					ast_mutex_unlock(&p->refer_call->owner->lock);
				ast_mutex_unlock(&p->refer_call->lock);
				p->refer_call = NULL;
				ast_set_flag(p, SIP_GOTREFER);	
			} else {
				ast_log(LOG_DEBUG,"202 Accepted (blind)\n");
				c = p->owner;
				if (c) {
					transfer_to = ast_bridged_channel(c);
					if (transfer_to) {
						ast_log(LOG_DEBUG, "Got SIP blind transfer, applying to '%s'\n", transfer_to->name);
						ast_moh_stop(transfer_to);
						if (!strcmp(p->refer_to, ast_parking_ext())) {
							/* Must release c's lock now, because it will not longer
							    be accessible after the transfer! */
							*nounlock = 1;
							ast_mutex_unlock(&c->lock);
							sip_park(transfer_to, c, req);
							nobye = 1;
						} else {
							/* Must release c's lock now, because it will not longer
							    be accessible after the transfer! */
							ast_indicate(transfer_to, AST_CONTROL_UNHOLD);

							*nounlock = 1;
							ast_mutex_unlock(&c->lock);
							ast_async_goto(transfer_to,p->context, p->refer_to,1);
						}
					} else {
						ast_log(LOG_DEBUG, "Got SIP blind transfer but nothing to transfer to.\n");
						ast_queue_hangup(p->owner);
					}
				}
				ast_set_flag(p, SIP_GOTREFER);	
			}
			transmit_response(p, "202 Accepted", req);
			transmit_notify_with_sipfrag(p, seqno);
			/* Always increment on a BYE */
			if (!nobye) {
				transmit_request_with_auth(p, SIP_BYE, 0, 1, 1);
				ast_set_flag(p, SIP_ALREADYGONE);	
			}
		}
	}
	return res;
}

/*! \brief Parse Reason header if exists and save data to channel */
static void handle_hangup_reason_header(struct sip_pvt *p, struct sip_request *req)
{
	char *reason;
	struct ast_channel *bridge;
	int cause = 0;
	int start = 0;

    if (!p->owner || !(bridge = ast_bridged_channel(p->owner)))
    {
		ast_log(LOG_DEBUG, "Owner or bridge doesn't exist\n");
		return;
    }

    for (reason = __get_header(req, "Reason", &start);
		!ast_strlen_zero(reason); reason = __get_header(req, "Reason", &start))
	{
		if (strncasecmp(reason, "Q.850", 5))
			continue;

		if (!(reason = strchr(reason, '=')))
		{
			ast_log(LOG_ERROR, "Invalid header - %s\n", reason);
			return;
		}
		else
			reason++;

		/* Can override previous Asterisk causes, but network causes have 
		   more meaning than local ones */
		if (sscanf(reason, "%d", &cause) != 1 ||
			(cause < AST_CAUSE_MIN || cause > AST_CAUSE_MAX))
		{
			ast_log(LOG_ERROR, "Cause outside of bounderies - %d\n", cause);
			return;
		}

		bridge->hangupcause = cause;
	}
}

/*! \brief  handle_request_cancel: Handle incoming CANCEL request ---*/
static int handle_request_cancel(struct sip_pvt *p, struct sip_request *req, int debug, int ignore)
{
		
	check_via(p, req);
	ast_set_flag(p, SIP_ALREADYGONE);	
	if (p->rtp) {
		/* Immediately stop RTP */
		ast_rtp_stop(p->rtp);
	}
	if (p->vrtp) {
		/* Immediately stop VRTP */
		ast_rtp_stop(p->vrtp);
	}
	switch (p->prack_level)
	{
	case PRACK_LEVEL_NONE:
	    break;
	case PRACK_LEVEL_SUPPORTED:
	    add_header(req, "Supported", "100rel");
	    if (sipdebug)
		ast_log(LOG_DEBUG, "Adding SIP Header \"Supported\" with content :100rel: \n");
	    break;
	case PRACK_LEVEL_REQUIRE:
	    add_header(req, "Require", "100rel");
	    if (sipdebug)
		ast_log(LOG_DEBUG, "Adding SIP Header \"Require\" with content :100rel: \n");
	    break;
	}
#if defined(T38_SUPPORT)
	if (p->udptl) {
		/* Immediately stop T.38 UDPTL */
		ast_udptl_stop(p->udptl);
	}
#endif

	handle_hangup_reason_header(p, req);

	if (p->owner)
		ast_queue_hangup(p->owner);
	else
		ast_set_flag(p, SIP_NEEDDESTROY);	
	if (p->initreq.len > 0) {
		if (!ignore)
			transmit_response_reliable(p, "487 Request Terminated", &p->initreq, 1);
		ast_clear_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
		transmit_response(p, "200 OK", req);
		return 1;
	} else {
		transmit_response(p, "481 Call Leg Does Not Exist", req);
		return 0;
	}
}

/*! \brief  handle_request_bye: Handle incoming BYE request ---*/
static int handle_request_bye(struct sip_pvt *p, struct sip_request *req, int debug, int ignore)
{
	struct ast_channel *c=NULL;
	int res;
	struct ast_channel *bridged_to;
	char iabuf[INET_ADDRSTRLEN];
	
	if (p->pendinginvite && !ast_test_flag(p, SIP_OUTGOING) && !ignore)
		transmit_response_reliable(p, "487 Request Terminated", &p->initreq, 1);

	copy_request(&p->initreq, req);
	check_via(p, req);
	ast_set_flag(p, SIP_ALREADYGONE);	
	if (p->rtp) {
		/* Immediately stop RTP */
		ast_rtp_stop(p->rtp);
	}
	if (p->vrtp) {
		/* Immediately stop VRTP */
		ast_rtp_stop(p->vrtp);
	}
#if defined(T38_SUPPORT)
	if (p->udptl) {
		/* Immediately stop T.38 UDPTL */
		ast_udptl_stop(p->udptl);
	}
#endif
    stop_session_timer(p); /* Stop Session-Timer */

	handle_hangup_reason_header(p, req);

	if (!ast_strlen_zero(get_header(req, "Also"))) {
		ast_log(LOG_NOTICE, "Client '%s' using deprecated BYE/Also transfer method.  Ask vendor to support REFER instead\n",
			ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr));
		if (ast_strlen_zero(p->context))
			strcpy(p->context, default_context);
		res = get_also_info(p, req);
		if (!res) {
			c = p->owner;
			if (c) {
				bridged_to = ast_bridged_channel(c);
				if (bridged_to) {
					/* Don't actually hangup here... */
					ast_moh_stop(bridged_to);
					ast_async_goto(bridged_to, p->context, p->refer_to,1);
				} else
					ast_queue_hangup(p->owner);
			}
		} else {
			ast_log(LOG_WARNING, "Invalid transfer information from '%s'\n", ast_inet_ntoa(iabuf, sizeof(iabuf), p->recv.sin_addr));
			if (p->owner)
				ast_queue_hangup(p->owner);
		}
	} else if (p->owner)
		ast_queue_hangup(p->owner);
	else
		ast_set_flag(p, SIP_NEEDDESTROY);	
	ast_clear_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
	transmit_response(p, "200 OK", req);
	manager_event(EVENT_FLAG_SYSTEM, "SessionChanged", 
	    "Established: 0\r\n"
	    "Peer: %s\r\n",
	    p->peername);

	return 1;
}

/*! \brief  handle_request_message: Handle incoming MESSAGE request ---*/
static int handle_request_message(struct sip_pvt *p, struct sip_request *req, int debug, int ignore, struct sockaddr_in *sin, char *e)
{
	if (!ignore) {
		if (debug)
			ast_verbose("Receiving message!\n");
		receive_message(p, req, 0, sin, e);
	} else {
		transmit_response(p, "202 Accepted", req);
	}
	return 1;
}
/*! \brief  handle_request_subscribe: Handle incoming SUBSCRIBE request ---*/
static int handle_request_subscribe(struct sip_pvt *p, struct sip_request *req, int debug, int ignore, struct sockaddr_in *sin, int seqno, char *e)
{
	int gotdest;
	int res = 0;
	int firststate = AST_EXTENSION_REMOVED;

	if (p->initreq.headers) {	
		/* We already have a dialog */
		if (p->initreq.method != SIP_SUBSCRIBE) {
			/* This is a SUBSCRIBE within another SIP dialog, which we do not support */
			/* For transfers, this could happen, but since we haven't seen it happening, let us just refuse this */
 			transmit_response(p, "403 Forbidden (within dialog)", req);
			/* Do not destroy session, since we will break the call if we do */
			ast_log(LOG_DEBUG, "Got a subscription within the context of another call, can't handle that - %s (Method %s)\n", p->callid, sip_methods[p->initreq.method].text);
			return 0;
		} else {
			if (debug)
				ast_log(LOG_DEBUG, "Got a re-subscribe on existing subscription %s\n", p->callid);
		}
	}
	if (!ignore && !p->initreq.headers) {
		/* Use this as the basis */
		if (debug)
			ast_verbose("Using latest SUBSCRIBE request as basis request\n");
		set_pvt_allowed_methods(p, req);
		/* This call is no longer outgoing if it ever was */
		ast_clear_flag(p, SIP_OUTGOING);
		copy_request(&p->initreq, req);
		check_via(p, req);
	} else if (debug && ignore)
		ast_verbose("Ignoring this SUBSCRIBE request\n");

	if (!p->lastinvite) {
		char mailboxbuf[256]="";
		int found = 0;
		char *mailbox = NULL;
		int mailboxsize = 0;

		char *event = get_header(req, "Event");	/* Get Event package name */
		char *accept = get_header(req, "Accept");

 		if (!strcmp(event, "message-summary") && !strcmp(accept, "application/simple-message-summary")) {
			mailbox = mailboxbuf;
			mailboxsize = sizeof(mailboxbuf);
		}
		/* Handle authentication if this is our first subscribe */
		res = check_user_full(p, req, SIP_SUBSCRIBE, e, 0, sin, ignore, mailbox, mailboxsize);
		if (res) {
			if (res < 0) {
				ast_log(LOG_NOTICE, "Failed to authenticate user %s for SUBSCRIBE\n", get_header(req, "From"));
				ast_set_flag(p, SIP_NEEDDESTROY);	
			}
			return 0;
		}
		/* Initialize the context if it hasn't been already */
		if (!ast_strlen_zero(p->subscribecontext))
			ast_copy_string(p->context, p->subscribecontext, sizeof(p->context));
		else if (ast_strlen_zero(p->context))
			strcpy(p->context, default_context);
		/* Get destination right away */
		gotdest = get_destination(p, NULL);
		build_contact(p);
		if (gotdest) {
			if (gotdest < 0)
				transmit_response(p, "404 Not Found", req);
			else
				transmit_response(p, "484 Address Incomplete", req);	/* Overlap dialing on SUBSCRIBE?? */
			ast_set_flag(p, SIP_NEEDDESTROY);	
		} else {

			/* Initialize tag for new subscriptions */	
			if (ast_strlen_zero(p->tag))
				make_our_tag(p->tag, sizeof(p->tag));

			if (!strcmp(event, "presence") || !strcmp(event, "dialog")) { /* Presence, RFC 3842 */

 				/* Header from Xten Eye-beam Accept: multipart/related, application/rlmi+xml, application/pidf+xml, application/xpidf+xml */
 				if (strstr(accept, "application/pidf+xml")) {
 					p->subscribed = PIDF_XML;         /* RFC 3863 format */
 				} else if (strstr(accept, "application/dialog-info+xml")) {
 					p->subscribed = DIALOG_INFO_XML;
 					/* IETF draft: draft-ietf-sipping-dialog-package-05.txt */
 				} else if (strstr(accept, "application/cpim-pidf+xml")) {
 					p->subscribed = CPIM_PIDF_XML;    /* RFC 3863 format */
 				} else if (strstr(accept, "application/xpidf+xml")) {
 					p->subscribed = XPIDF_XML;        /* Early pre-RFC 3863 format with MSN additions (Microsoft Messenger) */
 				} else if (strstr(p->useragent, "Polycom")) {
 					p->subscribed = XPIDF_XML;        /*  Polycoms subscribe for "event: dialog" but don't include an "accept:" header */
				} else {
 					/* Can't find a format for events that we know about */
 					transmit_response(p, "489 Bad Event", req);
 					ast_set_flag(p, SIP_NEEDDESTROY);	
 					return 0;
 				}
 			} else if (!strcmp(event, "message-summary") && !strcmp(accept, "application/simple-message-summary")) {
				/* Looks like they actually want a mailbox status */

				/* At this point, we should check if they subscribe to a mailbox that
				  has the same extension as the peer or the mailbox id. If we configure
				  the context to be the same as a SIP domain, we could check mailbox
				  context as well. To be able to securely accept subscribes on mailbox
				  IDs, not extensions, we need to check the digest auth user to make
				  sure that the user has access to the mailbox.
				 
				  Since we do not act on this subscribe anyway, we might as well 
				  accept any authenticated peer with a mailbox definition in their 
				  config section.
				
				*/
				if (!ast_strlen_zero(mailbox)) {
					found++;
				}

				if (found){
				        ast_clear_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
					transmit_response(p, "200 OK", req);
					ast_set_flag(p, SIP_NEEDDESTROY);	
				} else {
					transmit_response(p, "404 Not found", req);
					ast_set_flag(p, SIP_NEEDDESTROY);	
				}
				return 0;
			} else { /* At this point, Asterisk does not understand the specified event */
				transmit_response(p, "489 Bad Event", req);
				if (option_debug > 1)
					ast_log(LOG_DEBUG, "Received SIP subscribe for unknown event package: %s\n", event);
				ast_set_flag(p, SIP_NEEDDESTROY);	
				return 0;
			}
			if (p->subscribed != NONE)
				p->stateid = ast_extension_state_add(p->context, p->exten, cb_extensionstate, p);
		}
	}

	if (!ignore && p)
		p->lastinvite = seqno;
	if (p && !ast_test_flag(p, SIP_NEEDDESTROY)) {
		p->expiry = atoi(get_header(req, "Expires"));

		/* The next 4 lines can be removed if the SNOM Expires bug is fixed */
		if (p->subscribed == DIALOG_INFO_XML) {  
			if (p->expiry > max_expiry)
				p->expiry = max_expiry;
		}
		if (sipdebug || option_debug > 1)
			ast_log(LOG_DEBUG, "Adding subscription for extension %s context %s for peer %s\n", p->exten, p->context, p->username);
		if (p->autokillid > -1)
			sip_cancel_destroy(p);	/* Remove subscription expiry for renewals */
		sip_scheddestroy(p, (p->expiry + 10) * 1000);	/* Set timer for destruction of call at expiration */

		if ((firststate = ast_extension_state(NULL, p->context, p->exten)) < 0) {
			ast_log(LOG_ERROR, "Got SUBSCRIBE for extensions without hint. Please add hint to %s in context %s\n", p->exten, p->context);
			transmit_response(p, "404 Not found", req);
			ast_set_flag(p, SIP_NEEDDESTROY);	
			return 0;
		} else {
			struct sip_pvt *p_old;

			ast_set_flag((&p->flags_page2), SIP_PAGE2_DIALOG_ESTABLISHED);
			transmit_response(p, "200 OK", req);
			transmit_state_notify(p, firststate, 1, 1);	/* Send first notification */
			append_history(p, "Subscribestatus", ast_extension_state2str(firststate));

			/* remove any old subscription from this peer for the same exten/context,
			   as the peer has obviously forgotten about it and it's wasteful to wait
			   for it to expire and send NOTIFY messages to the peer only to have them
			   ignored (or generate errors)
			*/
			ast_mutex_lock(&iflock);
			for (p_old = iflist; p_old; p_old = p_old->next) {
				if (p_old == p)
					continue;
				if (p_old->initreq.method != SIP_SUBSCRIBE)
					continue;
				if (p_old->subscribed == NONE)
					continue;
				ast_mutex_lock(&p_old->lock);
				if (!strcmp(p_old->username, p->username)) {
					if (!strcmp(p_old->exten, p->exten) &&
					    !strcmp(p_old->context, p->context)) {
						ast_set_flag(p_old, SIP_NEEDDESTROY);
						ast_mutex_unlock(&p_old->lock);
						break;
					}
				}
				ast_mutex_unlock(&p_old->lock);
			}
			ast_mutex_unlock(&iflock);
		}
		if (!p->expiry)
			ast_set_flag(p, SIP_NEEDDESTROY);
	}
	return 1;
}

/*! \brief  handle_request_register: Handle incoming REGISTER request ---*/
static int handle_request_register(struct sip_pvt *p, struct sip_request *req, int debug, int ignore, struct sockaddr_in *sin, char *e)
{
	int res = 0;
	char iabuf[INET_ADDRSTRLEN];

	/* Use this as the basis */
	if (debug)
		ast_verbose("Using latest REGISTER request as basis request\n");
	copy_request(&p->initreq, req);
	check_via(p, req);
	if ((res = register_verify(p, sin, req, e, ignore)) < 0) 
		ast_log(LOG_NOTICE, "Registration from '%s' failed for '%s' - %s\n", get_header(req, "To"), ast_inet_ntoa(iabuf, sizeof(iabuf), sin->sin_addr), (res == -1) ? "Wrong password" : (res == -2 ? "Username/auth name mismatch" : "Not a local SIP domain"));
	if (res < 1) {
		/* Destroy the session, but keep us around for just a bit in case they don't
		   get our 200 OK */
		sip_scheddestroy(p, 15*1000);
	}
	return res;
}

static void jdsp_enabled_message_waiting_notify(char *remotemailbox, int activate)
{
#ifdef DSP_ENABLED
    extern void jdsp_message_waiting_notify(char *remotemailbox, int activate);
    jdsp_message_waiting_notify(remotemailbox, activate);
#endif
}

static void dect_enabled_message_waiting_notify(char *username, int activate)
{
#ifdef DECT_ENABLED
    extern void dect_message_waiting_notify(char *username, int activate);
    dect_message_waiting_notify(username, activate);
#endif
}

/*--- handle_request_notify: Handle incoming NOTIFY request ---*/
static int handle_request_notify(struct sip_pvt *p, struct sip_request *req, int debug, int ignore, struct sockaddr_in *sin, char *e)
{
    char *mwi;
    char *to = NULL;
    int has_message_waiting;
    char *content_type;
    char *user = "";
    char *msg;
    
	if (!strncasecmp(req->line[0], "SIP/2.0 200 OK", 14))
	{
	    	msg = "200 OK";
	    	goto Exit;
	}
	content_type = get_header(req, "Content-Type");
	if (strcmp(content_type, "application/simple-message-summary")) 	
	{
		/* for now, we don't know how to handle xml and stuff */
		if (debug)
			ast_log(LOG_WARNING, "Don't know what to do, didn't get content-type application/simple-message-summary\n");
		msg = "489 Bad Event"; 
		goto Exit;	
	}
	mwi = get_body(req, "Messages-Waiting");
	if (ast_strlen_zero(mwi))
	{
		if (debug)
			ast_log(LOG_WARNING, "There is no Messages-Waiting field in NOTIFY packet\n");
		msg = "489 Bad Event"; 
		goto Exit;
	}

	if (!strcasecmp(mwi, "yes"))
		has_message_waiting = 1;
	else if (!strcasecmp(mwi, "no"))
		has_message_waiting = 0;
	else
	{
		if (debug)
			ast_log(LOG_WARNING, "Message-Waiting header has some unknown value %s\n", mwi);
		msg = "489 Bad Request"; 
		goto Exit;
	}
	/* Get the user that got the notification */
	to = strdup(get_header(req, "To"));

	if (!*to) /* we got "" */
	{
		if (debug)
			ast_log(LOG_WARNING, "\"To\" header is missing from NOTIFY request\n");
		msg = "489 Bad Request"; 
		goto Exit;
	}
	else 
	{
		char *tmp = to;

		strsep(&tmp, ":");
		user = strsep(&tmp, "@");
	}

	msg = "481 Subscription does not exist";

	/* Find the user in the registerd users list.
	 * We search for username AND callid, since we can get a NOTIFY with 
	 * the username before we get a 2XX response on the registration */
	ASTOBJ_CONTAINER_TRAVERSE(&regl, 1, do {
		ASTOBJ_RDLOCK(iterator);
		if (!strncmp(iterator->username, user, strlen(user))) /* gotcha!! */
		{
			struct sip_peer *peer = find_peer(user, NULL, 1);

			if (peer && peer->remotemailbox)
			{
				if (is_mwi_external)
				{
					old_msg_count = new_msg_count;
					new_msg_count = has_message_waiting;
				}
				ast_app_set_remote_vm(peer->remotemailbox, has_message_waiting);
				jdsp_enabled_message_waiting_notify(peer->remotemailbox, 
					has_message_waiting);
				dect_enabled_message_waiting_notify(user, has_message_waiting);
				msg = "200 OK";
			}
			if (peer)
				ASTOBJ_UNREF(peer, sip_destroy_peer);
		}
		ASTOBJ_UNLOCK(iterator);
	} while(0));

Exit:
	build_contact(p);
	transmit_response(p, msg, req);
	if (!p->lastinvite) 
		ast_set_flag(p, SIP_NEEDDESTROY);
	free(to);
	return 0;
}

/*--- handle_request_prack: Handle incoming PRACK request ---*/
static int handle_request_prack(struct sip_pvt *p, struct sip_request *req, int debug)
{
    int res = 0;
    char *rack_header = get_header(req, "RAck");
    unsigned int rack_no, seq_no;
    char method[128];
   
    if (sscanf(rack_header, "%u %u %s", &rack_no, &seq_no, method) == 3 &&
	rack_no &&
	(p->prack_expected_rack == rack_no)) /* Is the the expected PRACK? */
    {
	if (!ast_strlen_zero(get_header(req, "Content-Type")))
	    process_sdp(p, req);
	__sip_ack_prack(p, rack_no);
	transmit_response(p, "200 OK", req);
    }
    else
	transmit_response(p, "481 Call leg/transaction does not exist", req);

    return res;
}

/*! \brief  handle_request: Handle SIP requests (methods) ---*/
/*      this is where all incoming requests go first   */
static int handle_request(struct sip_pvt *p, struct sip_request *req, struct sockaddr_in *sin, int *recount, int *nounlock)
{
	/* Called with p->lock held, as well as p->owner->lock if appropriate, keeping things
	   relatively static */
	struct sip_request resp;
	char *cmd;
	char *cseq;
	char *useragent;
	int seqno;
	int len;
	int ignore=0;
	int respid;
	int res = 0;
	char iabuf[INET_ADDRSTRLEN];
	int debug = sip_debug_test_pvt(p);
	char *e;
	int error = 0;

	/* Clear out potential response */
	memset(&resp, 0, sizeof(resp));

	/* Get Method and Cseq */
	cseq = get_header(req, "Cseq");
	cmd = req->header[0];

	/* Must have Cseq */
	if (ast_strlen_zero(cmd) || ast_strlen_zero(cseq)) {
		ast_log(LOG_ERROR, "Missing Cseq. Dropping this SIP message, it's incomplete.\n");
		error = 1;
	}
	if (!error && sscanf(cseq, "%d%n", &seqno, &len) != 1) {
		ast_log(LOG_ERROR, "No seqno in '%s'. Dropping incomplete message.\n", cmd);
		error = 1;
	}
	if (error) {
		if (!p->initreq.header)	/* New call */
			ast_set_flag(p, SIP_NEEDDESTROY);	/* Make sure we destroy this dialog */
		return -1;
	}
	/* Get the command XXX */

	cmd = req->rlPart1;
	e = req->rlPart2;

	/* Save useragent of the client */
	useragent = get_header(req, "User-Agent");
	if (!ast_strlen_zero(useragent))
		ast_copy_string(p->useragent, useragent, sizeof(p->useragent));

	/* Find out SIP method for incoming request */
	if (req->method == SIP_RESPONSE) {	/* Response to our request */
		/* Response to our request -- Do some sanity checks */	
		if (!p->initreq.headers) {
			ast_log(LOG_DEBUG, "That's odd...  Got a response on a call we dont know about. Cseq %d Cmd %s\n", seqno, cmd);
			ast_set_flag(p, SIP_NEEDDESTROY);	
			return 0;
		} else if (p->ocseq && (p->ocseq < seqno) && p->lastinvite && (p->lastinvite < seqno) && p->lastprack && ((p->lastprack < seqno))) {
			ast_log(LOG_DEBUG, "Ignoring out of order response %d (expecting %d)\n", seqno, p->ocseq);
			return -1;
		} else if (p->ocseq && (p->ocseq != seqno && p->lastinvite && (p->lastinvite != seqno) && p->lastprack && (p->lastprack != seqno))) {
			/* ignore means "don't do anything with it" but still have to 
			   respond appropriately  */
			ignore=1;
		} else if (e) {
			e = ast_skip_blanks(e);
			if (sscanf(e, "%d %n", &respid, &len) != 1) {
				ast_log(LOG_WARNING, "Invalid response: '%s'\n", e);
			} else {
				/* More SIP ridiculousness, we have to ignore bogus contacts in 100 etc responses */
				/* XXX we need 1xx for freenet */
				if ((respid > 100 && respid <= 200) || ((respid >= 300) && (respid <= 399)))
					extract_uri(p, req);
				handle_response(p, respid, e + len, req, ignore, seqno);
			}
		}
		return 0;
	}

	/* New SIP request coming in 
	   (could be new request in existing SIP dialog as well...) 
	 */			
	
	p->method = req->method;	/* Find out which SIP method they are using */
	if (option_debug > 2)
		ast_log(LOG_DEBUG, "**** Received %s (%d) - Command in SIP %s\n", sip_methods[p->method].text, sip_methods[p->method].id, cmd); 

	if (p->icseq && (p->icseq > seqno)) {
		/* The check for monotonically increasing CSeq numbers fails
		 * when PRACK is enabled:
		 *     INVITE has CSeq 102
		 *     PRACK(s) have CSeq 103 ...
		 *     ACK has CSeq 102! */
#if 0
		if (option_debug)
			ast_log(LOG_DEBUG, "Ignoring too old SIP packet packet %d (expecting >= %d)\n", seqno, p->icseq);
		if (req->method != SIP_ACK)
			transmit_response(p, "503 Server error", req);	/* We must respond according to RFC 3261 sec 12.2 */
		return -1;
#endif
	} else if (p->icseq && (p->icseq == seqno) && req->method != SIP_ACK &&(p->method != SIP_CANCEL|| ast_test_flag(p, SIP_ALREADYGONE))) {
		/* ignore means "don't do anything with it" but still have to 
		   respond appropriately.  We do this if we receive a repeat of
		   the last sequence number  */
		ignore=2;
		if (option_debug > 2)
			ast_log(LOG_DEBUG, "Ignoring SIP message because of retransmit (%s Seqno %d, ours %d)\n", sip_methods[p->method].text, p->icseq, seqno);
	}
		
	if (seqno >= p->icseq)
		/* Next should follow monotonically (but not necessarily 
		   incrementally -- thanks again to the genius authors of SIP --
		   increasing */
		p->icseq = seqno;

	/* Find their tag if we haven't got it */
	if (ast_strlen_zero(p->theirtag)) {
		gettag(req, "From", p->theirtag, sizeof(p->theirtag));
	}
	snprintf(p->lastmsg, sizeof(p->lastmsg), "Rx: %s", cmd);

	if (pedanticsipchecking) {
		/* If this is a request packet without a from tag, it's not
			correct according to RFC 3261  */
		/* Check if this a new request in a new dialog with a totag already attached to it,
			RFC 3261 - section 12.2 - and we don't want to mess with recovery  */
		if (req->method != SIP_NOTIFY && !p->initreq.headers && ast_test_flag(req, SIP_PKT_WITH_TOTAG)) {
			/* If this is a first request and it got a to-tag, it is not for us */
			if (!ignore && req->method == SIP_INVITE) {
				transmit_response_reliable(p, "481 Call/Transaction Does Not Exist", req, 1);
				/* Will cease to exist after ACK */
			} else if (req->method != SIP_ACK) {
				transmit_response(p, "481 Call/Transaction Does Not Exist", req);
				ast_set_flag(p, SIP_NEEDDESTROY);
			}
			return res;
		}
	}

	if (!e && (p->method == SIP_INVITE || p->method == SIP_SUBSCRIBE || p->method == SIP_REGISTER)) {
		transmit_response(p, "400 Bad request", req);
		ast_set_flag(p, SIP_NEEDDESTROY);
		return -1;
	}

	/* Handle various incoming SIP methods in requests */
	switch (p->method) {
	case SIP_OPTIONS:
		res = handle_request_options(p, req, debug);
		break;
	case SIP_UPDATE:
	case SIP_INVITE:
		res = handle_request_invite(p, req, debug, ignore, seqno, sin, recount, e);
		break;
	case SIP_REFER:
		res = handle_request_refer(p, req, debug, ignore, seqno, nounlock);
		break;
	case SIP_CANCEL:
		res = handle_request_cancel(p, req, debug, ignore);
		break;
	case SIP_BYE:
		res = handle_request_bye(p, req, debug, ignore);
		break;
	case SIP_MESSAGE:
		res = handle_request_message(p, req, debug, ignore, sin, e);
		break;
	case SIP_SUBSCRIBE:
		res = handle_request_subscribe(p, req, debug, ignore, sin, seqno, e);
		break;
	case SIP_REGISTER:
		res = handle_request_register(p, req, debug, ignore, sin, e);
		break;
	case SIP_INFO:
		if (!ignore) {
			if (debug)
				ast_verbose("Receiving INFO!\n");
			handle_request_info(p, req);
		} else { /* if ignoring, transmit response */
			transmit_response(p, "200 OK", req);
		}
		break;
	case SIP_NOTIFY:
		res = handle_request_notify(p, req, debug, ignore, sin, e);
		break;
	case SIP_ACK:
		/* Make sure we don't ignore this */
		if (seqno == p->pendinginvite) {
			p->pendinginvite = 0;
			__sip_ack(p, seqno, FLAG_RESPONSE, 0);
			if (!ast_strlen_zero(get_header(req, "Content-Type"))) {
				if (process_sdp(p, req))
					return -1;
			} 
			check_pendings(p);
		} else if (p->glareinvite == seqno) {
			/* handle ack for the 491 pending send for glareinvite */
			p->glareinvite = 0;
			__sip_ack(p, seqno, 1, 0);
		}
		if (!p->lastinvite && ast_strlen_zero(p->randdata) && p->autokillid < 0)
			ast_set_flag(p, SIP_NEEDDESTROY);	
		break;
	case SIP_PRACK:
		res = handle_request_prack(p, req, debug);
		break;	
	default:
		transmit_response_with_allow(p, "501 Method Not Implemented", req, 0);
		ast_log(LOG_NOTICE, "Unknown SIP command '%s' from '%s'\n", 
			cmd, ast_inet_ntoa(iabuf, sizeof(iabuf), p->sa.sin_addr));
		/* If this is some new method, and we don't have a call, destroy it now */
		if (!p->initreq.headers)
			ast_set_flag(p, SIP_NEEDDESTROY);	
		break;
	}
	return res;
}

/*! \brief  sipsock_read: Read data from SIP socket ---*/
/*    Successful messages is connected to SIP call and forwarded to handle_request() */
static int sipsock_read(int *id, int fd, short events, void *ignore)
{
	struct sip_request req;
	struct sockaddr_in sin = { 0, };
	struct sip_pvt *p;
	int res;
	socklen_t len;
	int nounlock;
	int recount = 0;
	char src[256];
	char diag_data[SIP_MAX_PACKET];
	struct sip_peer *peer = NULL;
	struct sip_registry *registry = NULL;

	len = sizeof(sin);
	memset(&req, 0, sizeof(req));
	res = recvfrom(sipsock, req.data, sizeof(req.data) - 1, 0, (struct sockaddr *)&sin, &len);
	if (res < 0) {
#if !defined(__FreeBSD__)
		if (errno == EAGAIN)
			ast_log(LOG_NOTICE, "SIP: Received packet with bad UDP checksum\n");
		else 
#endif
		if (errno != ECONNREFUSED)
			ast_log(LOG_WARNING, "Recv error: %s\n", strerror(errno));
		return 1;
	}
	if (res == sizeof(req.data)) {
		ast_log(LOG_DEBUG, "Received packet exceeds buffer. Data is possibly lost\n");
	}
	req.data[res] = '\0';
	req.len = res;
	if(sip_debug_test_addr(&sin))
		ast_set_flag(&req, SIP_PKT_DEBUG);
	if (pedanticsipchecking)
		req.len = lws2sws(req.data, req.len);	/* Fix multiline headers */
	addr2str(sin, src, sizeof(src));

	if (!global_allow_p2p_calls && !(peer = find_peer(NULL, &sin, 1)) &&
	    !(registry = find_registry_by_addr(&sin)) && !(register_whitelist && ast_apply_ha_default(register_whitelist, &sin, AST_SENSE_DENY)))
	{
		time_t nowtime;
		unsigned long hash_idx = sin.sin_addr.s_addr % BLOCKED_ADDR_HASH_SIZE;

		time(&nowtime);
		if (nowtime - sip_blocked_addr_hash[hash_idx] > BLOCKED_ADDR_MSG_SUPPRESS_TIME_SEC)
		{
			ast_log(LOG_DEBUG, "Received packet from unknown IP %s, suppressing for %d sec\n", src, BLOCKED_ADDR_MSG_SUPPRESS_TIME_SEC);
			sip_blocked_addr_hash[hash_idx] = nowtime;
		}
		return 1;
	}
	if (peer)
		ASTOBJ_UNREF(peer, sip_destroy_peer);
	if (registry)
		ASTOBJ_UNREF(registry, sip_registry_destroy);

	if (ast_test_flag(&req, SIP_PKT_DEBUG)) {
		ast_verbose("\n<-- SIP read from %s: \n%s\n", src, req.data);
	} else if (option_debug > 4)
		ast_log(LOG_EVENT, "[in] from %s:\n%s\n", src, req.data);
	
	/* prepare untouched buffer for diagnostic calls */
	memcpy(diag_data, req.data, req.len+1);
	
	parse_request(&req);
	req.method = find_sip_method(req.rlPart1);
	if (ast_test_flag(&req, SIP_PKT_DEBUG)) {
		ast_verbose("--- (%d headers %d lines)", req.headers, req.lines);
		if (req.headers + req.lines == 0) 
			ast_verbose(" Nat keepalive ");
		ast_verbose("---\n");
	}

	if (req.headers < 2) {
		/* Must have at least two headers */
		return 1;
	}


	/* Process request, with netlock held */
retrylock:
	ast_mutex_lock(&iflock);
	ast_mutex_lock(&netlock);
	p = find_call(&req, &sin, req.method);
	if (p) {
		/* Go ahead and lock the owner if it has one -- we may need it */
		if (p->owner && ast_mutex_trylock(&p->owner->lock)) {
			ast_log(LOG_DEBUG, "Failed to grab lock, trying again...\n");
			ast_mutex_unlock(&p->lock);
			ast_mutex_unlock(&netlock);
			ast_mutex_unlock(&iflock);
			/* Sleep infintismly short amount of time */
			usleep(1);
			goto retrylock;
		}
		if (pbx_builtin_getvar_helper(p->owner, "DIAGNOSTIC_CALL"))
		    manager_event_diagnostic_trace(diag_data, req.len, 0);
	
		memcpy(&p->recv, &sin, sizeof(p->recv));
		if (recordhistory) {
			char tmp[80];
			/* This is a response, note what it was for */
			snprintf(tmp, sizeof(tmp), "%s / %s", req.data, get_header(&req, "CSeq"));
			append_history(p, "Rx", tmp);
		}
		nounlock = 0;
		if (handle_request(p, &req, &sin, &recount, &nounlock) == -1) {
			/* Request failed */
			ast_log(LOG_DEBUG, "SIP message could not be handled, bad request: %-70.70s\n", p->callid[0] ? p->callid : "<no callid>");
		}
		
		if (p->owner && !nounlock)
			ast_mutex_unlock(&p->owner->lock);
		ast_mutex_unlock(&p->lock);
	}
	ast_mutex_unlock(&netlock);
	ast_mutex_unlock(&iflock);
	if (recount)
		ast_update_use_count();

	return 1;
}

/*! \brief  sip_send_mwi_to_peer: Send message waiting indication ---*/
static int sip_send_mwi_to_peer(struct sip_peer *peer)
{
	/* Called with peerl lock, but releases it */
	struct sip_pvt *p;
	int newmsgs, oldmsgs;

	/* Check for messages */
	if (is_mwi_external)
	{
		oldmsgs = old_msg_count;
		newmsgs = new_msg_count;
	}
	else
		ast_app_messagecount(peer->mailbox, &newmsgs, &oldmsgs);
	
	time(&peer->lastmsgcheck);
	
	/* Return now if it's the same thing we told them last time */
	if (((newmsgs << 8) | (oldmsgs)) == peer->lastmsgssent) {
		return 0;
	}
	
	p = sip_alloc(NULL, NULL, 0, SIP_NOTIFY);
	if (!p) {
		ast_log(LOG_WARNING, "Unable to build sip pvt data for MWI\n");
		return -1;
	}
	peer->lastmsgssent = ((newmsgs << 8) | (oldmsgs));
	if (create_addr_from_peer(p, peer)) {
		/* Maybe they're not registered, etc. */
		sip_destroy(p);
		return 0;
	}
	/* Recalculate our side, and recalculate Call ID */
	if (ast_sip_ouraddrfor(&p->sa.sin_addr,&p->ourip))
		memcpy(&p->ourip, &__ourip, sizeof(p->ourip));
	build_via(p, p->via, sizeof(p->via));
	build_callid(p->callid, sizeof(p->callid), p->ourip, p->fromdomain);
	/* Send MWI */
	ast_set_flag(p, SIP_OUTGOING);
	transmit_notify_with_mwi(p, newmsgs, oldmsgs, peer->vmexten);
	sip_scheddestroy(p, 15000);
	return 0;
}

/*! \brief  do_monitor: The SIP monitoring thread ---*/
static void *do_monitor(void *data)
{
	int res;
	struct sip_pvt *sip;
	struct sip_peer *peer = NULL;
	time_t t;
	int fastrestart =0;
	int lastpeernum = -1;
	int curpeernum;
	int reloading;

	/* Add an I/O event to our UDP socket */
	if (sipsock > -1) 
		sipsock_read_id = ast_io_add(io, sipsock, sipsock_read, AST_IO_IN, NULL);
	
	/* This thread monitors all the frame relay interfaces which are not yet in use
	   (and thus do not have a separate thread) indefinitely */
	/* From here on out, we die whenever asked */
	for(;;) {
		/* Check for a reload request */
		ast_mutex_lock(&sip_reload_lock);
		reloading = sip_reloading;
		sip_reloading = 0;
		ast_mutex_unlock(&sip_reload_lock);
		if (reloading) {
			if (option_verbose > 0)
				ast_verbose(VERBOSE_PREFIX_1 "Reloading SIP\n");
			sip_do_reload();

			/* Change the I/O fd of our UDP socket */
			if (sipsock > -1) {
				if (sipsock_read_id)
					sipsock_read_id = ast_io_change(io, sipsock_read_id, sipsock, NULL, 0, NULL);
				else
					sipsock_read_id = ast_io_add(io, sipsock, sipsock_read, AST_IO_IN, NULL);
			}
			else if (sipsock_read_id)
			{
				ast_io_remove(io, sipsock_read_id);
				sipsock_read_id = NULL;
			}
		}
		/* Check for interfaces needing to be killed */
		ast_mutex_lock(&iflock);
restartsearch:		
		time(&t);
		sip = iflist;
		while(sip) {
			ast_mutex_lock(&sip->lock);
			if (sip->rtp && sip->owner && (sip->owner->_state == AST_STATE_UP) && !sip->redirip.sin_addr.s_addr) {
				if (sip->lastrtptx && sip->rtpkeepalive && t > sip->lastrtptx + sip->rtpkeepalive) {
					/* Need to send an empty RTP packet */
					time(&sip->lastrtptx);
					ast_rtp_sendcng(sip->rtp, 0);
				}
				if (sip->lastrtprx && (sip->rtptimeout || sip->rtpholdtimeout) && t > sip->lastrtprx + sip->rtptimeout) {
					/* Might be a timeout now -- see if we're on hold */
					struct sockaddr_in sin;
					ast_rtp_get_peer(sip->rtp, &sin);
					if (sin.sin_addr.s_addr || 
							(sip->rtpholdtimeout && 
							  (t > sip->lastrtprx + sip->rtpholdtimeout))) {
						/* Needs a hangup */
						if (sip->rtptimeout) {
							while(sip->owner && ast_mutex_trylock(&sip->owner->lock)) {
								ast_mutex_unlock(&sip->lock);
								usleep(1);
								ast_mutex_lock(&sip->lock);
							}
							if (sip->owner) {
								ast_log(LOG_NOTICE, "Disconnecting call '%s' for lack of RTP activity in %ld seconds\n", sip->owner->name, (long)(t - sip->lastrtprx));
								/* Issue a softhangup */
								ast_softhangup(sip->owner, AST_SOFTHANGUP_DEV);
								ast_mutex_unlock(&sip->owner->lock);
							}
						}
					}
				}
			}
			if (ast_test_flag(sip, SIP_NEEDDESTROY) && !sip->packets && !sip->owner) {
				ast_mutex_unlock(&sip->lock);
				__sip_destroy(sip, 1);
				goto restartsearch;
			}
			ast_mutex_unlock(&sip->lock);
			sip = sip->next;
		}
		ast_mutex_unlock(&iflock);
		/* Don't let anybody kill us right away.  Nobody should lock the interface list
		   and wait for the monitor list, but the other way around is okay. */
		ast_mutex_lock(&monlock);
		/* Lock the network interface */
		ast_mutex_lock(&netlock);
		/* Okay, now that we know what to do, release the network lock */
		ast_mutex_unlock(&netlock);
		/* And from now on, we're okay to be killed, so release the monitor lock as well */
		ast_mutex_unlock(&monlock);
		pthread_testcancel();
		/* Wait for sched or io */
		res = ast_sched_wait(sched);
		if ((res < 0) || (res > 1000))
			res = 1000;
		/* If we might need to send more mailboxes, don't wait long at all.*/
		if (fastrestart)
			res = 1;
		res = ast_io_wait(io, res);
		if (res > 20)
			ast_log(LOG_DEBUG, "chan_sip: ast_io_wait ran %d all at once\n", res);
		ast_mutex_lock(&monlock);
		if (res >= 0)  {
			res = ast_sched_runq(sched);
			if (res >= 20)
				ast_log(LOG_DEBUG, "chan_sip: ast_sched_runq ran %d all at once\n", res);
		}

		/* needs work to send mwi to realtime peers */
		time(&t);
		fastrestart = 0;
		curpeernum = 0;
		peer = NULL;
		ASTOBJ_CONTAINER_TRAVERSE(&peerl, !peer, do {
			if ((curpeernum > lastpeernum) && !ast_strlen_zero(iterator->mailbox) && ((t - iterator->lastmsgcheck) > global_mwitime)) {
				fastrestart = 1;
				lastpeernum = curpeernum;
				peer = ASTOBJ_REF(iterator);
			};
			curpeernum++;
		} while (0)
		);
		if (peer) {
			ASTOBJ_WRLOCK(peer);
			sip_send_mwi_to_peer(peer);
			ASTOBJ_UNLOCK(peer);
			ASTOBJ_UNREF(peer,sip_destroy_peer);
		} else {
			/* Reset where we come from */
			lastpeernum = -1;
		}
		ast_mutex_unlock(&monlock);
	}
	/* Never reached */
	return NULL;
	
}

/*! \brief  restart_monitor: Start the channel monitor thread ---*/
static int restart_monitor(void)
{
	pthread_attr_t attr;
	/* If we're supposed to be stopped -- stay stopped */
	if (monitor_thread == AST_PTHREADT_STOP)
		return 0;
	if (ast_mutex_lock(&monlock)) {
		ast_log(LOG_WARNING, "Unable to lock monitor\n");
		return -1;
	}
	if (monitor_thread == pthread_self()) {
		ast_mutex_unlock(&monlock);
		ast_log(LOG_WARNING, "Cannot kill myself\n");
		return -1;
	}
	if (monitor_thread != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(monitor_thread, SIGURG);
	} else {
		pthread_attr_init(&attr);
#if 0
		/* This causes pthread_join to fail */
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#endif
		/* Start a new monitor */
		if (ast_pthread_create(&monitor_thread, &attr, do_monitor, NULL) < 0) {
			ast_mutex_unlock(&monlock);
			ast_log(LOG_ERROR, "Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&monlock);
	return 0;
}

/*! \brief Session-Timers: Restart session timer */
static void restart_session_timer(struct sip_pvt *p)
{
	if (!p->stimer) {
		ast_log(LOG_WARNING, "Null stimer in restart_session_timer - %s\n", p->callid);
		return;
	}

	if (p->stimer->st_active == TRUE) {
		if (ast_sched_del(sched, p->stimer->st_schedid) != 0) {
			ast_log(LOG_WARNING, "ast_sched_del failed: %d - %s\n", p->stimer->st_schedid, p->callid);
		}

		ast_log(LOG_DEBUG, "Session timer stopped: %d - %s\n", p->stimer->st_schedid, p->callid);
		start_session_timer(p);
	}
}


/*! \brief Session-Timers: Stop session timer */
static void stop_session_timer(struct sip_pvt *p)
{
	if (!p->stimer) {
		ast_log(LOG_WARNING, "Null stimer in stop_session_timer - %s\n", p->callid);
		return;
	}

	if (p->stimer->st_active == TRUE) {
		p->stimer->st_active = FALSE;
		ast_sched_del(sched, p->stimer->st_schedid);
		ast_log(LOG_DEBUG, "Session timer stopped: %d - %s\n", p->stimer->st_schedid, p->callid);
		p->stimer->st_schedid = -1;
	}
}


/*! \brief Session-Timers: Start session timer */
static void start_session_timer(struct sip_pvt *p)
{
	if (!p->stimer) {
		ast_log(LOG_WARNING, "Null stimer in start_session_timer - %s\n", p->callid);
		return;
	}

	p->stimer->st_schedid  = ast_sched_add(sched, p->stimer->st_interval * 1000 / 2, proc_session_timer, p);
	if (p->stimer->st_schedid < 0) {
		ast_log(LOG_ERROR, "ast_sched_add failed.\n");
	}
	ast_log(LOG_DEBUG, "Session timer started: %d - %s\n", p->stimer->st_schedid, p->callid);
}


/*! \brief Session-Timers: Process session refresh timeout event */
static int proc_session_timer(void *vp)
{
	struct sip_pvt *p = (struct sip_pvt *) vp;

	if (!p->stimer) {
		ast_log(LOG_WARNING, "Null stimer in proc_session_timer - %s\n", p->callid);
		return 0;
	}

	ast_log(LOG_DEBUG, "Session timer expired: %d - %s\n", p->stimer->st_schedid, p->callid);

	if (!p->owner) {
		if (p->stimer->st_active == TRUE) {
			stop_session_timer(p);
		}
		return 0;
	}

	if ((p->stimer->st_active != TRUE) || (p->owner->_state != AST_STATE_UP)) {
		return 0;
	}

	if (p->stimer->st_ref == SESSION_TIMER_REFRESHER_LOCAL) {
		if (ast_test_flag(&(p->flags_page2),
		    SIP_PAGE2_SESSION_REFRESH_UPDATE)) 
		{
			transmit_reinvite(p, 1, 0, SIP_UPDATE);	
		}
		else
		{
			if (p->t38state == T38_ENABLED)
				transmit_reinvite_with_t38_sdp(p, TRUE);
			else
				transmit_reinvite_with_sdp(p, TRUE);

		}
	} else if (p->stimer->st_ref == SESSION_TIMER_REFRESHER_REMOTE) {
		p->stimer->st_expirys++;
		if (p->stimer->st_expirys >= 2) {
			ast_log(LOG_WARNING, "Session-Timer expired - %s\n", p->callid);
			stop_session_timer(p);

			/* XXX ??? */
			ast_mutex_lock(&p->lock);
			while (p->owner && ast_mutex_trylock(&p->owner->lock)) {
				ast_mutex_unlock(&p->lock);
				usleep(1);
				ast_mutex_lock(&p->lock);
			}

			ast_softhangup_nolock(p->owner, AST_SOFTHANGUP_DEV);
			ast_mutex_unlock(&p->owner->lock);
			ast_mutex_unlock(&p->lock);
		}
	}
	else
	{
		ast_log(LOG_ERROR, "Unknown session refresher %d\n", p->stimer->st_ref);
		return 0;
	}

	return 1;
}


/* Session-Timers: Function for parsing Min-SE header */
int parse_minse (char *p_hdrval, int *const p_interval)
{
	if (ast_strlen_zero(p_hdrval)) {
		ast_log(LOG_WARNING, "Null Min-SE header\n");
		return -1;
	}

	*p_interval = 0;
	p_hdrval = ast_skip_blanks(p_hdrval);
	if (!sscanf(p_hdrval, "%d", p_interval)) {
		ast_log(LOG_WARNING, "Parsing of Min-SE header failed %s\n", p_hdrval);
		return -1;
	}

	ast_log(LOG_DEBUG, "Received Min-SE: %d\n", *p_interval);
	return 0;
}


/* Session-Timers: Function for parsing Session-Expires header */
int parse_session_expires(const char *p_hdrval, int *const p_interval, enum st_refresher *const p_ref, int uas)
{
	char *p_token;
	int  ref_idx;
	char *p_se_hdr;

	if (ast_strlen_zero(p_hdrval)) {
		ast_log(LOG_WARNING, "Null Session-Expires header\n");
		return -1;
	}

	*p_ref = SESSION_TIMER_REFRESHER_AUTO;
	*p_interval = 0;

	p_se_hdr = ast_strdupa(p_hdrval);
	p_se_hdr = ast_skip_blanks(p_se_hdr);

	while ((p_token = strsep(&p_se_hdr, ";"))) {
		p_token = ast_skip_blanks(p_token);
		if (!sscanf(p_token, "%d", p_interval)) {
			ast_log(LOG_WARNING, "Parsing of Session-Expires failed\n");
			return -1;
		}

		ast_log(LOG_DEBUG, "Session-Expires: %d\n", *p_interval);

		if (!p_se_hdr)
			continue;
		
		ref_idx = strlen("refresher=");
		if (!strncasecmp(p_se_hdr, "refresher=", ref_idx)) {
			p_se_hdr += ref_idx;
			p_se_hdr = ast_skip_blanks(p_se_hdr);

			if (!strncasecmp(p_se_hdr, "uac", strlen("uac"))) {
				*p_ref = uas ? SESSION_TIMER_REFRESHER_REMOTE : SESSION_TIMER_REFRESHER_LOCAL;
				ast_log(LOG_DEBUG, "Refresher: %s\n", strefresher2str(*p_ref));
			} else if (!strncasecmp(p_se_hdr, "uas", strlen("uas"))) {
				*p_ref = uas ? SESSION_TIMER_REFRESHER_LOCAL : SESSION_TIMER_REFRESHER_REMOTE;
				ast_log(LOG_DEBUG, "Refresher: %s\n\n", strefresher2str(*p_ref));
			} else {
				ast_log(LOG_WARNING, "Invalid refresher value %s\n", p_se_hdr);
				return -1;
			}
			break;
		}
	}
	return 0;
}


/*! \brief Handle 422 response to INVITE with session-timer requested

   Session-Timers:   An INVITE originated by Asterisk that asks for session-timers support
   from the UAS can result into a 422 response. This is how a UAS or an intermediary proxy 
   server tells Asterisk that the session refresh interval offered by Asterisk is too low 
   for them.  The proc_422_rsp() function handles a 422 response.  It extracts the Min-SE 
   header that comes back in 422 and sends a new INVITE accordingly. */
static void proc_422_rsp(struct sip_pvt *p, struct sip_request *rsp)
{
	int rtn;
	char *p_hdrval;
	int minse;

	p_hdrval = get_header(rsp, "Min-SE");
	if (ast_strlen_zero(p_hdrval)) {
		ast_log(LOG_WARNING, "422 response without a Min-SE header %s\n", p_hdrval);
		return;
	}
	rtn = parse_minse(p_hdrval, &minse);
	if (rtn != 0) {
		ast_log(LOG_WARNING, "Parsing of Min-SE header failed %s\n", p_hdrval);
		return;
	}
	p->stimer->st_min_se = MAX(p->stimer->st_min_se, minse);
	transmit_invite(p, SIP_INVITE, 1, 2); 
}


/*! \brief Get Max or Min SE (session timer expiry)
	\param max if true, get max se, otherwise min se
*/
int st_get_se(struct sip_pvt *p, int max)
{
	if (max == TRUE) {
		if (p->stimer->st_cached_max_se) {
			return p->stimer->st_cached_max_se;
		} else {
			if (p->username) {
				struct sip_user *up = find_user(p->username, 1);
				if (up) {
					p->stimer->st_cached_max_se = up->stimer.st_max_se;
					ASTOBJ_UNREF(up, sip_destroy_user);
					return (p->stimer->st_cached_max_se);
				}
			} 
			if (p->peername) {
				struct sip_peer *pp = find_peer(p->peername, NULL, 1);
				if (pp) {
					p->stimer->st_cached_max_se = pp->stimer.st_max_se;
					ASTOBJ_UNREF(pp, sip_destroy_peer);
					return (p->stimer->st_cached_max_se);
				}
			}
		}
		p->stimer->st_cached_max_se = global_max_se;
		return (p->stimer->st_cached_max_se);
	} else {
		if (p->stimer->st_cached_min_se) {
			return p->stimer->st_cached_min_se;
		} else {
			if (p->username) {
				struct sip_user *up = find_user(p->username, 1);
				if (up) {
					p->stimer->st_cached_min_se = up->stimer.st_min_se;
					ASTOBJ_UNREF(up, sip_destroy_user);
					return (p->stimer->st_cached_min_se);
				}
			} 
			if (p->peername) {
				struct sip_peer *pp = find_peer(p->peername, NULL, 1);
				if (pp) {
					p->stimer->st_cached_min_se = pp->stimer.st_min_se;
					ASTOBJ_UNREF(pp, sip_destroy_peer);
					return (p->stimer->st_cached_min_se);
				}
			}
		}
		p->stimer->st_cached_min_se = global_min_se;
		return (p->stimer->st_cached_min_se);
	}
}


/*! \brief Get the entity (UAC or UAS) that's acting as the session-timer refresher 
	\param sip_pvt pointer to the SIP dialog 
*/
enum st_refresher st_get_refresher(struct sip_pvt *p)
{
	if (p->stimer->st_cached_ref != SESSION_TIMER_REFRESHER_AUTO) 
		return p->stimer->st_cached_ref;

	if (p->username) {
		struct sip_user *up = find_user(p->username, 1);
		if (up) {
			p->stimer->st_cached_ref = up->stimer.st_ref;
			ASTOBJ_UNREF(up, sip_destroy_user);
			return p->stimer->st_cached_ref;
		}
	} 

	if (p->peername) {
		struct sip_peer *pp = find_peer(p->peername, NULL, 1);
		if (pp) {
			p->stimer->st_cached_ref = pp->stimer.st_ref;
			ASTOBJ_UNREF(pp, sip_destroy_peer);
			return p->stimer->st_cached_ref;
		}
	}
	
	p->stimer->st_cached_ref = global_st_refresher;
	return global_st_refresher;
}


/*!
 * \brief Get the session-timer mode 
 * \param p pointer to the SIP dialog 
 * \param no_cached, set this to true in order to force a peername lookup on
 *        the session timer mode.
*/
enum st_mode st_get_mode(struct sip_pvt *p, int no_cached)
{
	if (!p->stimer) 
		sip_st_alloc(p);

	if (!no_cached && p->stimer->st_cached_mode != SESSION_TIMER_MODE_INVALID) 
		return p->stimer->st_cached_mode;

	if (p->username) {
		struct sip_user *up = find_user(p->username, 1);
		if (up) {
			p->stimer->st_cached_mode = up->stimer.st_mode_oper;
			ASTOBJ_UNREF(up, sip_destroy_user);
			return p->stimer->st_cached_mode;
		}
	} 
	if (p->peername) {
		struct sip_peer *pp = find_peer(p->peername, NULL, 1);
		if (pp) {
			p->stimer->st_cached_mode = pp->stimer.st_mode_oper;
			ASTOBJ_UNREF(pp, sip_destroy_peer);
			return p->stimer->st_cached_mode;
		}
	}

	p->stimer->st_cached_mode = global_st_mode;
	return global_st_mode;
}

/*! \brief  sip_poke_noanswer: No answer to Qualify poke ---*/
static int sip_poke_noanswer(void *data)
{
	struct sip_peer *peer = data;
	
	peer->pokeexpire = -1;
	if (peer->lastms > -1) {
		ast_log(LOG_NOTICE, "Peer '%s' is now UNREACHABLE!  Last qualify: %d\n", peer->name, peer->lastms);
		manager_event(EVENT_FLAG_SYSTEM, "PeerStatus", "Peer: SIP/%s\r\nPeerStatus: Unreachable\r\nTime: %d\r\n", peer->name, -1);
	}
	if (peer->call)
		sip_destroy(peer->call);
	peer->call = NULL;
	peer->lastms = -1;
	ast_device_state_changed("SIP/%s", peer->name);
	/* Try again quickly */
	peer->pokeexpire = ast_sched_add(sched, DEFAULT_FREQ_NOTOK, sip_poke_peer_s, peer);
	return 0;
}

/*! \brief  sip_poke_peer: Check availability of peer, also keep NAT open ---*/
/*	This is done with the interval in qualify= option in sip.conf */
/*	Default is 2 seconds */
static int sip_poke_peer(struct sip_peer *peer)
{
	struct sip_pvt *p;
	if (!peer->maxms || !peer->addr.sin_addr.s_addr ||
	    !resolve_peer_addr(peer)) {
		/* IF we have no IP, or this isn't to be monitored, return
		  immediately after clearing things out */
		if (peer->pokeexpire > -1)
			ast_sched_del(sched, peer->pokeexpire);
		peer->lastms = 0;
		peer->pokeexpire = -1;
		peer->call = NULL;
		return 0;
	}
	if (peer->call > 0) {
		if (sipdebug)
			ast_log(LOG_NOTICE, "Still have a QUALIFY dialog active, deleting\n");
		sip_destroy(peer->call);
	}
	p = peer->call = sip_alloc(NULL, NULL, 0, SIP_OPTIONS);
	if (!peer->call) {
		ast_log(LOG_WARNING, "Unable to allocate dialog for poking peer '%s'\n", peer->name);
		return -1;
	}
	memcpy(&p->sa, &peer->addr, sizeof(p->sa));
	memcpy(&p->recv, &peer->addr, sizeof(p->sa));
	if (!ast_strlen_zero(peer->regname))
	    ast_copy_string(p->regname, peer->regname, sizeof(p->regname));

	/* Send options to peer's fullcontact */
	if (!ast_strlen_zero(peer->fullcontact)) {
		ast_copy_string (p->fullcontact, peer->fullcontact, sizeof(p->fullcontact));
	}

	if (!ast_strlen_zero(peer->tohost))
	{
		ast_copy_string(p->tohost, peer->tohost, sizeof(p->tohost));
		p->toport = peer->toport;
	}
	else
	{
		ast_inet_ntoa(p->tohost, sizeof(p->tohost), peer->addr.sin_addr);
		p->toport = peer->addr.sin_port;
	}

	if (!ast_strlen_zero(peer->todomain))
		ast_copy_string(p->todomain, peer->todomain, sizeof(p->todomain));

	/* Recalculate our side, and recalculate Call ID */
	if (ast_sip_ouraddrfor(&p->sa.sin_addr,&p->ourip))
		memcpy(&p->ourip, &__ourip, sizeof(p->ourip));
	build_via(p, p->via, sizeof(p->via));
	build_callid(p->callid, sizeof(p->callid), p->ourip, p->fromdomain);

	if (peer->pokeexpire > -1)
		ast_sched_del(sched, peer->pokeexpire);
	p->peerpoke = peer;
	ast_set_flag(p, SIP_OUTGOING);
#ifdef VOCAL_DATA_HACK
	ast_copy_string(p->username, "__VOCAL_DATA_SHOULD_READ_THE_SIP_SPEC__", sizeof(p->username));
	transmit_invite(p, SIP_INVITE, 0, 2);
#else
	transmit_invite(p, SIP_OPTIONS, 0, 2);
#endif
	peer->ps = ast_tvfromboot();
	peer->pokeexpire = ast_sched_add(sched, DEFAULT_MAXMS * 2, sip_poke_noanswer, peer);

	return 0;
}

/*! \brief  sip_devicestate: Part of PBX channel interface ---*/

/* Return values:---
	If we have qualify on and the device is not reachable, regardless of registration
	state we return AST_DEVICE_UNAVAILABLE

	For peers with call limit:
		not registered			AST_DEVICE_UNAVAILABLE
		registered, no call		AST_DEVICE_NOT_INUSE
		registered, calls possible	AST_DEVICE_INUSE
		registered, call limit reached	AST_DEVICE_BUSY
	For peers without call limit:
		not registered			AST_DEVICE_UNAVAILABLE
		registered			AST_DEVICE_UNKNOWN
*/
static int sip_devicestate(void *data)
{
	char *host;
	char *tmp;

	struct hostent *hp;
	struct ast_hostent ahp;
	struct sip_peer *p;

	int res = AST_DEVICE_INVALID;

	host = ast_strdupa(data);
	if ((tmp = strchr(host, '@')))
		host = tmp + 1;

	if (option_debug > 2) 
		ast_log(LOG_DEBUG, "Checking device state for peer %s\n", host);

	if ((p = find_peer(host, NULL, 1))) {
		if (p->addr.sin_addr.s_addr || resolve_peer_addr(p) ||
		    p->defaddr.sin_addr.s_addr) {
			/* we have an address for the peer */
			/* if qualify is turned on, check the status */
			if (p->maxms && (p->lastms > p->maxms)) {
				res = AST_DEVICE_UNAVAILABLE;
			} else {
				/* qualify is not on, or the peer is responding properly */
				/* check call limit */
				if (p->call_limit && (p->inUse == p->call_limit))
					res = AST_DEVICE_BUSY;
				else if (p->call_limit && p->inUse)
					res = AST_DEVICE_INUSE;
				else if (p->call_limit)
					res = AST_DEVICE_NOT_INUSE;
				else
					res = AST_DEVICE_UNKNOWN;
			}
		} else {
			/* there is no address, it's unavailable */
			res = AST_DEVICE_UNAVAILABLE;
		}
		ASTOBJ_UNREF(p,sip_destroy_peer);
	} else {
	        for (tmp = host; *tmp && !strchr(":?", *tmp); tmp++);
		if (*tmp)
			*tmp = '\0';

		hp = ast_gethostbyname(host, &ahp);
		if (hp)
			res = AST_DEVICE_UNKNOWN;
	}

	return res;
}

/*! \brief  sip_request: PBX interface function -build SIP pvt structure ---*/
/* SIP calls initiated by the PBX arrive here */
static struct ast_channel *sip_request_call(const char *type, const struct ast_codec_pref *formats, void *data, int *cause)
{
	struct sip_pvt *p;
	struct sip_user *user;
	struct sip_peer *peer = NULL;
	struct ast_channel *tmpc = NULL;
	char *ext, *host;
	char tmp[256];
	char *dest = data;
	int inuse = 0, callwaiting = CALLWAITING_NONE, call_limit = 0;

	p = sip_alloc(NULL, NULL, 0, SIP_INVITE);
	if (!p) {
		ast_log(LOG_WARNING, "Unable to build sip pvt data for '%s'\n", (char *)data);
		return NULL;
	}

	ast_set_flag(&p->flags_page2, SIP_PAGE2_OUTGOING_CALL);

	p->options = calloc(1, sizeof(*p->options));
	if (!p->options) {
		ast_log(LOG_ERROR, "Out of memory\n");
		return NULL;
	}

	/* Ignore callback forced by manager.c:prepare_extensions */
	if (strstr((char *)data, CALLBACK_MAGIC))
		ast_copy_string(tmp, dest + strlen(CALLBACK_MAGIC), sizeof(tmp));
	else
		ast_copy_string(tmp, dest, sizeof(tmp));
	host = strchr(tmp, '@');
	if (host) {
		*host = '\0';
		host++;
		ext = tmp;
	} else {
		ext = strchr(tmp, '/');
		if (ext) {
			*ext++ = '\0';
			host = tmp;
		}
		else {
			host = tmp;
			ext = NULL;
		}
	}
	if (create_addr(p, host)) {
		*cause = AST_CAUSE_UNREGISTERED;
		sip_destroy(p);
		return NULL;
	}

	if ((user = find_user(host, 1))) {
		/* Copying the CID from user to pvt data structure. Generally this is
		 * done in order to prevent a situation when the PBX initiates a call,
		 * and there is no CID in the SIP pvt data structure */
		ast_copy_string(p->cid_num, user->cid_num, sizeof(user->cid_num));
		ast_copy_string(p->cid_name, user->cid_name, sizeof(user->cid_name));
		inuse = user->inUse;
		call_limit = user->call_limit;
		callwaiting = user->callwaiting;
		ASTOBJ_UNREF(user, sip_destroy_user);
	} else if ((peer = find_peer(host, NULL, 1))) {
		inuse = peer->inUse;
		call_limit = peer->call_limit;
		callwaiting = peer->callwaiting;
		ASTOBJ_UNREF(peer, sip_destroy_peer);
	}
	if ((inuse && callwaiting == CALLWAITING_OFF) ||
	    (call_limit && inuse >= call_limit && callwaiting == CALLWAITING_ON)) {
	    	*cause = AST_CAUSE_BUSY;
		sip_destroy(p);
	    	return NULL;
	}

	ast_codec_pref_combine(&p->formats, formats, p->usercapability);

	if (ast_strlen_zero(p->peername) && ext)
		ast_copy_string(p->peername, ext, sizeof(p->peername));
	/* Recalculate our side, and recalculate Call ID */
	if (ast_sip_ouraddrfor(&p->sa.sin_addr,&p->ourip))
		memcpy(&p->ourip, &__ourip, sizeof(p->ourip));
	build_via(p, p->via, sizeof(p->via));
	build_callid(p->callid, sizeof(p->callid), p->ourip, p->fromdomain);
	
	/* We have an extension to call, don't use the full contact here */
	/* This to enable dialling registered peers with extension dialling,
	   like SIP/peername/extension 	
	   SIP/peername will still use the full contact */
	if (ext) {
		ast_copy_string(p->username, ext, sizeof(p->username));
		p->fullcontact[0] = 0;	
	}
#if 0
	printf("Setting up to call extension '%s' at '%s'\n", ext ? ext : "<none>", host);
#endif
	ast_mutex_lock(&p->lock);
	tmpc = sip_new(p, AST_STATE_DOWN, host, 0);	/* Place the call */
	ast_mutex_unlock(&p->lock);
	if (!tmpc)
		sip_destroy(p);
	ast_update_use_count();
	restart_monitor();
	return tmpc;
}

/*! \brief  handle_common_options: Handle flag-type options common to users and peers ---*/
static int handle_common_options(struct ast_flags *flags, struct ast_flags *mask, struct ast_variable *v)
{
	int res = 0;

	if (!strcasecmp(v->name, "trustrpid")) {
		ast_set_flag(mask, SIP_TRUSTRPID);
		ast_set2_flag(flags, ast_true(v->value), SIP_TRUSTRPID);
		res = 1;
	} else if (!strcasecmp(v->name, "sendrpid")) {
		ast_set_flag(mask, SIP_SENDRPID);
		ast_set2_flag(flags, ast_true(v->value), SIP_SENDRPID);
		res = 1;
	} else if (!strcasecmp(v->name, "useclientcode")) {
		ast_set_flag(mask, SIP_USECLIENTCODE);
		ast_set2_flag(flags, ast_true(v->value), SIP_USECLIENTCODE);
		res = 1;
	} else if (!strcasecmp(v->name, "dtmfmode")) {
		ast_set_flag(mask, SIP_DTMF);
		ast_clear_flag(flags, SIP_DTMF);
		if (!strcasecmp(v->value, "inband"))
			ast_set_flag(flags, SIP_DTMF_INBAND);
		else if (!strcasecmp(v->value, "rfc2833"))
			ast_set_flag(flags, SIP_DTMF_RFC2833);
		else if (!strcasecmp(v->value, "info"))
			ast_set_flag(flags, SIP_DTMF_INFO);
		else if (!strcasecmp(v->value, "auto"))
			ast_set_flag(flags, SIP_DTMF_AUTO);
		else {
			ast_log(LOG_WARNING, "Unknown dtmf mode '%s' on line %d, using rfc2833\n", v->value, v->lineno);
			ast_set_flag(flags, SIP_DTMF_RFC2833);
		}
	} else if (!strcasecmp(v->name, "compatmode")) {
		ast_set_flag(mask, SIP_COMPAT);
		ast_clear_flag(flags, SIP_COMPAT);
		if (!strcasecmp(v->value, "off"))
			ast_set_flag(flags, SIP_COMPAT_OFF);
		else if (!strcasecmp(v->value, "broadsoft"))
			ast_set_flag(flags, SIP_COMPAT_BROADSOFT);
		else {
			ast_log(LOG_WARNING, "Unknown compatibility mode '%s' on line %d, using off\n", v->value, v->lineno);
			ast_set_flag(flags, SIP_COMPAT_OFF);
		}
	} else if (!strcasecmp(v->name, "nat")) {
		ast_set_flag(mask, SIP_NAT);
		ast_clear_flag(flags, SIP_NAT);
		if (!strcasecmp(v->value, "never"))
			ast_set_flag(flags, SIP_NAT_NEVER);
		else if (!strcasecmp(v->value, "route"))
			ast_set_flag(flags, SIP_NAT_ROUTE);
		else if (ast_true(v->value))
			ast_set_flag(flags, SIP_NAT_ALWAYS);
		else
			ast_set_flag(flags, SIP_NAT_RFC3581);
	} else if (!strcasecmp(v->name, "canreinvite")) {
		ast_set_flag(mask, SIP_REINVITE);
		ast_clear_flag(flags, SIP_REINVITE);
		if (!strcasecmp(v->value, "update"))
			ast_set_flag(flags, SIP_REINVITE_UPDATE | SIP_CAN_REINVITE);
		else
			ast_set2_flag(flags, ast_true(v->value), SIP_CAN_REINVITE);
	} else if (!strcasecmp(v->name, "insecure")) {
		ast_set_flag(mask, SIP_INSECURE_PORT | SIP_INSECURE_INVITE);
		ast_clear_flag(flags, SIP_INSECURE_PORT | SIP_INSECURE_INVITE);
		if (!strcasecmp(v->value, "very"))
			ast_set_flag(flags, SIP_INSECURE_PORT | SIP_INSECURE_INVITE);
		else if (ast_true(v->value))
			ast_set_flag(flags, SIP_INSECURE_PORT);
		else if (!ast_false(v->value)) {
			char buf[64];
			char *word, *next;

			ast_copy_string(buf, v->value, sizeof(buf));
			next = buf;
			while ((word = strsep(&next, ","))) {
				if (!strcasecmp(word, "port"))
					ast_set_flag(flags, SIP_INSECURE_PORT);
				else if (!strcasecmp(word, "invite"))
					ast_set_flag(flags, SIP_INSECURE_INVITE);
				else
					ast_log(LOG_WARNING, "Unknown insecure mode '%s' on line %d\n", v->value, v->lineno);
			}
		}
	} else if (!strcasecmp(v->name, "progressinband")) {
		ast_set_flag(mask, SIP_PROG_INBAND);
		ast_clear_flag(flags, SIP_PROG_INBAND);
		if (ast_true(v->value))
			ast_set_flag(flags, SIP_PROG_INBAND_YES);
		else if (strcasecmp(v->value, "never"))
			ast_set_flag(flags, SIP_PROG_INBAND_NO);
  	} else if (!strcasecmp(v->name, "allowguest")) {
#ifdef OSP_SUPPORT
  		if (!strcasecmp(v->value, "osp"))
			global_allowguest = 2;
		else 
#endif
			if (ast_true(v->value)) 
				global_allowguest = 1;
			else
				global_allowguest = 0;
	} else if (!strcasecmp(v->name, "allow_p2p_calls")) {
		global_allow_p2p_calls = ast_true(v->value);
#ifdef OSP_SUPPORT
	} else if (!strcasecmp(v->name, "ospauth")) {
		ast_set_flag(mask, SIP_OSPAUTH);
		ast_clear_flag(flags, SIP_OSPAUTH);
		if (!strcasecmp(v->value, "proxy"))
			ast_set_flag(flags, SIP_OSPAUTH_PROXY);
		else if (!strcasecmp(v->value, "gateway"))
			ast_set_flag(flags, SIP_OSPAUTH_GATEWAY);
		else if(!strcasecmp (v->value, "exclusive"))
 			ast_set_flag(flags, SIP_OSPAUTH_EXCLUSIVE);
#endif
	} else if (!strcasecmp(v->name, "promiscredir")) {
		ast_set_flag(mask, SIP_PROMISCREDIR);
		ast_set2_flag(flags, ast_true(v->value), SIP_PROMISCREDIR);
		res = 1;
	}

	return res;
}

/*! \brief  add_sip_domain: Add SIP domain to list of domains we are responsible for */
static int add_sip_domain(const char *domain, const enum domain_mode mode, const char *context)
{
	struct domain *d;

	if (ast_strlen_zero(domain)) {
		ast_log(LOG_WARNING, "Zero length domain.\n");
		return 1;
	}

	d = calloc(1, sizeof(*d));
	if (!d) {
		ast_log(LOG_ERROR, "Allocation of domain structure failed, Out of memory\n");
		return 0;
	}

	ast_copy_string(d->domain, domain, sizeof(d->domain));

	if (!ast_strlen_zero(context))
		ast_copy_string(d->context, context, sizeof(d->context));

	d->mode = mode;

	AST_LIST_LOCK(&domain_list);
	AST_LIST_INSERT_TAIL(&domain_list, d, list);
	AST_LIST_UNLOCK(&domain_list);

 	if (sipdebug)	
		ast_log(LOG_DEBUG, "Added local SIP domain '%s'\n", domain);

	return 1;
}

/*! \brief  check_sip_domain: Check if domain part of uri is local to our server */
static int check_sip_domain(const char *domain, char *context, size_t len)
{
	struct domain *d;
	int result = 0;

	AST_LIST_LOCK(&domain_list);
	AST_LIST_TRAVERSE(&domain_list, d, list) {
		if (strcasecmp(d->domain, domain))
			continue;

		if (len && !ast_strlen_zero(d->context))
			ast_copy_string(context, d->context, len);
		
		result = 1;
		break;
	}
	AST_LIST_UNLOCK(&domain_list);

	return result;
}

/*! \brief  clear_sip_domains: Clear our domain list (at reload) */
static void clear_sip_domains(void)
{
	struct domain *d;

	AST_LIST_LOCK(&domain_list);
	while ((d = AST_LIST_REMOVE_HEAD(&domain_list, list)))
		free(d);
	AST_LIST_UNLOCK(&domain_list);
}


/*! \brief  add_realm_authentication: Add realm authentication in list ---*/
static struct sip_auth *add_realm_authentication(struct sip_auth *authlist, char *configuration, int lineno)
{
	char authcopy[256];
	char *username=NULL, *realm=NULL, *secret=NULL, *md5secret=NULL;
	char *stringp;
	struct sip_auth *auth;
	struct sip_auth *b = NULL, *a = authlist;

	if (ast_strlen_zero(configuration))
		return authlist;

	ast_log(LOG_DEBUG, "Auth config ::  %s\n", configuration);

	ast_copy_string(authcopy, configuration, sizeof(authcopy));
	stringp = authcopy;

	username = stringp;
	realm = strrchr(stringp, '@');
	if (realm) {
		*realm = '\0';
		realm++;
	}
	if (ast_strlen_zero(username) || ast_strlen_zero(realm)) {
		ast_log(LOG_WARNING, "Format for authentication entry is user[:secret]@realm at line %d\n", lineno);
		return authlist;
	}
	stringp = username;
	username = strsep(&stringp, ":");
	if (username) {
		secret = strsep(&stringp, ":");
		if (!secret) {
			stringp = username;
			md5secret = strsep(&stringp,"#");
		}
	}
	auth = malloc(sizeof(struct sip_auth));
	if (auth) {
		memset(auth, 0, sizeof(struct sip_auth));
		ast_copy_string(auth->realm, realm, sizeof(auth->realm));
		ast_copy_string(auth->username, username, sizeof(auth->username));
		if (secret)
			ast_copy_string(auth->secret, secret, sizeof(auth->secret));
		if (md5secret)
			ast_copy_string(auth->md5secret, md5secret, sizeof(auth->md5secret));
	} else {
		ast_log(LOG_ERROR, "Allocation of auth structure failed, Out of memory\n");
		return authlist;
	}

	/* Add authentication to authl */
	if (!authlist) {	/* No existing list */
		return auth;
	} 
	while(a) {
		b = a;
		a = a->next;
	}
	b->next = auth;	/* Add structure add end of list */

	if (option_verbose > 2)
		ast_verbose("Added authentication for realm %s\n", realm);

	return authlist;

}

/*! \brief  clear_realm_authentication: Clear realm authentication list (at reload) ---*/
static int clear_realm_authentication(struct sip_auth *authlist)
{
	struct sip_auth *a = authlist;
	struct sip_auth *b;

	while (a) {
		b = a;
		a = a->next;
		free(b);
	}

	return 1;
}

/*! \brief  find_realm_authentication: Find authentication for a specific realm ---*/
static struct sip_auth *find_realm_authentication(struct sip_auth *authlist, char *realm)
{
	struct sip_auth *a = authlist; 	/* First entry in auth list */

	while (a) {
		if (!strcasecmp(a->realm, realm)){
			break;
		}
		a = a->next;
	}
	
	return a;
}

/*! \brief  build_user: Initiate a SIP user structure from sip.conf ---*/
static struct sip_user *build_user(const char *name, struct ast_variable *v, int realtime)
{
	struct sip_user *user;
	int format;
	struct ast_ha *oldha = NULL;
	char *varname = NULL, *varval = NULL;
	struct ast_variable *tmpvar = NULL;
	struct ast_flags userflags = {(0)};
	struct ast_flags mask = {(0)};


	user = (struct sip_user *)malloc(sizeof(struct sip_user));
	if (!user) {
		return NULL;
	}
	memset(user, 0, sizeof(struct sip_user));
	suserobjs++;
	ASTOBJ_INIT(user);
	ast_copy_string(user->name, name, sizeof(user->name));
	oldha = user->ha;
	user->ha = NULL;
	ast_copy_flags(user, &global_flags, SIP_FLAGS_TO_COPY);
	user->capability = global_capability;
	user->prefs = global_prefs;
	user->stimer.st_mode_oper = global_st_mode;	/* Session-Timers */
	user->stimer.st_ref = global_st_refresher;
	user->stimer.st_min_se = global_min_se;
	user->stimer.st_max_se = global_max_se;
	user->callwaiting = CALLWAITING_NONE;
	ast_clear_flag((&user->flags_page2), SIP_SESSION_TIMERS_FLAGS_TO_COPY);

	/* set default context */
	strcpy(user->context, default_context);
	strcpy(user->language, default_language);
	strcpy(user->musicclass, global_musicclass);
	while(v) {
		if (handle_common_options(&userflags, &mask, v)) {
			v = v->next;
			continue;
		}

		if (!strcasecmp(v->name, "context")) {
			ast_copy_string(user->context, v->value, sizeof(user->context));
		} else if (!strcasecmp(v->name, "subscribecontext")) {
			ast_copy_string(user->subscribecontext, v->value, sizeof(user->subscribecontext));
		} else if (!strcasecmp(v->name, "setvar")) {
			varname = ast_strdupa(v->value);
			if (varname && (varval = strchr(varname,'='))) {
				*varval = '\0';
				varval++;
				if ((tmpvar = ast_variable_new(varname, varval))) {
					tmpvar->next = user->chanvars;
					user->chanvars = tmpvar;
				}
			}
		} else if (!strcasecmp(v->name, "permit") ||
				   !strcasecmp(v->name, "deny")) {
			user->ha = ast_append_ha(v->name, v->value, user->ha);
		} else if (!strcasecmp(v->name, "secret")) {
			ast_copy_string(user->secret, v->value, sizeof(user->secret)); 
		} else if (!strcasecmp(v->name, "md5secret")) {
			ast_copy_string(user->md5secret, v->value, sizeof(user->md5secret));
		} else if (!strcasecmp(v->name, "callerid")) {
			ast_callerid_split(v->value, user->cid_name, sizeof(user->cid_name), user->cid_num, sizeof(user->cid_num));
		} else if (!strcasecmp(v->name, "callgroup")) {
			user->callgroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "pickupgroup")) {
			user->pickupgroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "language")) {
			ast_copy_string(user->language, v->value, sizeof(user->language));
		} else if (!strcasecmp(v->name, "musicclass") || !strcasecmp(v->name, "musiconhold")) {
			ast_copy_string(user->musicclass, v->value, sizeof(user->musicclass));
		} else if (!strcasecmp(v->name, "accountcode")) {
			ast_copy_string(user->accountcode, v->value, sizeof(user->accountcode));
		} else if (!strcasecmp(v->name, "call-limit") || !strcasecmp(v->name, "incominglimit")) {
			user->call_limit = atoi(v->value);
			if (user->call_limit < 0)
				user->call_limit = 0;
		} else if (!strcasecmp(v->name, "callwaiting")) {
		    	if (!strcasecmp(v->value, "none"))
			    	user->callwaiting = CALLWAITING_NONE;
			else if (!strcasecmp(v->value, "on"))
				user->callwaiting = CALLWAITING_ON;
			else if (!strcasecmp(v->value, "off"))
				user->callwaiting = CALLWAITING_OFF;
		} else if (!strcasecmp(v->name, "amaflags")) {
			format = ast_cdr_amaflags2int(v->value);
			if (format < 0) {
				ast_log(LOG_WARNING, "Invalid AMA Flags: %s at line %d\n", v->value, v->lineno);
			} else {
				user->amaflags = format;
			}
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&user->prefs, &user->capability, v->value,
				1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&user->prefs, &user->capability, v->value, 0);
		} else if (!strcasecmp(v->name, "callingpres")) {
			user->callingpres = ast_parse_caller_presentation(v->value);
			if (user->callingpres == -1)
				user->callingpres = atoi(v->value);
		} else if (!strcasecmp(v->name, "session-timers")) {
			int i = (int) str2stmode(v->value); 
			if (i < 0) {
				ast_log(LOG_WARNING, "Invalid session-timers '%s' at line %d of %s\n", v->value, v->lineno, config);
				user->stimer.st_mode_oper = global_st_mode;
			} else {
				user->stimer.st_mode_oper = i;
			}
		} else if (!strcasecmp(v->name, "session-expires")) {
			if (sscanf(v->value, "%d", &user->stimer.st_max_se) != 1) {
				ast_log(LOG_WARNING, "Invalid session-expires '%s' at line %d of %s\n", v->value, v->lineno, config);
				user->stimer.st_max_se = global_max_se;
			} 
		} else if (!strcasecmp(v->name, "session-minse")) {
			if (sscanf(v->value, "%d", &user->stimer.st_min_se) != 1) {
				ast_log(LOG_WARNING, "Invalid session-minse '%s' at line %d of %s\n", v->value, v->lineno, config);
				user->stimer.st_min_se = global_min_se;
			} 
			if (user->stimer.st_min_se < 90) {
				ast_log(LOG_WARNING, "session-minse '%s' at line %d of %s is not allowed to be < 90 secs\n", v->value, v->lineno, config);
				user->stimer.st_min_se = global_min_se;
			} 
		} else if (!strcasecmp(v->name, "session-refresher")) {
			int i = (int) str2strefresher(v->value); 
			if (i < 0) {
				ast_log(LOG_WARNING, "Invalid session-refresher '%s' at line %d of %s\n", v->value, v->lineno, config);
				user->stimer.st_ref = global_st_refresher;
			} else {
				user->stimer.st_ref = i;
			}
		} else if (!strcasecmp(v->name, "session-refresh-update")) {
			ast_set2_flag(&(user->flags_page2), ast_true(v->value), SIP_PAGE2_SESSION_REFRESH_UPDATE);
		} else if (!strcasecmp(v->name, "session-timers-force")) {
			ast_set2_flag(&(user->flags_page2), ast_true(v->value),
				SIP_PAGE2_SESSION_TIMERS_FORCE);
		} else if (!strcasecmp(v->name, "username")) {
			ast_copy_string(user->username, v->value, sizeof(user->username));
		}
		/*else if (strcasecmp(v->name,"type"))
		 *	ast_log(LOG_WARNING, "Ignoring %s\n", v->name);
		 */
		v = v->next;
	}
	ast_copy_flags(user, &userflags, mask.flags);
	ast_free_ha(oldha);
	return user;
}

/*! \brief  temp_peer: Create temporary peer (used in autocreatepeer mode) ---*/
static struct sip_peer *temp_peer(const char *name)
{
	struct sip_peer *peer;

	peer = malloc(sizeof(*peer));
	if (!peer)
		return NULL;

	memset(peer, 0, sizeof(*peer));
	apeerobjs++;
	ASTOBJ_INIT(peer);

	peer->expire = -1;
	peer->pokeexpire = -1;
	ast_copy_string(peer->name, name, sizeof(peer->name));
	ast_copy_flags(peer, &global_flags, SIP_FLAGS_TO_COPY);
	strcpy(peer->context, default_context);
	strcpy(peer->subscribecontext, default_subscribecontext);
	strcpy(peer->language, default_language);
	strcpy(peer->musicclass, global_musicclass);
	peer->addr.sin_port = htons(DEFAULT_SIP_PORT);
	peer->addr.sin_family = AF_INET;
	peer->capability = global_capability;
	peer->rtptimeout = global_rtptimeout;
	peer->rtpholdtimeout = global_rtpholdtimeout;
	peer->rtpkeepalive = global_rtpkeepalive;
	ast_set_flag(peer, SIP_SELFDESTRUCT);
	ast_set_flag(peer, SIP_DYNAMIC);
	peer->prefs = global_prefs;
	peer->stimer.st_mode_oper = global_st_mode;	/* Session-Timers */
	peer->stimer.st_ref = global_st_refresher;
	peer->stimer.st_min_se = global_min_se;
	peer->stimer.st_max_se = global_max_se;

	reg_source_db(peer);

	return peer;
}

static void sip_template_unmark(struct sip_peer *peer, struct sip_peer *template)
{
	struct sip_user *user;
	struct ast_config *cfg;

	if (!ast_test_flag(&(peer->flags_page2), SIP_PAGE2_FROM_TEMPLATE) || strlen(peer->name) != strlen(template->name))
	    return;

	if (!sip_peer_name_template(template->name, peer->name))
	    return;
	
	ASTOBJ_UNMARK(peer);

	cfg = ast_config_load(config);
	user = build_user(peer->name, ast_variable_browse(cfg, template->name), 0);
	if (user) {
	    ASTOBJ_CONTAINER_LINK(&userl,user);
	    ASTOBJ_UNREF(user, sip_destroy_user);
	}
}

static struct sip_registry *get_registry_for_sip(struct sip_pvt *sip)
{
    char buff[256];
    struct sip_registry *reg = NULL;

    if (sip->registry)
	return sip->registry;

    if (!sip->tohost || !sip->peername ||
	snprintf(buff, sizeof(buff), "%s@%s", sip->peername, sip->tohost) < 0)
    {
	return NULL;
    }
    
    ASTOBJ_CONTAINER_TRAVERSE(&regl, !reg, do {
	ASTOBJ_RDLOCK(iterator);

	if (!strcmp(buff, iterator->username))
	    reg = iterator;

	ASTOBJ_UNLOCK(iterator);
    } while(0));

    return reg;
}

static struct sip_registry *find_registry(const char * username, 
	const char *obproxy)
{
	struct sip_registry *registry = NULL;
	char *reg_username = NULL;

	ASTOBJ_CONTAINER_TRAVERSE(&regl, !registry, {
		ASTOBJ_RDLOCK(iterator);
		reg_username = ast_strdupa(iterator->username);
		if ((!strcmp(username, strtok(reg_username, "@"))) && (!strcmp(obproxy,
		iterator->obproxy)))
		{
		registry = ASTOBJ_REF(iterator);
		}
		ASTOBJ_UNLOCK(iterator);
		});

	return registry;
}

static struct sip_registry *find_registry_by_addr(struct sockaddr_in *sin)
{
	struct sip_registry *registry = NULL;

	ASTOBJ_CONTAINER_TRAVERSE(&regl, !registry, do {
		ASTOBJ_RDLOCK(iterator);
		if (!inaddrcmp(&iterator->uas_srv.addr, sin))
			registry = ASTOBJ_REF(iterator);
		ASTOBJ_UNLOCK(iterator);
	} while (0));

	return registry;
}

/*! \brief  build_peer: Build peer from config file ---*/
static struct sip_peer *build_peer(const char *name, struct ast_variable *v, int realtime, int reload)
{
	struct sip_peer *peer = NULL;
	struct ast_ha *oldha = NULL;
	int obproxyfound=0;
	int found=0;
	int format=0;		/* Ama flags */
	time_t regseconds;
	char *varname = NULL, *varval = NULL;
	struct ast_variable *tmpvar = NULL;
	struct ast_flags peerflags = {(0)};
	struct ast_flags mask = {(0)};


	if (!realtime)
		/* Note we do NOT use find_peer here, to avoid realtime recursion */
		/* We also use a case-sensitive comparison (unlike find_peer) so
		   that case changes made to the peer name will be properly handled
		   during reload
		*/
		peer = ASTOBJ_CONTAINER_FIND_UNLINK_FULL(&peerl, name, name, 0, 0, strcmp);

	if (peer) {
		/* Already in the list, remove it and it will be added back (or FREE'd)  */
		found++;
 	} else {
		peer = malloc(sizeof(*peer));
		if (peer) {
			memset(peer, 0, sizeof(*peer));
			if (realtime)
				rpeerobjs++;
			else
				speerobjs++;
			ASTOBJ_INIT(peer);
			peer->expire = -1;
			peer->pokeexpire = -1;
		} else {
			ast_log(LOG_WARNING, "Can't allocate SIP peer memory\n");
		}
	}
	/* Note that our peer HAS had its reference count incrased */
	if (!peer)
		return NULL;

	peer->lastmsgssent = -1;
	if (!found) {
		if (name)
			ast_copy_string(peer->name, name, sizeof(peer->name));
		peer->addr.sin_port = htons(DEFAULT_SIP_PORT);
		peer->addr.sin_family = AF_INET;
		peer->defaddr.sin_family = AF_INET;
	}
	/* If we have channel variables, remove them (reload) */
	if (peer->chanvars) {
		ast_variables_destroy(peer->chanvars);
		peer->chanvars = NULL;
	}
	strcpy(peer->context, default_context);
	strcpy(peer->subscribecontext, default_subscribecontext);
	strcpy(peer->vmexten, global_vmexten);
	strcpy(peer->language, default_language);
	strcpy(peer->musicclass, global_musicclass);
	ast_copy_flags(peer, &global_flags, SIP_USEREQPHONE);
	peer->secret[0] = '\0';
	peer->md5secret[0] = '\0';
	peer->cid_num[0] = '\0';
	peer->cid_name[0] = '\0';
	peer->fromdomain[0] = '\0';
	peer->todomain[0] = '\0';
	peer->fromuser[0] = '\0';
	peer->fromname[0] = '\0';
	peer->fromuri[0] = '\0';	
	peer->organization[0] = '\0';	
	peer->prack_level = PRACK_LEVEL_NONE;
	peer->regexten[0] = '\0';
	peer->mailbox[0] = '\0';
	peer->remotemailbox[0] = '\0';
	peer->callgroup = 0;
	peer->clir = 0;
	peer->displayinfo = 0;
	/* there are servers which confirm our codec as our input, but wish
	 * another one for their input. when we see this, we change both
	 * directions to use the server's codec, and expect to see only that one
	 * on our input. this causes us to reject the server's packets (with our
	 * originally declared codec).
	 * to avoid this problem, declare both PCM codecs in advance, so the
	 * server will use its preferred codec for both sides, like we do */
	peer->faxtxcodecs = AST_FORMAT_ULAW | AST_FORMAT_ALAW;
	peer->modemtxcodecs = AST_FORMAT_ULAW | AST_FORMAT_ALAW;
	peer->faxmethod = FAX_PASSTHROUGH_AUTO;
	peer->pickupgroup = 0;
	peer->rtpkeepalive = global_rtpkeepalive;
	peer->maxms = default_qualify;
	peer->prefs = global_prefs;
	peer->stimer.st_mode_oper = global_st_mode;	/* Session-Timers */
	peer->stimer.st_ref = global_st_refresher;
	peer->stimer.st_min_se = global_min_se;
	peer->stimer.st_max_se = global_max_se;
	if(!found)
	    memset(&peer->rtp_stats, 0, sizeof(rtp_stats_t));
	peer->callwaiting = CALLWAITING_NONE;

	oldha = peer->ha;
	peer->ha = NULL;
	peer->addr.sin_family = AF_INET;
	ast_copy_flags(peer, &global_flags, SIP_FLAGS_TO_COPY);
	peer->capability = global_capability;
	peer->rtptimeout = global_rtptimeout;
	peer->rtpholdtimeout = global_rtpholdtimeout;
	ast_clear_flag((&peer->flags_page2), SIP_PAGE2_G729_ANNEXB);
	ast_clear_flag((&peer->flags_page2), SIP_SESSION_TIMERS_FLAGS_TO_COPY);
	peer->bw_mgt = NULL;
	while(v) {
		if (handle_common_options(&peerflags, &mask, v)) {
			v = v->next;
			continue;
		}

		if (realtime && !strcasecmp(v->name, "regseconds")) {
			if (sscanf(v->value, "%ld", (time_t *)&regseconds) != 1)
				regseconds = 0;
		} else if (realtime && !strcasecmp(v->name, "ipaddr") && !ast_strlen_zero(v->value) ) {
			inet_aton(v->value, &(peer->addr.sin_addr));
		} else if (realtime && !strcasecmp(v->name, "name"))
			ast_copy_string(peer->name, v->value, sizeof(peer->name));
		else if (realtime && !strcasecmp(v->name, "fullcontact")) {
			ast_copy_string(peer->fullcontact, v->value, sizeof(peer->fullcontact));
			ast_set_flag((&peer->flags_page2), SIP_PAGE2_RT_FROMCONTACT);
		} else if (!strcasecmp(v->name, "secret")) 
			ast_copy_string(peer->secret, v->value, sizeof(peer->secret));
		else if (!strcasecmp(v->name, "md5secret")) 
			ast_copy_string(peer->md5secret, v->value, sizeof(peer->md5secret));
		else if (!strcasecmp(v->name, "auth"))
			peer->auth = add_realm_authentication(peer->auth, v->value, v->lineno);
		else if (!strcasecmp(v->name, "clir"))
			peer->clir = atoi(v->value);
		else if (!strcasecmp(v->name, "callerid")) {
			ast_callerid_split(v->value, peer->cid_name, sizeof(peer->cid_name), peer->cid_num, sizeof(peer->cid_num));
		} else if (!strcasecmp(v->name, "context")) {
			ast_copy_string(peer->context, v->value, sizeof(peer->context));
		} else if (!strcasecmp(v->name, "subscribecontext")) {
			ast_copy_string(peer->subscribecontext, v->value, sizeof(peer->subscribecontext));
		} else if (!strcasecmp(v->name, "fromdomain"))
			ast_copy_string(peer->fromdomain, v->value, sizeof(peer->fromdomain));
		else if (!strcasecmp(v->name, "todomain"))
			ast_copy_string(peer->todomain, v->value, sizeof(peer->todomain));
		else if (!strcasecmp(v->name, "usereqphone"))
			ast_set2_flag(peer, ast_true(v->value), SIP_USEREQPHONE);
		else if (!strcasecmp(v->name, "fromuser"))
			ast_copy_string(peer->fromuser, v->value, sizeof(peer->fromuser));
		else if (!strcasecmp(v->name, "displayinfo"))
		        peer->displayinfo = ast_true(v->value);
		else if (!strcasecmp(v->name, "fromname"))
			ast_copy_string(peer->fromname, v->value, sizeof(peer->fromname));
		else if (!strcasecmp(v->name, "fromuri"))
			ast_copy_string(peer->fromuri, v->value, sizeof(peer->fromuri));
		else if (!strcasecmp(v->name, "organization"))
			ast_copy_string(peer->organization, v->value, sizeof(peer->organization));
		else if (!strcasecmp(v->name, "host") || !strcasecmp(v->name, "outboundproxy")) {
			if (!strcasecmp(v->value, "dynamic")) {
				if (!strcasecmp(v->name, "outboundproxy") || obproxyfound) {
					ast_log(LOG_WARNING, "You can't have a dynamic outbound proxy, you big silly head at line %d.\n", v->lineno);
				} else {
					/* They'll register with us */
					ast_set_flag(peer, SIP_DYNAMIC);
					if (!found) {
						/* Initialize stuff iff we're not found, otherwise
						   we keep going with what we had */
						memset(&peer->addr.sin_addr, 0, 4);
						if (peer->addr.sin_port) {
							/* If we've already got a port, make it the default rather than absolute */
							peer->defaddr.sin_port = peer->addr.sin_port;
							peer->addr.sin_port = 0;
						}
					}
				}
			} else {
				/* Non-dynamic.  Make sure we become that way if we're not */
				if (peer->expire > -1)
					ast_sched_del(sched, peer->expire);
				peer->expire = -1;
				ast_clear_flag(peer, SIP_DYNAMIC);	
				
				if (!strcasecmp(v->name, "outboundproxy")) {
					ast_copy_string(peer->obproxy, v->value, sizeof(peer->obproxy));
					obproxyfound=1;
				}
				else {
					ast_copy_string(peer->tohost, v->value, sizeof(peer->tohost));
					if (!peer->addr.sin_port)
						peer->addr.sin_port = htons(DEFAULT_SIP_PORT);
				}
			}
		} else if (!strcasecmp(v->name, "outboundproxyport")) {
			peer->addr.sin_port = htons(atoi(v->value));
		} else if (!strcasecmp(v->name, "defaultip")) {
			if (ast_get_ip(&peer->defaddr, v->value)) {
				ASTOBJ_UNREF(peer, sip_destroy_peer);
				return NULL;
			}
		} else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
			peer->ha = ast_append_ha(v->name, v->value, peer->ha);
		} else if (!strcasecmp(v->name, "port")) {
		    peer->toport = atoi(v->value);
		    if (!obproxyfound)
		    {
				if (!realtime && ast_test_flag(peer, SIP_DYNAMIC))
					peer->defaddr.sin_port = htons(atoi(v->value));
				else
					peer->addr.sin_port = htons(atoi(v->value));
			}
		} else if (!strcasecmp(v->name, "callingpres")) {
			peer->callingpres = ast_parse_caller_presentation(v->value);
			if (peer->callingpres == -1)
				peer->callingpres = atoi(v->value);
		} else if (!strcasecmp(v->name, "username")) {
			ast_copy_string(peer->username, v->value, sizeof(peer->username));
		} else if (!strcasecmp(v->name, "language")) {
			ast_copy_string(peer->language, v->value, sizeof(peer->language));
		} else if (!strcasecmp(v->name, "regexten")) {
			ast_copy_string(peer->regexten, v->value, sizeof(peer->regexten));
		} else if (!strcasecmp(v->name, "call-limit") || !strcasecmp(v->name, "incominglimit")) {
			peer->call_limit = atoi(v->value);
			if (peer->call_limit < 0)
				peer->call_limit = 0;
		} else if (!strcasecmp(v->name, "callwaiting")) {
		    	if (!strcasecmp(v->value, "none"))
			    	peer->callwaiting = CALLWAITING_NONE;
			else if (!strcasecmp(v->value, "on"))
				peer->callwaiting = CALLWAITING_ON;
			else if (!strcasecmp(v->value, "off"))
				peer->callwaiting = CALLWAITING_OFF;
		} else if (!strcasecmp(v->name, "amaflags")) {
			format = ast_cdr_amaflags2int(v->value);
			if (format < 0) {
				ast_log(LOG_WARNING, "Invalid AMA Flags for peer: %s at line %d\n", v->value, v->lineno);
			} else {
				peer->amaflags = format;
			}
		} else if (!strcasecmp(v->name, "accountcode")) {
			ast_copy_string(peer->accountcode, v->value, sizeof(peer->accountcode));
		} else if (!strcasecmp(v->name, "musicclass") || !strcasecmp(v->name, "musiconhold")) {
			ast_copy_string(peer->musicclass, v->value, sizeof(peer->musicclass));
		} else if (!strcasecmp(v->name, "mailbox")) {
			ast_copy_string(peer->mailbox, v->value, sizeof(peer->mailbox));
		} else if (!strcasecmp(v->name, "remotemailbox")) {
			ast_copy_string(peer->remotemailbox, v->value, sizeof(peer->remotemailbox));
		} else if (!strcasecmp(v->name, "vmexten")) {
			ast_copy_string(peer->vmexten, v->value, sizeof(peer->vmexten));
		} else if (!strcasecmp(v->name, "callgroup")) {
			peer->callgroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "pickupgroup")) {
			peer->pickupgroup = ast_get_group(v->value);
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&peer->prefs, &peer->capability, v->value,
				1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&peer->prefs, &peer->capability, v->value, 0);
		} else if (!strcasecmp(v->name, "silencesuppression")) {
			if (ast_getformatbyname(v->value) == AST_FORMAT_G729A)
				ast_set_flag(&(peer->flags_page2), SIP_PAGE2_G729_ANNEXB);
		} else if (!strcasecmp(v->name, "rtptimeout")) {
			if ((sscanf(v->value, "%d", &peer->rtptimeout) != 1) || (peer->rtptimeout < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid RTP hold time at line %d.  Using default.\n", v->value, v->lineno);
				peer->rtptimeout = global_rtptimeout;
			}
		} else if (!strcasecmp(v->name, "rtpholdtimeout")) {
			if ((sscanf(v->value, "%d", &peer->rtpholdtimeout) != 1) || (peer->rtpholdtimeout < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid RTP hold time at line %d.  Using default.\n", v->value, v->lineno);
				peer->rtpholdtimeout = global_rtpholdtimeout;
			}
		} else if (!strcasecmp(v->name, "rtpkeepalive")) {
			if ((sscanf(v->value, "%d", &peer->rtpkeepalive) != 1) || (peer->rtpkeepalive < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid RTP keepalive time at line %d.  Using default.\n", v->value, v->lineno);
				peer->rtpkeepalive = global_rtpkeepalive;
			}
		} else if (!strcasecmp(v->name, "setvar")) {
			/* Set peer channel variable */
			varname = ast_strdupa(v->value);
			if (varname && (varval = strchr(varname,'='))) {
				*varval = '\0';
				varval++;
				if ((tmpvar = ast_variable_new(varname, varval))) {
					tmpvar->next = peer->chanvars;
					peer->chanvars = tmpvar;
				}
			}
		} else if (!strcasecmp(v->name, "qualify")) {
			if (!strcasecmp(v->value, "no")) {
				peer->maxms = 0;
			} else if (!strcasecmp(v->value, "yes")) {
				peer->maxms = DEFAULT_MAXMS;
			} else if (sscanf(v->value, "%d", &peer->maxms) != 1) {
				ast_log(LOG_WARNING, "Qualification of peer '%s' should be 'yes', 'no', or a number of milliseconds at line %d of sip.conf\n", peer->name, v->lineno);
				peer->maxms = 0;
			}
		} else if (!strcasecmp(v->name, "faxtxcodecs")) {
			peer->faxtxcodecs = ast_parse_codec_list(v->value);
		} else if (!strcasecmp(v->name, "modemtxcodecs")) {
			peer->modemtxcodecs = ast_parse_codec_list(v->value);
		} else if (!strcasecmp(v->name, "faxmethod")) {
			peer->faxmethod = jdsp_fax_method_parse(v->value);
		} else if (!strcasecmp(v->name, "100rel")) {
		    if (!strcasecmp(v->value, "none"))
			peer->prack_level = PRACK_LEVEL_NONE;
		    else if (!strcasecmp(v->value, "supported"))
			peer->prack_level = PRACK_LEVEL_SUPPORTED;
		    else if (!strcasecmp(v->value, "require"))
			peer->prack_level = PRACK_LEVEL_REQUIRE;
		    else ast_log(LOG_WARNING, "Invalid value for 100rel entry "
			"in line %d of sip.conf - must be 'none', 'supported' "
			"or 'require'. 'none' assumed.\n", v->lineno);			
		} else if (!strcasecmp(v->name, "rtcpinterval")) {
			peer->rtcp_interval = atoi(v->value);
		} else if (!strcasecmp(v->name, "session-timers")) {
			int i = (int) str2stmode(v->value); 
			if (i < 0) {
				ast_log(LOG_WARNING, "Invalid session-timers '%s' at line %d of %s\n", v->value, v->lineno, config);
				peer->stimer.st_mode_oper = global_st_mode;
			} else {
				peer->stimer.st_mode_oper = i;
			}
		} else if (!strcasecmp(v->name, "session-expires")) {
			if (sscanf(v->value, "%d", &peer->stimer.st_max_se) != 1) {
				ast_log(LOG_WARNING, "Invalid session-expires '%s' at line %d of %s\n", v->value, v->lineno, config);
				peer->stimer.st_max_se = global_max_se;
			} 
		} else if (!strcasecmp(v->name, "session-minse")) {
			if (sscanf(v->value, "%d", &peer->stimer.st_min_se) != 1) {
				ast_log(LOG_WARNING, "Invalid session-minse '%s' at line %d of %s\n", v->value, v->lineno, config);
				peer->stimer.st_min_se = global_min_se;
			} 
			if (peer->stimer.st_min_se < 90) {
				ast_log(LOG_WARNING, "session-minse '%s' at line %d of %s is not allowed to be < 90 secs\n", v->value, v->lineno, config);
				peer->stimer.st_min_se = global_min_se;
			} 
		} else if (!strcasecmp(v->name, "session-refresher")) {
			int i = (int) str2strefresher(v->value); 
			if (i < 0) {
				ast_log(LOG_WARNING, "Invalid session-refresher '%s' at line %d of %s\n", v->value, v->lineno, config);
				peer->stimer.st_ref = global_st_refresher;
			} else {
				peer->stimer.st_ref = i;
			}
		} else if (!strcasecmp(v->name, "session-refresh-update")) {
			ast_set2_flag(&(peer->flags_page2), ast_true(v->value), SIP_PAGE2_SESSION_REFRESH_UPDATE);
		} else if (!strcasecmp(v->name, "session-timers-force")) {
			ast_set2_flag(&(peer->flags_page2), ast_true(v->value),
				SIP_PAGE2_SESSION_TIMERS_FORCE);
		} else if (!strcasecmp(v->name, "template")) {
		     if (atoi(v->value) == 2)
			 ast_set_flag(&(peer->flags_page2), SIP_PAGE2_FROM_TEMPLATE);
		     else
			 ast_set2_flag(&(peer->flags_page2), ast_true(v->value), SIP_PAGE2_TEMPLATE);
		     /* We want to save all template peers only at reload */
		     if (reload) {
			 ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
			     ASTOBJ_RDLOCK(iterator);
			     sip_template_unmark(iterator, peer);
			     ASTOBJ_UNLOCK(iterator);
			 } while (0)
			 );
		     }
		} else if (!strcasecmp(v->name, "bw_mgt_enabled")) {
			if (ast_true(v->value))
				peer->bw_mgt = &global_bw_mgt;
		}
		/* else if (strcasecmp(v->name,"type"))
		 *	ast_log(LOG_WARNING, "Ignoring %s\n", v->name);
		 */
		v=v->next;
	}

	/* In case we found both an outbound proxy and a username, 
	 * search for a matching registry.
	 */
	if (obproxyfound && srv_failover_enabled && peer->username &&
		!inet_aton(peer->obproxy, &peer->addr.sin_addr)) {
		if (!(peer->registry = find_registry(peer->username, peer->obproxy))) {
			ast_log(LOG_WARNING, "This is weird... Peer "
				"with a username, an outbound proxy "
				" and no matching registery.");
		}
		else if (!peer->registry->uas_srv.healthy) {
			/* If the UAS address of the registry is not healthy, it's an
			 * indication that the srv record could not be resolved, so we can't
			 * use this registry nor this peer. */
			ast_log(LOG_DEBUG, "Registry UAC SRV is not healthy yet, continue anyway\n");
		}
	}

	/* In case we don't have an obproxy, or we do have an obproxy but 
	 * don't have a registry, we resolve the peer address on our own.
	 */
	if (!obproxyfound || !peer->registry) {
	    if (!resolve_peer_addr(peer))
	    {
		ast_log(LOG_WARNING, "Failed to resolve peer \'%s\' addr, "
		    "creating peer with unspecified address.\n", peer->name);
	    }
	}

	if (!ast_test_flag((&global_flags_page2), SIP_PAGE2_IGNOREREGEXPIRE) && ast_test_flag(peer, SIP_DYNAMIC) && realtime) {
		time_t nowtime;

		time(&nowtime);
		if ((nowtime - regseconds) > 0) {
			destroy_association(peer);
			memset(&peer->addr, 0, sizeof(peer->addr));
			if (option_debug)
				ast_log(LOG_DEBUG, "Bah, we're expired (%d/%d/%d)!\n", (int)(nowtime - regseconds), (int)regseconds, (int)nowtime);
		}
	}
	ast_copy_flags(peer, &peerflags, mask.flags);
	if (!found && ast_test_flag(peer, SIP_DYNAMIC) && !ast_test_flag(peer, SIP_REALTIME))
		reg_source_db(peer);
	ASTOBJ_UNMARK(peer);
	ast_free_ha(oldha);
	return peer;
}

static void sip_hangup_disabled_users(void)
{
	struct sip_pvt *p;
	
	ast_mutex_lock(&iflock);
	p = iflist;
	while (p)
	{
		struct sip_user *user = NULL;
		struct sip_peer *peer = NULL;

		if (!(user = find_user(p->username, 1)) &&
            !(peer = find_peer(p->peername, NULL, 1)))  
		{
			if (p->owner && p->owner->_state == AST_STATE_UP)
			{
				ast_mutex_lock(&p->lock);
				ast_softhangup(p->owner, AST_SOFTHANGUP_DEV);
				ast_mutex_unlock(&p->lock);
			}
		}
	
		if (user)
			ASTOBJ_UNREF(user, sip_destroy_user);
		if (peer)
			ASTOBJ_UNREF(peer, sip_destroy_peer);

		p = p->next;
	}
	ast_mutex_unlock(&iflock);
}

/*! \brief  reload_config: Re-read SIP.conf config file ---*/
/*	This function reloads all config data, except for
	active peers (with registrations). They will only
	change configuration data at restart, not at reload.
	SIP debug and recordhistory state will not change
 */
static int reload_config(void)
{
	struct ast_config *cfg;
	struct ast_variable *v;
	struct sip_peer *peer;
	struct sip_user *user;
	struct ast_hostent ahp;
	char *cat;
	char *utype;
	struct hostent *hp;
	int format;
	char iabuf[INET_ADDRSTRLEN];
	struct ast_flags dummy;
	int auto_sip_domains = 0;
	int temp_int;

	ast_config_file_md5_update(config, conf_file_md5);
	cfg = ast_config_load(config);

	/* We *must* have a config file otherwise stop immediately */
	if (!cfg) {
		ast_log(LOG_NOTICE, "Unable to load config %s\n", config);
		return -1;
	}
	
	/* Reset IP addresses  */
	memset(&bindaddr, 0, sizeof(bindaddr));
	memset(&localaddr, 0, sizeof(localaddr));
	memset(&register_whitelist, 0, sizeof(register_whitelist));
	memset(&externip, 0, sizeof(externip));
	memset(&global_prefs, 0 , sizeof(global_prefs));
	global_preferred_codec = 1;
	sipdebug &= ~SIP_DEBUG_CONFIG;
	global_st_mode = SESSION_TIMER_MODE_REFUSE;    /* Session-Timers */
	global_st_refresher = SESSION_TIMER_REFRESHER_LOCAL;
	global_min_se  = DEFAULT_MIN_SE;
	global_max_se  = DEFAULT_MAX_SE;

	/* Initialize some reasonable defaults at SIP reload */
	ast_copy_string(default_context, DEFAULT_CONTEXT, sizeof(default_context));
	default_subscribecontext[0] = '\0';
	default_language[0] = '\0';
	default_fromdomain[0] = '\0';
	default_domain[0] = '\0';
	default_qualify = 0;
	allow_external_domains = 1;	/* Allow external invites */
	externhost[0] = '\0';
	externexpire = 0;
	externrefresh = 10;
	ast_copy_string(default_useragent, DEFAULT_USERAGENT, sizeof(default_useragent));
	ast_copy_string(default_notifymime, DEFAULT_NOTIFYMIME, sizeof(default_notifymime));
	global_notifyringing = 1;
	ast_copy_string(global_realm, DEFAULT_REALM, sizeof(global_realm));
	ast_copy_string(global_musicclass, "default", sizeof(global_musicclass));
	ast_copy_string(default_callerid, DEFAULT_CALLERID, sizeof(default_callerid));
#if defined(T38_SUPPORT)
	t38udptlsupport = 0;
	t38rtpsupport = 0;
	t38tcpsupport = 0;
#endif
	memset(&outboundproxyip, 0, sizeof(outboundproxyip));
	outboundproxyip.sin_port = htons(DEFAULT_SIP_PORT);
	outboundproxyip.sin_family = AF_INET;	/* Type of address: IPv4 */
	videosupport = 0;
	compactheaders = 0;
	dumphistory = 0;
	recordhistory = 0;
	relaxdtmf = 0;
	callevents = 0;
	ourport = DEFAULT_SIP_PORT;
	global_rtptimeout = 0;
	global_rtpholdtimeout = 0;
	global_rtpkeepalive = 0;
	pedanticsipchecking = 0;
	global_reg_timeout = DEFAULT_REGISTRATION_TIMEOUT;
	global_regattempts_max = 0;
	ast_clear_flag(&global_flags, AST_FLAGS_ALL);
	ast_set_flag(&global_flags, SIP_DTMF_AUTO);
	ast_set_flag(&global_flags, SIP_NAT_RFC3581);
	ast_set_flag(&global_flags, SIP_CAN_REINVITE);
	ast_set_flag(&global_flags_page2, SIP_PAGE2_RTUPDATE);
	global_mwitime = DEFAULT_MWITIME;
	strcpy(global_vmexten, DEFAULT_VMEXTEN);
	srvlookup = 0;
	srv_failover_enabled = 0;
	min_srv_ttl = 0;
	srv_recover_time = DEFAULT_SRV_RECOVER_TIME;
	reg_recover_time = DEFAULT_REG_RECOVER_TIME;
	autocreatepeer = 0;
	regcontext[0] = '\0';
	tos = 0;
	so_mark = 0;
	expiry = DEFAULT_EXPIRY;
	global_allowguest = 1;
	global_allow_p2p_calls = 1;
	global_highqualitycalls.max = -1;

	/* Read the [general] config section of sip.conf (or from realtime config) */
	v = ast_variable_browse(cfg, "general");
	while(v) {
		if (handle_common_options(&global_flags, &dummy, v)) {
			v = v->next;
			continue;
		}

		/* Create the interface list */
		if (!strcasecmp(v->name, "context")) {
			ast_copy_string(default_context, v->value, sizeof(default_context));
		} else if (!strcasecmp(v->name, "realm")) {
			ast_copy_string(global_realm, v->value, sizeof(global_realm));
		} else if (!strcasecmp(v->name, "useragent")) {
			ast_copy_string(default_useragent, v->value, sizeof(default_useragent));
			ast_log(LOG_DEBUG, "Setting User Agent Name to %s\n",
				default_useragent);
		} else if (!strcasecmp(v->name, "rtcachefriends")) {
			ast_set2_flag((&global_flags_page2), ast_true(v->value), SIP_PAGE2_RTCACHEFRIENDS);	
		} else if (!strcasecmp(v->name, "rtupdate")) {
			ast_set2_flag((&global_flags_page2), ast_true(v->value), SIP_PAGE2_RTUPDATE);	
		} else if (!strcasecmp(v->name, "ignoreregexpire")) {
			ast_set2_flag((&global_flags_page2), ast_true(v->value), SIP_PAGE2_IGNOREREGEXPIRE);	
		} else if (!strcasecmp(v->name, "rtautoclear")) {
			int i = atoi(v->value);
			if (i > 0)
				global_rtautoclear = i;
			else
				i = 0;
			ast_set2_flag((&global_flags_page2), i || ast_true(v->value), SIP_PAGE2_RTAUTOCLEAR);
		} else if (!strcasecmp(v->name, "usereqphone")) {
			ast_set2_flag((&global_flags), ast_true(v->value), SIP_USEREQPHONE);	
		} else if (!strcasecmp(v->name, "relaxdtmf")) {
			relaxdtmf = ast_true(v->value);
		} else if (!strcasecmp(v->name, "inbandtonedetect")) {
			inbandtonedetect = ast_true(v->value);
		} else if (!strcasecmp(v->name, "checkmwi")) {
			if ((sscanf(v->value, "%d", &global_mwitime) != 1) || (global_mwitime < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid MWI time setting at line %d.  Using default (10).\n", v->value, v->lineno);
				global_mwitime = DEFAULT_MWITIME;
			}
		} else if (!strcasecmp(v->name, "vmexten")) {
			ast_copy_string(global_vmexten, v->value, sizeof(global_vmexten));
		} else if (!strcasecmp(v->name, "mwi_external")) {
			is_mwi_external = ast_true(v->value);
		} else if (!strcasecmp(v->name, "rtptimeout")) {
			if ((sscanf(v->value, "%d", &global_rtptimeout) != 1) || (global_rtptimeout < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid RTP hold time at line %d.  Using default.\n", v->value, v->lineno);
				global_rtptimeout = 0;
			}
		} else if (!strcasecmp(v->name, "rtpholdtimeout")) {
			if ((sscanf(v->value, "%d", &global_rtpholdtimeout) != 1) || (global_rtpholdtimeout < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid RTP hold time at line %d.  Using default.\n", v->value, v->lineno);
				global_rtpholdtimeout = 0;
			}
		} else if (!strcasecmp(v->name, "rtpkeepalive")) {
			if ((sscanf(v->value, "%d", &global_rtpkeepalive) != 1) || (global_rtpkeepalive < 0)) {
				ast_log(LOG_WARNING, "'%s' is not a valid RTP keepalive time at line %d.  Using default.\n", v->value, v->lineno);
				global_rtpkeepalive = 0;
			}
		} else if (!strcasecmp(v->name, "videosupport")) {
			videosupport = ast_true(v->value);
#if defined(T38_SUPPORT)
		} else if (!strcasecmp(v->name, "t38udptlsupport")) {
			t38udptlsupport = ast_true(v->value);
		} else if (!strcasecmp(v->name, "t38rtpsupport")) {
			t38rtpsupport = ast_true(v->value);
		} else if (!strcasecmp(v->name, "t38tcpsupport")) {
			t38tcpsupport = ast_true(v->value);
#endif
		} else if (!strcasecmp(v->name, "compactheaders")) {
			compactheaders = ast_true(v->value);
		} else if (!strcasecmp(v->name, "notifymimetype")) {
			ast_copy_string(default_notifymime, v->value, sizeof(default_notifymime));
		} else if (!strcasecmp(v->name, "notifyringing")) {
			global_notifyringing = ast_true(v->value);
		} else if (!strcasecmp(v->name, "musicclass") || !strcasecmp(v->name, "musiconhold")) {
			ast_copy_string(global_musicclass, v->value, sizeof(global_musicclass));
		} else if (!strcasecmp(v->name, "language")) {
			ast_copy_string(default_language, v->value, sizeof(default_language));
		} else if (!strcasecmp(v->name, "regcontext")) {
			ast_copy_string(regcontext, v->value, sizeof(regcontext));
			/* Create context if it doesn't exist already */
			if (!ast_context_find(regcontext))
				ast_context_create(NULL, regcontext, channeltype);
		} else if (!strcasecmp(v->name, "callerid")) {
			ast_copy_string(default_callerid, v->value, sizeof(default_callerid));
		} else if (!strcasecmp(v->name, "fromdomain")) {
			ast_copy_string(default_fromdomain, v->value, sizeof(default_fromdomain));
		} else if (!strcasecmp(v->name, "outboundproxy")) {
			if (ast_get_ip_or_srv(&outboundproxyip, v->value, "_sip._udp") < 0)
				ast_log(LOG_WARNING, "Unable to locate host '%s'\n", v->value);
		} else if (!strcasecmp(v->name, "outboundproxyport")) {
			/* Port needs to be after IP */
			sscanf(v->value, "%d", &format);
			outboundproxyip.sin_port = htons(format);
		} else if (!strcasecmp(v->name, "autocreatepeer")) {
			autocreatepeer = ast_true(v->value);
		} else if (!strcasecmp(v->name, "srvlookup")) {
			srvlookup = ast_true(v->value);
		} else if (!strcasecmp(v->name, "srv_failover_enabled")) {
			srv_failover_enabled = ast_true(v->value);
		} else if (!strcasecmp(v->name, "minsrvttl")) {
			min_srv_ttl = atoi(v->value);
		} else if (!strcasecmp(v->name, "srvrecovertime")) {
			srv_recover_time = atoi(v->value);
		} else if (!strcasecmp(v->name, "regrecovertime")) {
			reg_recover_time = atoi(v->value);
		} else if (!strcasecmp(v->name, "pedantic")) {
			pedanticsipchecking = ast_true(v->value);
		} else if (!strcasecmp(v->name, "maxexpirey") || !strcasecmp(v->name, "maxexpiry")) {
			max_expiry = atoi(v->value);
			if (max_expiry < 1)
				max_expiry = DEFAULT_MAX_EXPIRY;
		} else if (!strcasecmp(v->name, "defaultexpiry_server")){
			default_expiry_server = atoi(v->value);
			if (default_expiry_server  < 1)
				default_expiry_server = DEFAULT_MAX_EXPIRY; 
		} else if (!strcasecmp(v->name, "check_for_unique_register")){
		      if(!strcasecmp(v->value, "yes")){
		          check_for_unique_register = 1;
		      }
		} else if (!strcasecmp(v->name, "instantdialenabled")) {
			instant_dial_enabled = atoi(v->value);
		} else if (!strcasecmp(v->name, "use_asserted_identity")) {
			use_asserted_identity = atoi(v->value);
		} else if (!strcasecmp(v->name, "unregister_existing_bindings")) {
			unregister_existing_bindings = atoi(v->value);
		} else if (!strcasecmp(v->name, "ip_changed")) {
			ip_changed = atoi(v->value);
		} else if (!strcasecmp(v->name, "defaultexpiry") || !strcasecmp(v->name, "defaultexpirey")) {
			default_expiry = atoi(v->value);
			if (default_expiry < 1)
				default_expiry = DEFAULT_DEFAULT_EXPIRY;
		} else if (!strcasecmp(v->name, "regspacing")) {
			default_regspacing = atoi(v->value);
			if (default_regspacing < 1)
				default_regspacing = DEFAULT_REGISTRATION_SPACE;
		} else if (!strcasecmp(v->name, "regperiod")) {
			default_reg_period = atoi(v->value);
			if (default_reg_period < 0)
				default_reg_period = DEFAULT_DEFAULT_EXPIRY;
		} else if (!strcasecmp(v->name, "regdomain")) {
			ast_copy_string(default_domain, v->value, sizeof(default_domain));
		} else if (!strcasecmp(v->name, "sipdebug")) {
			if (ast_true(v->value))
				sipdebug |= SIP_DEBUG_CONFIG;
		} else if (!strcasecmp(v->name, "subscribetimeout")) {
			global_sub_timeout = atoi(v->value);
		} else if (!strcasecmp(v->name, "subscribeexpiration")) {
			default_subscription_expiry = atoi(v->value);
		} else if (!strcasecmp(v->name, "dumphistory")) {
			dumphistory = ast_true(v->value);
		} else if (!strcasecmp(v->name, "recordhistory")) {
			recordhistory = ast_true(v->value);
		} else if (!strcasecmp(v->name, "registertimeout")) {
			global_reg_timeout = atoi(v->value);
			if (global_reg_timeout < 1)
				global_reg_timeout = DEFAULT_REGISTRATION_TIMEOUT;
		} else if (!strcasecmp(v->name, "subscribeattempts")) {
			global_subattempts_max = atoi(v->value);
		} else if (!strcasecmp(v->name, "registerattempts")) {
			global_regattempts_max = atoi(v->value);
		} else if (!strcasecmp(v->name, "maxsessions")) {
		    	global_max_sessions = atoi(v->value);
		} else if (!strcasecmp(v->name, "maxfromtemplates")) {
		    	global_max_from_templates = atoi(v->value);
                } else if (!strcasecmp(v->name, "expires_renew_percentage")) {
                        expires_renew_percentage = atoi(v->value);
		} else if (!strcasecmp(v->name, "bindaddr")) {
			if (!(hp = ast_gethostbyname(v->value, &ahp))) {
				ast_log(LOG_WARNING, "Invalid address: %s\n", v->value);
			} else {
				memcpy(&bindaddr.sin_addr, hp->h_addr, sizeof(bindaddr.sin_addr));
			}
		} else if (!strcasecmp(v->name, "localnet")) {
			struct ast_ha *na;
			if (!(na = ast_append_ha("d", v->value, localaddr)))
				ast_log(LOG_WARNING, "Invalid localnet value: %s\n", v->value);
			else
				localaddr = na;
		} else if (!strcasecmp(v->name, "register_whitelist")) {
			if (!(register_whitelist  = ast_append_ha("p", v->value, NULL)))
				ast_log(LOG_WARNING, "Invalid value for the white list: %s\n", v->value);
		} else if (!strcasecmp(v->name, "localmask")) {
			ast_log(LOG_WARNING, "Use of localmask is no long supported -- use localnet with mask syntax\n");
		} else if (!strcasecmp(v->name, "externip")) {
			if (!(hp = ast_gethostbyname(v->value, &ahp))) 
				ast_log(LOG_WARNING, "Invalid address for externip keyword: %s\n", v->value);
			else
				memcpy(&externip.sin_addr, hp->h_addr, sizeof(externip.sin_addr));
			externexpire = 0;
		} else if (!strcasecmp(v->name, "externhost")) {
			ast_copy_string(externhost, v->value, sizeof(externhost));
			if (!(hp = ast_gethostbyname(externhost, &ahp))) 
				ast_log(LOG_WARNING, "Invalid address for externhost keyword: %s\n", externhost);
			else
				memcpy(&externip.sin_addr, hp->h_addr, sizeof(externip.sin_addr));
			time(&externexpire);
		} else if (!strcasecmp(v->name, "externrefresh")) {
			if (sscanf(v->value, "%d", &externrefresh) != 1) {
				ast_log(LOG_WARNING, "Invalid externrefresh value '%s', must be an integer >0 at line %d\n", v->value, v->lineno);
				externrefresh = 10;
			}
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&global_prefs, &global_capability,
				v->value, 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&global_prefs, &global_capability, v->value, 0);
		} else if (!strcasecmp(v->name, "allowexternaldomains")) {
			allow_external_domains = ast_true(v->value);
		} else if (!strcasecmp(v->name, "autodomain")) {
			auto_sip_domains = ast_true(v->value);
		} else if (!strcasecmp(v->name, "domain")) {
			char *domain = ast_strdupa(v->value);
			char *context = strchr(domain, ',');

			if (context)
				*context++ = '\0';

			if (ast_strlen_zero(domain))
				ast_log(LOG_WARNING, "Empty domain specified at line %d\n", v->lineno);
			else if (ast_strlen_zero(context))
				ast_log(LOG_WARNING, "Empty context specified at line %d for domain '%s'\n", v->lineno, domain);
			else
				add_sip_domain(ast_strip(domain), SIP_DOMAIN_CONFIG, context ? ast_strip(context) : "");
		} else if (!strcasecmp(v->name, "register")) {
			sip_register(v->value, v->lineno);
		} else if (!strcasecmp(v->name, "subscribe")) {
			sip_subscribe(v->value, v->lineno);
		} else if (!strcasecmp(v->name, "tos")) {
			if (ast_str2tos(v->value, &tos))
				ast_log(LOG_WARNING, "Invalid tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
		} else if (!strcasecmp(v->name, "so_mark")) {
		    int so_mark_val;
		    if (sscanf(v->value, "%i", &so_mark_val) == 1)
			{
				so_mark = so_mark_val;
				ast_set_so_mark(so_mark);
			}
		    else
			ast_log(LOG_WARNING, "Invalid so_mark value at line %d, should be integer\n", v->lineno);
		} else if (!strcasecmp(v->name, "bindport")) {
			if (sscanf(v->value, "%d", &ourport) == 1) {
				bindaddr.sin_port = htons(ourport);
			} else {
				ast_log(LOG_WARNING, "Invalid port number '%s' at line %d of %s\n", v->value, v->lineno, config);
			}
		} else if (!strcasecmp(v->name, "qualify")) {
			if (!strcasecmp(v->value, "no")) {
				default_qualify = 0;
			} else if (!strcasecmp(v->value, "yes")) {
				default_qualify = DEFAULT_MAXMS;
			} else if (sscanf(v->value, "%d", &default_qualify) != 1) {
				ast_log(LOG_WARNING, "Qualification default should be 'yes', 'no', or a number of milliseconds at line %d of sip.conf\n", v->lineno);
				default_qualify = 0;
			}
		} else if (!strcasecmp(v->name, "callevents")) {
		    callevents = ast_true(v->value);
		}
		else if (!strcasecmp(v->name, "delay_backup_proxy"))
		{
			if ((sscanf(v->value, "%d", &temp_int) == 1) &&
				temp_int >= 1 && temp_int <= 3600)
			{
				global_vf_s1_delay_backup_proxy = temp_int *
					1000;
			}
			else
			{
				ast_log(LOG_WARNING, "'%s' is not a valid "
					"delay_backup_proxy at line %d.  Using default.\n",
					v->value, v->lineno);
				ast_log(LOG_DEBUG,"delay_backup_proxy set to %d.\n",
					global_vf_s1_delay_backup_proxy);
			}
		}
		else if (!strcasecmp(v->name, "delay_primary_proxy"))
		{
			if ((sscanf(v->value, "%d", &temp_int) == 1) &&
				temp_int >= 1 && temp_int <= 3600)
			{
				global_vf_s2_delay_primary_proxy = temp_int *
				    1000;
			}
			else
			{
				ast_log(LOG_WARNING, "'%s' is not a valid "
					"delay_primary_proxy at line %d.  Using default.\n",
					v->value, v->lineno);
				ast_log(LOG_DEBUG,"delay_primary_proxy set to "
					"%d.\n", global_vf_s2_delay_primary_proxy);
			}
		}
		else if (!strcasecmp(v->name, "delay_recover_primary"))
		{
			if ((sscanf(v->value, "%d", &temp_int) == 1) &&
				temp_int >= 1 && temp_int <= 3600)
			{
				global_vf_t2_delay_recover_primary = temp_int * 
				    1000;
			}
			else
			{
				ast_log(LOG_WARNING, "'%s' is not a valid "
					"delay_recover_primary at line %d.  Using default.\n",
					v->value, v->lineno);
				ast_log(LOG_DEBUG,"delay_recover_primary set to "
					"%d.\n", global_vf_t2_delay_recover_primary);
			}
		}
		else if (!strcasecmp(v->name, "delay_dereg_backup"))
		{
			if ((sscanf(v->value, "%d", &temp_int) == 1) &&
				temp_int >= 1 && temp_int <= 3600)
				global_vf_1s_delay_dereg_backup = temp_int * 
				    1000;
			else
				ast_log(LOG_WARNING, "'%s' is not a valid "
					"delay_dereg_backup at line %d.  Using default.\n",
					v->value, v->lineno);
			ast_log(LOG_DEBUG,"YINON::delay_dereg_backup  set to %d.\n",
				global_vf_1s_delay_dereg_backup);
		} else if (!strcasecmp(v->name, "session-timers")) {
			int i = (int) str2stmode(v->value); 
			if (i < 0) {
				ast_log(LOG_WARNING, "Invalid session-timers '%s' at line %d of %s\n", v->value, v->lineno, config);
				global_st_mode = SESSION_TIMER_MODE_ACCEPT;
			} else {
				global_st_mode = i;
			}
		} else if (!strcasecmp(v->name, "session-expires")) {
			if (sscanf(v->value, "%d", &global_max_se) != 1) {
				ast_log(LOG_WARNING, "Invalid session-expires '%s' at line %d of %s\n", v->value, v->lineno, config);
				global_max_se = DEFAULT_MAX_SE;
			} 
		} else if (!strcasecmp(v->name, "session-minse")) {
			if (sscanf(v->value, "%d", &global_min_se) != 1) {
				ast_log(LOG_WARNING, "Invalid session-minse '%s' at line %d of %s\n", v->value, v->lineno, config);
				global_min_se = DEFAULT_MIN_SE;
			} 
			if (global_min_se < 90) {
				ast_log(LOG_WARNING, "session-minse '%s' at line %d of %s is not allowed to be < 90 secs\n", v->value, v->lineno, config);
				global_min_se = DEFAULT_MIN_SE;
			} 
		} else if (!strcasecmp(v->name, "session-refresher")) {
			int i = (int) str2strefresher(v->value); 
			if (i < 0) {
				ast_log(LOG_WARNING, "Invalid session-refresher '%s' at line %d of %s\n", v->value, v->lineno, config);
				global_st_refresher = SESSION_TIMER_REFRESHER_LOCAL;
			} else {
				global_st_refresher = i;
			}
                } else if (!strcasecmp(v->name, "session-refresher-force")) {
                        global_st_refresher_force = ast_true(v->value);
		} else if (!strcasecmp(v->name, "smsoverip")) {
			ast_set2_flag((&global_flags_page2), ast_true(v->value), SIP_PAGE2_FEATURE_3GPP_SMS);	
		} else if (!strcasecmp(v->name, "tx_reinvite")) {
			modemfax_tx_reinvite_delay = atoi(v->value);
		} else if (!strcasecmp(v->name, "rx_reinvite")) {
			modemfax_rx_reinvite_delay = atoi(v->value);
		} else if (!strcasecmp(v->name, "bw_mgt_upstream")) {
			global_bw_mgt.cfg_upstream = strtoul(v->value, NULL, 10);
		} else if (!strcasecmp(v->name, "highqualitycalls"))
 	                global_highqualitycalls.max = atoi(v->value);
		/* else if (strcasecmp(v->name,"type"))
		 *	ast_log(LOG_WARNING, "Ignoring %s\n", v->name);
		 */
		 v = v->next;
	}

	if (!allow_external_domains && AST_LIST_EMPTY(&domain_list)) {
		ast_log(LOG_WARNING, "To disallow external domains, you need to configure local SIP domains.\n");
		allow_external_domains = 1;
	}
	
	/* Build list of authentication to various SIP realms, i.e. service providers */
 	v = ast_variable_browse(cfg, "authentication");
 	while(v) {
 		/* Format for authentication is auth = username:password@realm */
 		if (!strcasecmp(v->name, "auth")) {
 			authl = add_realm_authentication(authl, v->value, v->lineno);
 		}
 		v = v->next;
 	}
	
	/* Load peers, users and friends */
	cat = ast_category_browse(cfg, NULL);
	while(cat) {
		if (strcasecmp(cat, "general") && strcasecmp(cat, "authentication")) {
			utype = ast_variable_retrieve(cfg, cat, "type");
			if (utype) {
				if (!strcasecmp(utype, "user") || !strcasecmp(utype, "friend")) {
					user = build_user(cat, ast_variable_browse(cfg, cat), 0);
					if (user) {
						ASTOBJ_CONTAINER_LINK(&userl,user);
						ASTOBJ_UNREF(user, sip_destroy_user);
					}
				}
				if (!strcasecmp(utype, "peer") || !strcasecmp(utype, "friend")) {
					peer = build_peer(cat, ast_variable_browse(cfg, cat), 0, 1);
					if (peer) {
						ASTOBJ_CONTAINER_LINK(&peerl,peer);
						ASTOBJ_UNREF(peer, sip_destroy_peer);
					}
				} else if (strcasecmp(utype, "user")) {
					ast_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sip.conf");
				}
			} else
				ast_log(LOG_WARNING, "Section '%s' lacks type\n", cat);
		}
		cat = ast_category_browse(cfg, cat);
	}


	/* load sip proxy attributes */
	v = ast_variable_browse(cfg, "sip_proxy");
	while(v) {
		if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&sip_proxy_internal_call_prefs, 0, v->value, 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&sip_proxy_internal_call_prefs, 0, v->value, 0);
		}
		v = v->next;
	}

	if (ast_find_ourip(&__ourip, bindaddr)) {
		ast_log(LOG_WARNING, "Unable to get own IP address, SIP disabled\n");
		ast_config_destroy(cfg);
		return 0;
	}
	if (!ntohs(bindaddr.sin_port))
		bindaddr.sin_port = ntohs(DEFAULT_SIP_PORT);
	bindaddr.sin_family = AF_INET;
	ast_mutex_lock(&netlock);
	if (sipsock > -1) {
		close(sipsock);
		sipsock = -1;
	}
	if (sipsock < 0) {
		sipsock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sipsock < 0) {
			ast_log(LOG_WARNING, "Unable to create SIP socket: %s\n", strerror(errno));
		} else {
			/* Allow SIP clients on the same host to access us: */
			const int reuseFlag = 1;
			setsockopt(sipsock, SOL_SOCKET, SO_REUSEADDR,
				   (const char*)&reuseFlag,
				   sizeof reuseFlag);

			if (bind(sipsock, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) < 0) {
				ast_log(LOG_WARNING, "Failed to bind to %s:%d: %s\n",
				ast_inet_ntoa(iabuf, sizeof(iabuf), bindaddr.sin_addr), ntohs(bindaddr.sin_port),
				strerror(errno));
				close(sipsock);
				sipsock = -1;
			} else {
				if (option_verbose > 1) { 
					ast_verbose(VERBOSE_PREFIX_2 "SIP Listening on %s:%d\n", 
					ast_inet_ntoa(iabuf, sizeof(iabuf), bindaddr.sin_addr), ntohs(bindaddr.sin_port));
					ast_verbose(VERBOSE_PREFIX_2 "Using TOS bits %d\n", tos);
					ast_verbose(VERBOSE_PREFIX_2 "Using SO_MARK %#x\n", so_mark);
				}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
				if (so_mark && setsockopt(sipsock, SOL_SOCKET, SO_MARK, &so_mark, sizeof(so_mark))) 
					ast_log(LOG_WARNING, "Unable to set SO_MARK to %d\n", so_mark);
#endif
				if (setsockopt(sipsock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos))) 
					ast_log(LOG_WARNING, "Unable to set TOS to %d\n", tos);
			}
		}
	}
	ast_mutex_unlock(&netlock);

	/* Add default domains - host name, IP address and IP:port */
	/* Only do this if user added any sip domain with "localdomains" */
	/* In order to *not* break backwards compatibility */
	/* 	Some phones address us at IP only, some with additional port number */
	if (auto_sip_domains) {
		char temp[MAXHOSTNAMELEN];

		/* First our default IP address */
		if (bindaddr.sin_addr.s_addr) {
			ast_inet_ntoa(temp, sizeof(temp), bindaddr.sin_addr);
			add_sip_domain(temp, SIP_DOMAIN_AUTO, NULL);
		} else {
			ast_log(LOG_NOTICE, "Can't add wildcard IP address to domain list, please add IP address to domain manually.\n");
		}

		/* Our extern IP address, if configured */
		if (externip.sin_addr.s_addr) {
			ast_inet_ntoa(temp, sizeof(temp), externip.sin_addr);
			add_sip_domain(temp, SIP_DOMAIN_AUTO, NULL);
		}

		/* Extern host name (NAT traversal support) */
		if (!ast_strlen_zero(externhost))
			add_sip_domain(externhost, SIP_DOMAIN_AUTO, NULL);
		
		/* Our host name */
		if (!gethostname(temp, sizeof(temp)))
			add_sip_domain(temp, SIP_DOMAIN_AUTO, NULL);
	}

	/* Release configuration from memory */
	ast_config_destroy(cfg);

	/* Load the list of manual NOTIFY types to support */
	if (notify_types)
		ast_config_destroy(notify_types);
	notify_types = ast_config_load(notify_config);

	return 0;
}

/*! \brief  sip_get_rtp_peer: Returns null if we can't reinvite (part of RTP interface) */
static struct ast_rtp *sip_get_rtp_peer(struct ast_channel *chan)
{
	struct sip_pvt *p;
	struct ast_rtp *rtp = NULL;
	p = chan->tech_pvt;
	if (!p)
		return NULL;
	ast_mutex_lock(&p->lock);
	if (p->rtp && ast_test_flag(p, SIP_CAN_REINVITE))
		rtp =  p->rtp;
	ast_mutex_unlock(&p->lock);
	return rtp;
}

/*! \brief  sip_get_vrtp_peer: Returns null if we can't reinvite video (part of RTP interface) */
static struct ast_rtp *sip_get_vrtp_peer(struct ast_channel *chan)
{
	struct sip_pvt *p;
	struct ast_rtp *rtp = NULL;
	p = chan->tech_pvt;
	if (!p)
		return NULL;

	ast_mutex_lock(&p->lock);
	if (p->vrtp && ast_test_flag(p, SIP_CAN_REINVITE))
		rtp = p->vrtp;
	ast_mutex_unlock(&p->lock);
	return rtp;
}

/*! \brief  sip_set_rtp_peer: Set the RTP peer for this call ---*/
static int sip_set_rtp_peer(struct ast_channel *chan, struct ast_rtp *rtp, struct ast_rtp *vrtp, const struct ast_codec_pref *codecs, int nat_active)
{
	struct sip_pvt *p;
	int changed = 0;

	p = chan->tech_pvt;
	if (!p) 
		return -1;
	ast_mutex_lock(&p->lock);
	if (rtp) {
		changed |= ast_rtp_get_peer(rtp, &p->redirip);
	} else if (p->redirip.sin_addr.s_addr || ntohs(p->redirip.sin_port) != 0) {
		memset(&p->redirip, 0, sizeof(p->redirip));
		changed = 1;
	}
	if (vrtp) {
		changed |= ast_rtp_get_peer(vrtp, &p->vredirip);
	} else if (p->vredirip.sin_addr.s_addr || ntohs(p->vredirip.sin_port) != 0) {
		memset(&p->vredirip, 0, sizeof(p->vredirip));
		changed = 1;
	}

	if (codecs && memcmp(&p->redircodecs, codecs, sizeof(p->redircodecs))) {
	    memcpy(&p->redircodecs, codecs, sizeof(p->redircodecs));
	    changed = 1;
	}
	if (changed && !ast_test_flag(p, SIP_GOTREFER)) {
		if (!p->pendinginvite) {
			if (option_debug > 2) {
				char iabuf[INET_ADDRSTRLEN];
				ast_log(LOG_DEBUG, "Sending reinvite on SIP '%s' - It's audio soon redirected to IP %s\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), rtp ? p->redirip.sin_addr : p->ourip));
			}
			transmit_reinvite_with_sdp(p, FALSE);
		} else if (!ast_test_flag(p, SIP_PENDINGBYE)) {
			if (option_debug > 2) {
				char iabuf[INET_ADDRSTRLEN];
				ast_log(LOG_DEBUG, "Deferring reinvite on SIP '%s' - It's audio will be redirected to IP %s\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), rtp ? p->redirip.sin_addr : p->ourip));
			}
			ast_set_flag(p, SIP_NEEDREINVITE);	
		}
	}
	/* Reset lastrtprx timer */
	time(&p->lastrtprx);
	time(&p->lastrtptx);
	ast_mutex_unlock(&p->lock);
	return 0;
}

#if defined(T38_SUPPORT)
static struct ast_udptl *sip_get_udptl_peer(struct ast_channel *chan)
{
	struct sip_pvt *p;
	struct ast_udptl *udptl = NULL;

	p = chan->tech_pvt;
	if (!p)
		return NULL;

	ast_mutex_lock(&p->lock);
	if (p->udptl && ast_test_flag(p, SIP_CAN_REINVITE))
		udptl = p->udptl;
	ast_mutex_unlock(&p->lock);
	return udptl;
}

static int sip_set_udptl_peer(struct ast_channel *chan, struct ast_udptl *udptl)
{
	struct sip_pvt *p;

	p = chan->tech_pvt;
	if (!p) 
		return -1;
	ast_mutex_lock(&p->lock);
	if (udptl)
		ast_udptl_get_peer(udptl, &p->udptlredirip);
	else
		memset(&p->udptlredirip, 0, sizeof(p->udptlredirip));
	if (!ast_test_flag(p, SIP_GOTREFER)) {
		if (!p->pendinginvite) {
			if (option_debug > 2) {
				char iabuf[INET_ADDRSTRLEN];
				ast_log(LOG_DEBUG, "Sending reinvite on SIP '%s' - It's UDPTL soon redirected to IP %s:%d\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), udptl ? p->udptlredirip.sin_addr : p->ourip), udptl ? ntohs(p->udptlredirip.sin_port) : 0);
			}
			transmit_reinvite_with_t38_sdp(p, FALSE);
		} else if (!ast_test_flag(p, SIP_PENDINGBYE)) {
			if (option_debug > 2) {
				char iabuf[INET_ADDRSTRLEN];
				ast_log(LOG_DEBUG, "Deferring reinvite on SIP '%s' - It's UDPTL will be redirected to IP %s:%d\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), udptl ? p->udptlredirip.sin_addr : p->ourip), udptl ? ntohs(p->udptlredirip.sin_port) : 0);
			}
			ast_set_flag(p, SIP_NEEDREINVITE);	
		}
	}
	/* Reset lastrtprx timer */
	time(&p->lastrtprx);
	time(&p->lastrtptx);
	ast_mutex_unlock(&p->lock);
	return 0;
}

static int sip_handle_t38_reinvite(struct ast_channel *chan, struct sip_pvt *pvt, int reinvite)
{
	struct sip_pvt *p;
	int flag = 0;

	p = chan->tech_pvt;
	if (!p) 
		return -1;
	if (!pvt->udptl)
		return -1;

	/* Setup everything on the other side like offered/responded from first side */
	ast_mutex_lock(&p->lock);
	p->t38jointcapability = p->t38peercapability = pvt->t38jointcapability; 
	ast_udptl_set_far_max_datagram(p->udptl, ast_udptl_get_local_max_datagram(pvt->udptl));
	ast_udptl_set_local_max_datagram(p->udptl, ast_udptl_get_local_max_datagram(pvt->udptl));
	ast_udptl_set_error_correction_scheme(p->udptl, ast_udptl_get_error_correction_scheme(pvt->udptl));

	if (reinvite) {  	/* If we are handling sending re-invite to the other side of the bridge */
		if (ast_test_flag(p, SIP_CAN_REINVITE) && ast_test_flag(pvt, SIP_CAN_REINVITE)) {
			ast_udptl_get_peer(pvt->udptl, &p->udptlredirip);
			flag =1;
		} else {
			memset(&p->udptlredirip, 0, sizeof(p->udptlredirip));
		}
		if (!ast_test_flag(p, SIP_GOTREFER)) {
			if (!p->pendinginvite) {
				if (option_debug > 2) {
					char iabuf[INET_ADDRSTRLEN];
					if (flag)
						ast_log(LOG_DEBUG, "Sending reinvite on SIP '%s' - It's UDPTL soon redirected to IP %s:%d\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), p->udptlredirip.sin_addr), ntohs(p->udptlredirip.sin_port));
					else
						ast_log(LOG_DEBUG, "Sending reinvite on SIP '%s' - It's UDPTL soon redirected to us (IP %s)\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip));
				}
				transmit_reinvite_with_t38_sdp(p, FALSE);
			} else if (!ast_test_flag(p, SIP_PENDINGBYE)) {
				if (option_debug > 2) {
					char iabuf[INET_ADDRSTRLEN];
					if (flag)
						ast_log(LOG_DEBUG, "Deferring reinvite on SIP '%s' - It's UDPTL will be redirected to IP %s:%d\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), p->udptlredirip.sin_addr), ntohs(p->udptlredirip.sin_port));
					else
						ast_log(LOG_DEBUG, "Deferring reinvite on SIP '%s' - It's UDPTL will be redirected to us (IP %s)\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip));
				}
				ast_set_flag(p, SIP_NEEDREINVITE);	
			}
		}
		/* Reset lastrtprx timer */
		time(&p->lastrtprx);
		time(&p->lastrtptx);
		ast_mutex_unlock(&p->lock);
		return 0;
	} else { 	/* If we are handling sending 200 OK to the other side of the bridge */
		if (ast_test_flag(p, SIP_CAN_REINVITE) && ast_test_flag(pvt, SIP_CAN_REINVITE)) {
			ast_udptl_get_peer(pvt->udptl, &p->udptlredirip);
			flag = 1;
		} else {
			memset(&p->udptlredirip, 0, sizeof(p->udptlredirip));
		}
		if (option_debug > 2) {
			char iabuf[INET_ADDRSTRLEN];
			if (flag)
				ast_log(LOG_DEBUG, "Responding 200 OK on SIP '%s' - It's UDPTL soon redirected to IP %s:%d\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), p->udptlredirip.sin_addr), ntohs(p->udptlredirip.sin_port));
			else
				ast_log(LOG_DEBUG, "Responding 200 OK on SIP '%s' - It's UDPTL soon redirected to us (IP %s)\n", p->callid, ast_inet_ntoa(iabuf, sizeof(iabuf), p->ourip));
		}
		pvt->t38state = T38_ENABLED;
		p->t38state = T38_ENABLED;
		ast_log(LOG_DEBUG, "T38 changed state to %d on channel %s\n", pvt->t38state, pvt->owner ? pvt->owner->name : "<none>");
		ast_log(LOG_DEBUG, "T38 changed state to %d on channel %s\n", p->t38state, chan ? chan->name : "<none>");
		transmit_response_with_t38_sdp(p, "200 OK", &p->initreq, 1, FALSE);
		time(&p->lastrtprx);
		time(&p->lastrtptx);
		ast_mutex_unlock(&p->lock);
		return 0;
	}
}
#endif

static char *synopsis_dtmfmode = "Change the dtmfmode for a SIP call";
static char *descrip_dtmfmode = "SIPDtmfMode(inband|info|rfc2833): Changes the dtmfmode for a SIP call\n";
static char *app_dtmfmode = "SIPDtmfMode";

static char *app_sipaddheader = "SIPAddHeader";
static char *synopsis_sipaddheader = "Add a SIP header to the outbound call";


static char *descrip_sipaddheader = ""
"  SIPAddHeader(Header: Content)\n"
"Adds a header to a SIP call placed with DIAL.\n"
"Remember to user the X-header if you are adding non-standard SIP\n"
"headers, like \"X-Asterisk-Accountcode:\". Use this with care.\n"
"Adding the wrong headers may jeopardize the SIP dialog.\n"
"Always returns 0\n";

static char *app_sipgetheader = "SIPGetHeader";
static char *synopsis_sipgetheader = "Get a SIP header from an incoming call";
 
static char *descrip_sipgetheader = ""
"  SIPGetHeader(var=headername): \n"
"Sets a channel variable to the content of a SIP header\n"
"Skips to priority+101 if header does not exist\n"
"Otherwise returns 0\n";

/*! \brief  sip_dtmfmode: change the DTMFmode for a SIP call (application) ---*/
static int sip_dtmfmode(struct ast_channel *chan, void *data)
{
	struct sip_pvt *p;
	char *mode;
	if (data)
		mode = (char *)data;
	else {
		ast_log(LOG_WARNING, "This application requires the argument: info, inband, rfc2833\n");
		return 0;
	}
	ast_mutex_lock(&chan->lock);
	if (chan->type != channeltype) {
		ast_log(LOG_WARNING, "Call this application only on SIP incoming calls\n");
		ast_mutex_unlock(&chan->lock);
		return 0;
	}
	p = chan->tech_pvt;
	if (!p) {
		ast_mutex_unlock(&chan->lock);
		return 0;
	}
	ast_mutex_lock(&p->lock);
	if (!strcasecmp(mode,"info")) {
		ast_clear_flag(p, SIP_DTMF);
		ast_set_flag(p, SIP_DTMF_INFO);
	} else if (!strcasecmp(mode,"rfc2833")) {
		ast_clear_flag(p, SIP_DTMF);
		ast_set_flag(p, SIP_DTMF_RFC2833);
	} else if (!strcasecmp(mode,"inband")) { 
		ast_clear_flag(p, SIP_DTMF);
		ast_set_flag(p, SIP_DTMF_INBAND);
	} else
		ast_log(LOG_WARNING, "I don't know about this dtmf mode: %s\n",mode);
	if (ast_test_flag(p, SIP_DTMF) == SIP_DTMF_INBAND && inbandtonedetect) {
		if (!p->vad) {
			p->vad = ast_dsp_new();
			ast_dsp_set_features(p->vad, DSP_FEATURE_DTMF_DETECT);
		}
	} else {
		if (p->vad) {
			ast_dsp_free(p->vad);
			p->vad = NULL;
		}
	}
	ast_mutex_unlock(&p->lock);
	ast_mutex_unlock(&chan->lock);
	return 0;
}

/*! \brief  sip_addheader: Add a SIP header ---*/
static int sip_addheader(struct ast_channel *chan, void *data)
{
	int no = 0;
	int ok = 0;
	char varbuf[128];
	
	if (ast_strlen_zero((char *)data)) {
		ast_log(LOG_WARNING, "This application requires the argument: Header\n");
		return 0;
	}
	ast_mutex_lock(&chan->lock);

	/* Check for headers */
	while (!ok && no <= 50) {
		no++;
		snprintf(varbuf, sizeof(varbuf), "_SIPADDHEADER%02d", no);
		if (ast_strlen_zero(pbx_builtin_getvar_helper(chan, varbuf + 1)))
			ok = 1;
	}
	if (ok) {
		pbx_builtin_setvar_helper (chan, varbuf, (char *)data);
		if (sipdebug)
			ast_log(LOG_DEBUG,"SIP Header added \"%s\" as %s\n", (char *) data, varbuf);
	} else {
		ast_log(LOG_WARNING, "Too many SIP headers added, max 50\n");
	}
	ast_mutex_unlock(&chan->lock);
	return 0;
}

/*! \brief  sip_getheader: Get a SIP header (dialplan app) ---*/
static int sip_getheader(struct ast_channel *chan, void *data)
{
	static int dep_warning = 0;
	struct sip_pvt *p;
	char *argv, *varname = NULL, *header = NULL, *content;
	
	if (!dep_warning) {
		ast_log(LOG_WARNING, "SIPGetHeader is deprecated, use the SIP_HEADER function instead.\n");
		dep_warning = 1;
	}

	argv = ast_strdupa(data);
	if (!argv) {
		ast_log(LOG_DEBUG, "Memory allocation failed\n");
		return 0;
	}

	if (strchr (argv, '=') ) {	/* Pick out argumenet */
		varname = strsep (&argv, "=");
		header = strsep (&argv, "\0");
	}

	if (!varname || !header) {
		ast_log(LOG_DEBUG, "SipGetHeader: Ignoring command, Syntax error in argument\n");
		return 0;
	}

	ast_mutex_lock(&chan->lock);
	if (chan->type != channeltype) {
		ast_log(LOG_WARNING, "Call this application only on incoming SIP calls\n");
		ast_mutex_unlock(&chan->lock);
		return 0;
	}

	p = chan->tech_pvt;
	content = get_header(&p->initreq, header);	/* Get the header */
	if (!ast_strlen_zero(content)) {
		pbx_builtin_setvar_helper(chan, varname, content);
	} else {
		ast_log(LOG_WARNING,"SIP Header %s not found for channel variable %s\n", header, varname);
		ast_goto_if_exists(chan, chan->context, chan->exten, chan->priority + 101);
	}
	
	ast_mutex_unlock(&chan->lock);
	return 0;
}

/*! \brief  sip_sipredirect: Transfer call before connect with a 302 redirect ---*/
/* Called by the transfer() dialplan application through the sip_transfer() */
/* pbx interface function if the call is in ringing state */
/* coded by Martin Pycko (m78pl@yahoo.com) */
static int sip_sipredirect(struct sip_pvt *p, const char *dest)
{
	char *cdest;
	char *extension, *host, *port;
	char tmp[80];
	
	cdest = ast_strdupa(dest);
	if (!cdest) {
		ast_log(LOG_ERROR, "Problem allocating the memory\n");
		return 0;
	}
	extension = strsep(&cdest, "@");
	host = strsep(&cdest, ":");
	port = strsep(&cdest, ":");
	if (!extension) {
		ast_log(LOG_ERROR, "Missing mandatory argument: extension\n");
		return 0;
	}

	/* we'll issue the redirect message here */
	if (!host) {
		char *localtmp;
		ast_copy_string(tmp, get_header(&p->initreq, "To"), sizeof(tmp));
		if (!strlen(tmp)) {
			ast_log(LOG_ERROR, "Cannot retrieve the 'To' header from the original SIP request!\n");
			return 0;
		}
		if ((localtmp = strstr(tmp, "sip:")) && (localtmp = strchr(localtmp, '@'))) {
			char lhost[80], lport[80];
			memset(lhost, 0, sizeof(lhost));
			memset(lport, 0, sizeof(lport));
			localtmp++;
			/* This is okey because lhost and lport are as big as tmp */
			sscanf(localtmp, "%[^<>:; ]:%[^<>:; ]", lhost, lport);
			if (!strlen(lhost)) {
				ast_log(LOG_ERROR, "Can't find the host address\n");
				return 0;
			}
			host = ast_strdupa(lhost);
			if (!host) {
				ast_log(LOG_ERROR, "Problem allocating the memory\n");
				return 0;
			}
			if (!ast_strlen_zero(lport)) {
				port = ast_strdupa(lport);
				if (!port) {
					ast_log(LOG_ERROR, "Problem allocating the memory\n");
					return 0;
				}
			}
		}
	}

	snprintf(p->our_contact, sizeof(p->our_contact), "Transfer <sip:%s@%s%s%s>", extension, host, port ? ":" : "", port ? port : "");
	transmit_response_reliable(p, "302 Moved Temporarily", &p->initreq, 1);

	/* this is all that we want to send to that SIP device */
	ast_set_flag(p, SIP_ALREADYGONE);

	/* hangup here */
	return -1;
}

#if defined(T38_SUPPORT)
/*--- sip_udptl: Interface structure with callbacks used to connect to UDPTL module --*/
static struct ast_udptl_protocol sip_udptl = {
	type: "SIP",
	get_udptl_info: sip_get_udptl_peer,
	set_udptl_peer: sip_set_udptl_peer,
};
#endif

/*! \brief  sip_rtp: Interface structure with callbacks used to connect to rtp module --*/
static struct ast_rtp_protocol sip_rtp = {
	type: channeltype,
	get_rtp_info: sip_get_rtp_peer,
	get_vrtp_info: sip_get_vrtp_peer,
	set_rtp_peer: sip_set_rtp_peer,
};

/*! \brief  sip_poke_all_peers: Send a poke to all known peers */
static void sip_poke_all_peers(void)
{
	ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
		ASTOBJ_WRLOCK(iterator);
		sip_poke_peer(iterator);
		ASTOBJ_UNLOCK(iterator);
	} while (0)
	);
}

/*--- sip_send_all_subscriptions: Send all known subscriptions */
static void sip_send_all_subscriptions(void)
{
	int ms;

	ASTOBJ_CONTAINER_TRAVERSE(&subl, 1, do {
		ASTOBJ_WRLOCK(iterator);
		if (iterator->expire > -1)
			ast_sched_del(sched, iterator->expire);
		ms = (rand() >> 12) & 0x1fff;
		iterator->expire = ast_sched_add(sched, ms, sip_resubscribe, iterator);
		ASTOBJ_UNLOCK(iterator);
	} while (0)
	);
}

/* Send register to all registries if user==NULL, else only to that user's
 * registry. if unregister is true, send unregister instead (expiry=0) */
static void sip_send_registers(int unregister, char *user)
{
	int ms = DEFAULT_REGISTRATION_SPACE;
	int regspacing;
	if (!regobjs)
		return;
	regspacing = default_expiry * 1000/regobjs;
	if (regspacing > default_regspacing)
		regspacing = default_regspacing;
	ASTOBJ_CONTAINER_TRAVERSE(&regl, 1, do {
		ASTOBJ_WRLOCK(iterator);
		/* avoid starting backup proxys */
		if ((!user && !iterator->backup) ||
		    (user && !strcmp(user, iterator->username)))
		{
			if (iterator->expire > -1)
				ast_sched_del(sched, iterator->expire);
			iterator->expire = ast_sched_add(sched, ms,
			     (unregister? sip_reunregister :
			     sip_expire_redoregister), iterator);
			ms += unregister ? DEFAULT_REGISTRATION_SPACE : regspacing;
		}
		ASTOBJ_UNLOCK(iterator);
	} while (0)
	);
}

/*! \brief  sip_send_all_registers: Send all known registrations */
static void sip_send_all_registers(int unregister)
{
    sip_send_registers(unregister, NULL);
}

struct in_addr sip_get_ourip(struct ast_channel *chan)
{
    struct sip_pvt *pvt = NULL;
    struct in_addr addr = {};

    if (chan->type == channeltype)
        pvt = chan->tech_pvt;
    if (pvt)
        addr = pvt->ourip;

    return addr;
}

/*! \brief  sip_do_reload: Reload module */
static int sip_do_reload(void)
{
	clear_realm_authentication(authl);
	clear_sip_domains();
	authl = NULL;

	/* First, destroy all outstanding registry calls */
	/* This is needed, since otherwise active registry entries will not be destroyed */
	ASTOBJ_CONTAINER_TRAVERSE(&regl, 1, do {
		ASTOBJ_RDLOCK(iterator);
		if (iterator->call) {
			if (option_debug > 2)
				ast_log(LOG_DEBUG, "Destroying active SIP dialog for registry %s@%s\n", iterator->username, iterator->hostname);
			/* This will also remove references to the registry */
			sip_destroy(iterator->call);
		}
		ASTOBJ_UNLOCK(iterator);
	} while(0));

	/* Second, unref all registries from peers. */
	ASTOBJ_CONTAINER_TRAVERSE(&peerl, 1, do {
		ASTOBJ_RDLOCK(iterator);
		if (iterator->registry) 
			ASTOBJ_UNREF(iterator->registry, sip_registry_destroy);
		ASTOBJ_UNLOCK(iterator);
	} while(0));

	ASTOBJ_CONTAINER_DESTROYALL(&userl, sip_destroy_user);
	ASTOBJ_CONTAINER_DESTROYALL(&regl, sip_registry_destroy);
	ASTOBJ_CONTAINER_DESTROYALL(&subl, sip_subscription_destroy);
	ASTOBJ_CONTAINER_MARKALL(&peerl);
	reload_config();
	/* Prune peers who still are supposed to be deleted */
	ASTOBJ_CONTAINER_PRUNE_MARKED(&peerl, sip_destroy_peer);

	/* Hang up calls for unloaded sip users */	
	sip_hangup_disabled_users();

	sip_poke_all_peers();
	sip_send_all_registers(0);
	sip_send_all_subscriptions();

	return 0;
}

/*! \brief  sip_reload: Force reload of module from cli ---*/
static int sip_reload(int fd, int argc, char *argv[])
{

	ast_mutex_lock(&sip_reload_lock);
	if (sip_reloading) {
		ast_verbose("Previous SIP reload not yet done\n");
	} else
		sip_reloading = 1;
	ast_mutex_unlock(&sip_reload_lock);
	restart_monitor();

	return 0;
}

/*! \brief  reload: Part of Asterisk module interface ---*/
int reload(void)
{
	return sip_reload(0, 0, NULL);
}

int reload_if_changed(void)
{
	if (!ast_config_file_md5_update(config, conf_file_md5))
	{
		ast_log(LOG_DEBUG, "Skipping reload since %s was not changed\n", config);
		return 0;
	}

	return reload();
}

static int sip_hangup_active_calls(int fd, int argc, char *argv[])
{
	struct sip_pvt *sip;
	
	ast_mutex_lock(&iflock);
	sip = iflist;

	while(sip)
	{
		ast_mutex_lock(&sip->lock);
		while(sip->owner && ast_mutex_trylock(&sip->owner->lock))
		{
			ast_mutex_unlock(&sip->lock);
			usleep(1);
			ast_mutex_lock(&sip->lock);
		}
		if (sip->owner)
		{
		    	struct ast_channel *bridged;

			sip->is_cli_hangup = 1;
			ast_log(LOG_DEBUG, "hanging up callid %s\n", sip->callid);
			if ((bridged = ast_bridged_channel(sip->owner)))
			    	bridged->hangupcause = AST_CAUSE_NETWORK_OUT_OF_ORDER;
			sip->owner->hangupcause = AST_CAUSE_NETWORK_OUT_OF_ORDER;
			ast_queue_hangup(sip->owner);
			ast_mutex_unlock(&sip->owner->lock);
		}
		ast_mutex_unlock(&sip->lock);
		sip = sip->next;
	}

	ast_mutex_unlock(&iflock);

	return RESULT_SUCCESS;
}

static int sip_sendmessage_cli(int fd, int argc, char *argv[])
{
	int len;
	char *buf;

	if (argc < 7)
		return RESULT_SHOWUSAGE;

	len = strlen(argv[6]);
	buf = calloc(1, len/2);

	len = ast_hexdecode(argv[6], buf, len/2);
	sip_sendmessage(argv[2], argv[3], argv[4], argv[5], buf, len);
	free(buf);
	return RESULT_SUCCESS;
}

static int sip_unregister(int fd, int argc, char *argv[])
{
    char *user;

    if (argc != 3) 
        return RESULT_SHOWUSAGE;

    user = argv[2];
    sip_send_registers(1, user);

    return RESULT_SUCCESS;
}

static int sip_template_register_forbid(int fd, int argc, char *argv[])
{
    if (argc != 3)
        return RESULT_SHOWUSAGE;

    global_template_registration_forbid = atoi(argv[2]);

    return RESULT_SUCCESS;
}

static struct ast_cli_entry  my_clis[] = {
	{ { "sip", "notify", NULL }, sip_notify, "Send a notify packet to a SIP peer", notify_usage, complete_sipnotify },
	{ { "sip", "show", "objects", NULL }, sip_show_objects, "Show all SIP object allocations", show_objects_usage },
	{ { "sip", "show", "users", NULL }, sip_show_users, "Show defined SIP users", show_users_usage },
	{ { "sip", "show", "user", NULL }, sip_show_user, "Show details on specific SIP user", show_user_usage, complete_sip_show_user },
	{ { "sip", "show", "subscriptions", NULL }, sip_show_subscriptions, "Show active SIP subscriptions", show_subscriptions_usage},
	{ { "sip", "show", "channels", NULL }, sip_show_channels, "Show active SIP channels", show_channels_usage},
	{ { "sip", "show", "channel", NULL }, sip_show_channel, "Show detailed SIP channel info", show_channel_usage, complete_sipch  },
	{ { "sip", "show", "history", NULL }, sip_show_history, "Show SIP dialog history", show_history_usage, complete_sipch  },
	{ { "sip", "show", "domains", NULL }, sip_show_domains, "List our local SIP domains.", show_domains_usage },
	{ { "sip", "show", "settings", NULL }, sip_show_settings, "Show SIP global settings", show_settings_usage  },
	{ { "sip", "debug", NULL }, sip_do_debug, "Enable SIP debugging", debug_usage },
	{ { "sip", "debug", "ip", NULL }, sip_do_debug, "Enable SIP debugging on IP", debug_usage },
	{ { "sip", "debug", "peer", NULL }, sip_do_debug, "Enable SIP debugging on Peername", debug_usage, complete_sip_debug_peer },
	{ { "sip", "show", "peer", NULL }, sip_show_peer, "Show details on specific SIP peer", show_peer_usage, complete_sip_show_peer },
	{ { "sip", "show", "peers", NULL }, sip_show_peers, "Show defined SIP peers", show_peers_usage },
	{ { "sip", "prune", "realtime", NULL }, sip_prune_realtime,
	  "Prune cached Realtime object(s)", prune_realtime_usage },
	{ { "sip", "prune", "realtime", "peer", NULL }, sip_prune_realtime,
	  "Prune cached Realtime peer(s)", prune_realtime_usage, complete_sip_prune_realtime_peer },
	{ { "sip", "prune", "realtime", "user", NULL }, sip_prune_realtime,
	  "Prune cached Realtime user(s)", prune_realtime_usage, complete_sip_prune_realtime_user },
	{ { "sip", "show", "inuse", NULL }, sip_show_inuse, "List all inuse/limits", show_inuse_usage },
	{ { "sip", "show", "registry", NULL }, sip_show_registry, "Show SIP registration status", show_reg_usage },
	{ { "sip", "history", NULL }, sip_do_history, "Enable SIP history", history_usage },
	{ { "sip", "no", "history", NULL }, sip_no_history, "Disable SIP history", no_history_usage },
	{ { "sip", "no", "debug", NULL }, sip_no_debug, "Disable SIP debugging", no_debug_usage },
	{ { "sip", "reload", NULL }, sip_reload, "Reload SIP configuration", sip_reload_usage },
	{ { "sip", "hangup_active_calls", NULL }, sip_hangup_active_calls,
 	    "Hangup all active sip calls", sip_hangup_active_calls_usage },
	{ { "sip", "message", NULL }, sip_sendmessage_cli,
		"send SIP MESSAGE", sip_sendmessage_cli_usage},
	{ { "sip", "unregister", NULL }, sip_unregister,
 	    "Unregister SIP proxy", sip_unregister_usage },
	{ { "sip", "template_register_forbid", NULL }, sip_template_register_forbid, "Indicate template registration status", sip_template_register_forbid_usage },
};

/*! \brief  load_module: PBX load module - initialization ---*/
int load_module()
{
	ASTOBJ_CONTAINER_INIT(&userl);	/* User object list */
	ASTOBJ_CONTAINER_INIT(&peerl);	/* Peer object list */
	ASTOBJ_CONTAINER_INIT(&regl);	/* Registry object list */

	sched = sched_context_create();
	if (!sched) {
		ast_log(LOG_WARNING, "Unable to create schedule context\n");
	}

	io = io_context_create();
	if (!io) {
		ast_log(LOG_WARNING, "Unable to create I/O context\n");
	}

	reload_config();	/* Load the configuration from sip.conf */

	/* Make sure we can register our sip channel type */
	if (ast_channel_register(&sip_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel type %s\n", channeltype);
		return -1;
	}

	/* Register all CLI functions for SIP */
	ast_cli_register_multiple(my_clis, sizeof(my_clis)/ sizeof(my_clis[0]));

	/* Tell the RTP subdriver that we're here */
	ast_rtp_proto_register(&sip_rtp);
#if defined(T38_SUPPORT)
	/* Tell the UDPTL subdriver that we're here */
	ast_udptl_proto_register(&sip_udptl);
#endif
	/* Register dialplan applications */
	ast_register_application(app_dtmfmode, sip_dtmfmode, synopsis_dtmfmode, descrip_dtmfmode);

	/* These will be removed soon */
	ast_register_application(app_sipaddheader, sip_addheader, synopsis_sipaddheader, descrip_sipaddheader);
	ast_register_application(app_sipgetheader, sip_getheader, synopsis_sipgetheader, descrip_sipgetheader);

	/* Register dialplan functions */
	ast_custom_function_register(&sip_header_function);
	ast_custom_function_register(&sippeer_function);
	ast_custom_function_register(&sipchaninfo_function);
	ast_custom_function_register(&checksipdomain_function);

	/* Register manager commands */
	ast_manager_register2("SIPpeers", EVENT_FLAG_SYSTEM, manager_sip_show_peers,
			"List SIP peers (text format)", mandescr_show_peers);
	ast_manager_register2("SIPshowpeer", EVENT_FLAG_SYSTEM, manager_sip_show_peer,
			"Show SIP peer (text format)", mandescr_show_peer);
	ast_manager_register2("SIPpeergetstats", EVENT_FLAG_SYSTEM,
		manager_sip_get_stats, "Get Packet Statistics", NULL);
	ast_manager_register2("SIPpeerresetstats", EVENT_FLAG_SYSTEM,
		manager_sip_reset_stats, "Reset Packet Statistics", NULL);
	ast_manager_register2("SIPsendmessage", EVENT_FLAG_CALL, manager_sip_send_message, "Send a SIP MESSAGE", NULL);
	ast_manager_register2("SIPgettimesincelastanswer", EVENT_FLAG_SYSTEM,
		manager_sip_get_time_since_last_answer, "Get the amount of time passed since the last answered call (secs)", NULL);
	ast_manager_register2("SIPgettimesincelastrtp", EVENT_FLAG_SYSTEM,
		manager_sip_get_time_since_last_rtp, "Get the amount of time passed since the last received RTP (secs)", NULL);
	ast_manager_register2("SIPisserverna", EVENT_FLAG_SYSTEM,
		manager_sip_is_server_na, "At least one SIP server is unavailable", NULL);

	sip_poke_all_peers();	
	sip_send_all_registers(0);
	sip_send_all_subscriptions();
	
	/* And start the monitor for the first time */
	restart_monitor();

	return 0;
}

int unload_module()
{
	struct sip_pvt *p, *pl;
	
	/* First, take us out of the channel type list */
	ast_channel_unregister(&sip_tech);

	ast_custom_function_unregister(&sipchaninfo_function);
	ast_custom_function_unregister(&sippeer_function);
	ast_custom_function_unregister(&sip_header_function);
	ast_custom_function_unregister(&checksipdomain_function);

	ast_unregister_application(app_dtmfmode);
	ast_unregister_application(app_sipaddheader);
	ast_unregister_application(app_sipgetheader);

	ast_cli_unregister_multiple(my_clis, sizeof(my_clis) / sizeof(my_clis[0]));

	ast_rtp_proto_unregister(&sip_rtp);

#if defined(T38_SUPPORT)
	/* Tell the UDPTL subdriver that we're here */
	ast_udptl_proto_unregister(&sip_udptl);
#endif

	ast_manager_unregister("SIPpeers");
	ast_manager_unregister("SIPshowpeer");
	ast_manager_unregister("SIPpeergetstats");
	ast_manager_unregister("SIPpeerresetstats");
	ast_manager_unregister("SIPsendmessage");

	if (!ast_mutex_lock(&iflock)) {
		/* Hangup all interfaces if they have an owner */
		p = iflist;
		while (p) {
			if (p->owner)
				ast_softhangup(p->owner, AST_SOFTHANGUP_APPUNLOAD);
			p = p->next;
		}
		ast_mutex_unlock(&iflock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the interface list\n");
		return -1;
	}

	if (!ast_mutex_lock(&monlock)) {
		if (monitor_thread && (monitor_thread != AST_PTHREADT_STOP)) {
			pthread_cancel(monitor_thread);
			pthread_kill(monitor_thread, SIGURG);
			if (pthread_join(monitor_thread, NULL))
			    ast_log(LOG_ERROR, "pthread_join FAILED !!!! \n");
		}
		monitor_thread = AST_PTHREADT_STOP;
		ast_mutex_unlock(&monlock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the monitor\n");
		return -1;
	}

	if (!ast_mutex_lock(&iflock)) {
		/* Destroy all the interfaces and free their memory */
		p = iflist;
		while (p) {
			pl = p;
			p = p->next;
			/* Free associated memory */
			ast_mutex_destroy(&pl->lock);
			if (pl->chanvars) {
				ast_variables_destroy(pl->chanvars);
				pl->chanvars = NULL;
			}
			free(pl);
		}
		iflist = NULL;
		ast_mutex_unlock(&iflock);
	} else {
		ast_log(LOG_WARNING, "Unable to lock the interface list\n");
		return -1;
	}

	/* Free memory for local network address mask */
	ast_free_ha(localaddr);

	ast_free_ha(register_whitelist);

	ASTOBJ_CONTAINER_DESTROYALL(&userl, sip_destroy_user);
	ASTOBJ_CONTAINER_DESTROY(&userl);
	ASTOBJ_CONTAINER_DESTROYALL(&peerl, sip_destroy_peer);
	ASTOBJ_CONTAINER_DESTROY(&peerl);
	ASTOBJ_CONTAINER_DESTROYALL(&regl, sip_registry_destroy);
	ASTOBJ_CONTAINER_DESTROY(&regl);

	clear_realm_authentication(authl);
	clear_sip_domains();
	close(sipsock);

	memset(conf_file_md5, 0, MD5_DIGEST_LEN);
		
	return 0;
}

int usecount()
{
	return usecnt;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}

char *description()
{
	return (char *) desc;
}


