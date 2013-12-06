
// === Includes ===

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <libmilter/mfapi.h>
#include <syslog.h>
#include <sqlite3.h>
#include <math.h>


// === Global definitions ===

static sqlite3 *db 		= NULL;
static char learning_only	= 0;
static float threshold_temp	=  0.45;
static float threshold_reject	= -0.9;
static float score_start	=  0.4;
static float score_k		=  0.1;
static float slow_down_limit	=  2.72*2.72;

// === Code: DB routines ===

struct stats {
	uint32_t	ipv4addr;
	float		score;
	uint32_t	tries;
	uint8_t		isnew;
};

static int stats_get_callback(struct stats *stats_p, int argc, char **argv, char **colname) {
	int i;
	i=0;
	while(i<argc) {
//		printf("%s: %s\n", colname[i], argv[i]);
		if(!strcmp(colname[i], "score"))
			stats_p->score = atof(argv[i]);
		else
		if(!strcmp(colname[i], "tries"))
			stats_p->tries = atoi(argv[i]);
		i++;
	}

	stats_p->isnew = 0;
	return 0;
}

void stats_get(struct stats *stats_p, uint32_t ipv4address) {
	char query[BUFSIZ];
	int rc;
	char *errmsg = NULL;
	stats_p->ipv4addr = ipv4address;
	stats_p->score    = score_start;
	stats_p->tries    = 0;
	stats_p->isnew    = 1;
	sprintf(query, "SELECT ipv4addr, score, tries FROM statmilter_stats WHERE ipv4addr = %i", ipv4address);
	rc = sqlite3_exec(db, query, (int (*)(void *, int,  char **, char **))stats_get_callback, stats_p, &errmsg);
	if(rc != SQLITE_OK) {
		syslog(LOG_CRIT, "Cannot get statistics from DB: %s. Ignoring.\n", errmsg);
		//exit(EX_SOFTWARE);
	}
	return;
}

void stats_set(struct stats *stats_p) {
	char query[BUFSIZ];
	int rc;
	char *errmsg = NULL;
	if(stats_p->isnew) {
		sprintf(query, "INSERT INTO statmilter_stats VALUES(%i, \"%f\", %u)", 
			stats_p->ipv4addr, stats_p->score, stats_p->tries);
	} else {
		sprintf(query, "UPDATE statmilter_stats SET score=\"%f\", tries=%u WHERE ipv4addr = %i", 
			stats_p->score, stats_p->tries, stats_p->ipv4addr);
	}

//	sqlite3_stmt *stmt = NULL;
//	rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
	rc = sqlite3_exec(db, query, NULL, NULL, &errmsg);
	if(rc != SQLITE_OK) {
		syslog(LOG_CRIT, "Cannot update statistics in DB: %s. Exit.\n", errmsg);
		exit(EX_SOFTWARE);
	}
//	sqlite3_finalize(stmt);
	return;
}

void stats_xxcrease(struct stats *stats_p, float side) {
	float slow_down = logf(stats_p->tries+2);

	if(slow_down > slow_down_limit)
		slow_down = slow_down_limit;

	stats_p->score = stats_p->score + (side - stats_p->score) * score_k / slow_down;
}

inline void stats_decrease(struct stats *stats_p) {
	stats_xxcrease(stats_p, -1);
}

inline void stats_increase(struct stats *stats_p) {
	stats_xxcrease(stats_p, +1);
}

// === Code: milter self ===

extern sfsistat statmilter_cleanup(SMFICTX *, bool);

sfsistat statmilter_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *_hostaddr) {
	if(_hostaddr == NULL) {
		syslog(LOG_NOTICE, "hostaddr is NULL. Skipping this mail. ACCEPT.");
		return SMFIS_ACCEPT;
	}

	struct stats *stats_p = malloc(sizeof(struct stats *));
	smfi_setpriv(ctx, stats_p);

	struct sockaddr_in *hostaddr = (struct sockaddr_in *)_hostaddr;
	uint32_t ipv4address = hostaddr->sin_addr.s_addr;

	stats_get(stats_p, ipv4address);
	syslog(LOG_NOTICE, "New connection from %s (id %s). Stats: new == %u; score == %f; tries == %u", 
		inet_ntoa(hostaddr->sin_addr), smfi_getsymval(ctx, "i"), stats_p->isnew, stats_p->score, stats_p->tries);

	return SMFIS_CONTINUE;
}

sfsistat statmilter_helo(SMFICTX *ctx, char *helohost) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_envfrom(SMFICTX *ctx, char **argv) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_envrcpt(SMFICTX *ctx, char **argv) {
	char *status_code = smfi_getsymval(ctx, "{rcpt_host}");
	char *mailer      = smfi_getsymval(ctx, "{rcpt_mailer}");
	struct stats *stats_p = smfi_getpriv(ctx);
	if(stats_p == NULL)
		return SMFIS_CONTINUE;

	if(!strcmp(status_code, "5.1.1"))
		stats_decrease(stats_p);
	else
	if(!strcmp(mailer, "smtp"))
		stats_increase(stats_p);
	else
		return SMFIS_CONTINUE;

	if(stats_p->tries < ~0)
		stats_p->tries++;

	syslog(LOG_NOTICE, "Updated stats: new == %u; score == %f; tries == %u", 
		stats_p->isnew, stats_p->score, stats_p->tries);

	return SMFIS_CONTINUE;
}

sfsistat statmilter_header(SMFICTX *ctx, char *headerf, char *headerv) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_eoh(SMFICTX *ctx) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_body(SMFICTX *ctx, unsigned char *bodyp, size_t bodylen) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_eom(SMFICTX *ctx) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_abort(SMFICTX *ctx) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_close(SMFICTX *ctx) {
	struct stats *stats_p = smfi_getpriv(ctx);
	if(stats_p == NULL)
		return SMFIS_CONTINUE;

	stats_set(stats_p);
	syslog(LOG_NOTICE, "Closed connection (id %s). Stats: score == %f; tries == %u", 
		smfi_getsymval(ctx, "i"), stats_p->score, stats_p->tries);

	free(stats_p);
	smfi_setpriv(ctx, NULL);
	return SMFIS_CONTINUE;
}

sfsistat statmilter_unknown(SMFICTX *ctx, const char *cmd) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_data(SMFICTX *ctx) {
	struct stats *stats_p = smfi_getpriv(ctx);
	if(stats_p == NULL)
		return SMFIS_CONTINUE;

	if(learning_only) {
		syslog(LOG_NOTICE, "%s: score %f; thres_temp %f; thres_rej %f\n", 
			smfi_getsymval(ctx, "i"), stats_p->score, threshold_temp, threshold_reject);
		return SMFIS_CONTINUE;
	}

	if(stats_p->score < threshold_temp)
		return SMFIS_TEMPFAIL;

	if(stats_p->score < threshold_reject)
		return SMFIS_REJECT;

	return SMFIS_CONTINUE;
}

sfsistat statmilter_negotiate(ctx, f0, f1, f2, f3, pf0, pf1, pf2, pf3)
	SMFICTX *ctx;
	unsigned long f0;
	unsigned long f1;
	unsigned long f2;
	unsigned long f3;
	unsigned long *pf0;
	unsigned long *pf1;
	unsigned long *pf2;
	unsigned long *pf3;
{
	*pf0 = f0;
	*pf1 = SMFIP_RCPT_REJ;
	return SMFIS_CONTINUE;
}

static void usage(const char *path) {
	fprintf(stderr, "Usage: %s -p socket-addr -d db-path [-t timeout] [-l] [-x temp unavail thresh] [-X reject thresh] [-s score start value] [-k score increase/decrease multiplier]\n",
		path);
}

int main(int argc, char *argv[]) {
	struct smfiDesc mailfilterdesc = {
		"StatMilter",			// filter name
		SMFI_VERSION,			// version code -- do not change
		SMFIF_ADDHDRS|SMFIF_ADDRCPT,	// flags
		statmilter_connect,		// connection info filter
		statmilter_helo,		// SMTP HELO command filter
		statmilter_envfrom,		// envelope sender filter
		statmilter_envrcpt,		// envelope recipient filter
		statmilter_header,		// header filter
		statmilter_eoh,			// end of header
		statmilter_body,		// body block filter
		statmilter_eom,			// end of message
		statmilter_abort,		// message aborted
		statmilter_close,		// connection cleanup
		statmilter_unknown,		// unknown SMTP commands
		statmilter_data,		// DATA command
		statmilter_negotiate		// Once, at the start of each SMTP connection
	};

	learning_only = 0;

	char setconn  = 0;
	int c;
	const char *args = "lp:d:t:x:X:s:k:h";
	extern char *optarg;
	// Process command line options
	while ((c = getopt(argc, argv, args)) != -1) {
		switch (c) {
			case 'l':
				learning_only = 1;
				break;
			case 'p':
				if (optarg == NULL || *optarg == '\0')
				{
					(void)fprintf(stderr, "Illegal conn: %s\n",
						optarg);
					exit(EX_USAGE);
				}
				if (smfi_setconn(optarg) == MI_FAILURE)
				{
					(void)fprintf(stderr,
						"smfi_setconn failed\n");
					exit(EX_SOFTWARE);
				}

				if (strncasecmp(optarg, "unix:", 5) == 0)
					unlink(optarg + 5);
				else if (strncasecmp(optarg, "local:", 6) == 0)
					unlink(optarg + 6);
				setconn = 1;
				break;
			case 'd': {
				// Openning the DB
				if(sqlite3_open_v2(optarg, &db, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL)) {
//				if(sqlite3_open(optarg, &db)) {
					fprintf(stderr, "Cannot open SQLite3 DB-file \"%s\"\n", optarg);
					exit(EX_SOFTWARE);
				}

				// Checking it's validness. Fixing if required.
				int rc;
				sqlite3_stmt *stmt = NULL;
				rc = sqlite3_prepare_v2(db, "SELECT ipv4addr, score, tries FROM statmilter_stats LIMIT 1", -1, &stmt, NULL);
				if(rc != SQLITE_OK) {
					// Fixing the table "statmilter_stats"
					fprintf(stderr, "Invalid DB file \"%s\". Recreating table \"statmilter_stats\" in it.\n", optarg);
					sqlite3_exec(db, "DROP TABLE statmilter_stats", NULL, NULL, NULL);
					sqlite3_exec(db, "CREATE TABLE statmilter_stats (ipv4addr INTEGER PRIMARY KEY, score FLOAT, tries INTEGER)", NULL, NULL, NULL);
				}
				sqlite3_finalize(stmt);
				break;
			}
			case 't':
				if (optarg == NULL || *optarg == '\0') {
					(void)fprintf(stderr, "Illegal timeout: %s\n", 
						optarg);
					exit(EX_USAGE);
				}
				if (smfi_settimeout(atoi(optarg)) == MI_FAILURE) {
					(void)fprintf(stderr,
						"smfi_settimeout failed\n");
					exit(EX_SOFTWARE);
				}
				break;
			case 'x':
				threshold_temp	= atof(optarg);
				break;
			case 'X':
				threshold_reject= atof(optarg);
				break;
			case 's':
				score_start	= atof(optarg);
				break;
			case 'k':
				score_k		= atof(optarg);
				break;
			case 'h':
			default:
				usage(argv[0]);
				exit(EX_USAGE);
		}
	}
	if(!setconn) {
		fprintf(stderr, "%s: Missing required -p argument\n", argv[0]);
		usage(argv[0]);
		exit(EX_USAGE);
	}
	if(db==NULL) {
		fprintf(stderr, "%s: Missing required -d argument\n", argv[0]);
		usage(argv[0]);
		exit(EX_USAGE);
	}
	if(smfi_register(mailfilterdesc) == MI_FAILURE) {
		fprintf(stderr, "smfi_register failed\n");
		exit(EX_UNAVAILABLE);
	}

	openlog(NULL, LOG_PID, LOG_MAIL);
	int ret = smfi_main();
	closelog();
	sqlite3_close(db);
	return ret;
}

