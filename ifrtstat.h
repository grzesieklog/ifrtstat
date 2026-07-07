#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/link.h>

#define PROG_NAME "ifrtstat"
#define MAX_IF_LEN 16
#define MAX_GOPT_LEN 20
#define DAY_IN_SEC 86400ULL
#define BITS 8ULL

// units
#define KILO 1000ULL
#define MEGA (KILO * 1000ULL)
#define GIGA (MEGA * 1000ULL)
#define TERA (GIGA * 1000ULL)
#define PETA (TERA * 1000ULL)
#define EKSA (PETA * 1000ULL)

#define KILO_PREFIX "k"
#define MEGA_PREFIX "M"
#define GIGA_PREFIX "G"
#define TERA_PREFIX "T"
#define PETA_PREFIX "P"
#define EKSA_PREFIX "E"

#define BIT_SUM_SUFFIX "b"
#define BYTE_SUM_SUFFIX "B"
#define BIT_RATE_SUFFIX "bps"
#define BYTE_RATE_SUFFIX "Bps"

#define MAXLEN_SUM_UNIT sizeof(EKSA_PREFIX BYTE_SUM_SUFFIX)
#define MAXLEN_RATE_UNIT sizeof(EKSA_PREFIX BYTE_RATE_SUFFIX)

// option flags
#define FLAG_HELP         (1ULL << 0)
#define FLAG_MAX          (1ULL << 1)
#define FLAG_DATE         (1ULL << 2)
#define FLAG_TIMER        (1ULL << 3)
#define FLAG_GREATER      (1ULL << 4)
#define FLAG_BYTES_PS     (1ULL << 5)
#define FLAG_CONV         (1ULL << 6)
#define FLAG_INTERFACE    (1ULL << 7)




