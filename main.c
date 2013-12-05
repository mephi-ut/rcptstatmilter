
// === Includes ===

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <libmilter/mfapi.h>
#include <syslog.h>

// === Code ===

extern sfsistat statmilter_cleanup(SMFICTX *, bool);

sfsistat statmilter_connect(SMFICTX *ctx, char *hostname, _SOCK_ADDR *hostaddr) {
	//syslog(LOG_NOTICE, "New mail from %s.\n", hostname);
	return SMFIS_CONTINUE;
}

sfsistat statmilter_helo(SMFICTX *ctx, char *helohost) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_envfrom(SMFICTX *ctx, char **argv) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_envrcpt(SMFICTX *ctx, char **argv) {
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
	smfi_addheader(ctx, "X-FromChk-Milter", "passed");

	return SMFIS_CONTINUE;
}

sfsistat statmilter_abort(SMFICTX *ctx) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_close(SMFICTX *ctx) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_unknown(SMFICTX *ctx, const char *cmd) {
	return SMFIS_CONTINUE;
}

sfsistat statmilter_data(SMFICTX *ctx) {
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
	return SMFIS_ALL_OPTS;
}

static void usage(const char *path) {
	fprintf(stderr, "Usage: %s -p socket-addr [-t timeout]\n",
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

	char setconn = 0;
	int c;
	const char *args = "p:t:h";
	extern char *optarg;
	// Process command line options
	while ((c = getopt(argc, argv, args)) != -1) {
		switch (c) {
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
			case 'h':
			default:
				usage(argv[0]);
				exit(EX_USAGE);
		}
	}
	if (!setconn) {
		fprintf(stderr, "%s: Missing required -p argument\n", argv[0]);
		usage(argv[0]);
		exit(EX_USAGE);
	}
	if (smfi_register(mailfilterdesc) == MI_FAILURE) {
		fprintf(stderr, "smfi_register failed\n");
		exit(EX_UNAVAILABLE);
	}
	openlog(NULL, LOG_PID, LOG_MAIL);
	int ret = smfi_main();
	closelog();
	return ret;
}

