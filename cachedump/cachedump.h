#ifndef __CACHEDUMP__
#define __CACHEDUMP__

#define MAX_CACHE_ENTRIES   50 // should be enought most of time
#define BUFFER_SIZE			20480
#define CD_TIMEOUT			10000000 // ms
//#define PIPE_NAME			"\\\\.\\pipe\\cachedumppipe"
#define ERROR_MESSAGE		"ERROR %s (code %d)\n"
#define REG_LSA_CIPHER_KEY	"SECURITY\\Policy\\Secrets\\NL$KM\\CurrVal"
#define REG_SECURITY_CACHE  "SECURITY\\CACHE"
#define REG_CACHE_CONTROL	"NL$Control"
#define CACHE_DUMP_SERVICE_NAME "CacheDump"
#define CHARS_IN_GUID		39

typedef struct _CACHE_CHUNK {
	DWORD size;
	BYTE * data;
} CACHE_CHUNK;

// structure that seems to be used by SystemFunction005
typedef struct _LSA_UNICODE_STRING2 {
    ULONG Length;
    ULONG MaximumLength;
    BYTE* Buffer;
} LSA_UNICODE_STRING2;

typedef int (WINAPI *PSystemFunction005)( LSA_UNICODE_STRING2 *,
										  LSA_UNICODE_STRING2 *,
										  LSA_UNICODE_STRING2 * );

//char ErrorBuffer[ BUFFER_SIZE ];

#endif