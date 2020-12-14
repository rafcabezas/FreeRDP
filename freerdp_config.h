#ifndef __FREERDP_CONFIG_H
#define __FREERDP_CONFIG_H

#include "version.h"

#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#if IS_TARGET_OSX==1
#define WITH_CUPS 1
#define WITH_MACAUDIO 1
#else
#define WITH_IOSAUDIO 1
#endif

#define WITH_OPENSSL
#define BUILTIN_CHANNELS

#define EXT_PATH    "~/Documents"

//original
#define FREERDP_DATA_PATH "/usr/local/share/freerdp"
#define FREERDP_PLUGIN_PATH "/usr/local/lib/freerdp"
#define FREERDP_KEYMAP_PATH "/usr/local/share/freerdp/keymaps"
#define FREERDP_LIBRARY_PATH "~/Documents"
#define FREERDP_INSTALL_PREFIX "~/Documents"
#define FREERDP_ADDIN_PATH "~/Documents"

/* Include files */
#define FREERDP_HAVE_LIMITS_H
#define FREERDP_HAVE_STDINT_H
#define FREERDP_HAVE_STDBOOL_H
#define FREERDP_HAVE_INTTYPES_H
#define FREERDP_STATIC_PLUGINS //Remoter
#define STATIC_CHANNELS

void remoterRDPProcessRedirection(void *instance);


//#define HAVE_FCNTL_H
//#define HAVE_UNISTD_H
#define HAVE_INTTYPES_H
#define HAVE_SYS_MODEM_H
#define HAVE_SYS_FILIO_H
#define HAVE_SYS_SELECT_H
#define HAVE_SYS_SOCKIO_H
#define HAVE_SYS_STRTIO_H
//#define HAVE_EVENTFD_H
//#define HAVE_TIMERFD_H
#define HAVE_TM_GMTOFF
#define HAVE_AIO_H
#define HAVE_POLL_H
//#define HAVE_SYSLOG_H
//#define HAVE_JOURNALD_H
//#define HAVE_PTHREAD_MUTEX_TIMEDLOCK
//#define HAVE_VALGRIND_MEMCHECK_H
#define HAVE_EXECINFO_H



//end

//#define OPENSSL_NO_TLSEXT

#if IS_TARGET_OSX==1
#if defined(__x86_64__)
#define WITH_SSE2
#endif
#endif
#define WITH_JPEG

// Debugging
//#define WITH_DEBUG_CERTIFICATE
//#define WITH_DEBUG_CHANNELS
//#define WITH_DEBUG_CLIPRDR
//#define WITH_DEBUG_DVC
//#define WITH_DEBUG_GDI
//#define WITH_DEBUG_KBD
//#define WITH_DEBUG_LICENSE
//#define WITH_DEBUG_NEGO
//#define WITH_DEBUG_NLA
//#define WITH_DEBUG_NTLM
//#define WITH_DEBUG_TSG
//#define WITH_DEBUG_ORDERS
//#define WITH_DEBUG_RAIL
//#define WITH_DEBUG_RDP
//#define WITH_DEBUG_REDIR
//#define WITH_DEBUG_RFX
//#define WITH_DEBUG_SCARD
//#define WITH_DEBUG_SVC
//#define WITH_DEBUG_TRANSPORT
//#define WITH_DEBUG_WND
//#define WITH_DEBUG_X11
//#define WITH_DEBUG_X11_CLIPRDR
//#define WITH_DEBUG_X11_LOCAL_MOVESIZE
//#define WITH_DEBUG_XV

//#define freerdp_log(instance,...) printf(__VA_ARGS__)
void freerdp_log(void *inst, const char *type, const char *tag, const char *p, ...);
const char *freerdp_timezone(void);

/* Options */
/* #undef WITH_PROFILER */
/* #undef WITH_SSE2 */
/* #undef WITH_NEON */
/* #undef WITH_NATIVE_SSPI */

/* Debug */
/* #undef WITH_DEBUG_CERTIFICATE */
/* #undef WITH_DEBUG_CHANNELS */
/* #undef WITH_DEBUG_CLIPRDR */
/* #undef WITH_DEBUG_DVC */
/* #undef WITH_DEBUG_GDI */
/* #undef WITH_DEBUG_KBD */
/* #undef WITH_DEBUG_LICENSE */
/* #undef WITH_DEBUG_NEGO */
/* #undef WITH_DEBUG_NLA */
/* #undef WITH_DEBUG_NTLM */
/* #undef WITH_DEBUG_TSG */
/* #undef WITH_DEBUG_ORDERS */
/* #undef WITH_DEBUG_RAIL */
/* #undef WITH_DEBUG_RDP */
/* #undef WITH_DEBUG_REDIR */
/* #undef WITH_DEBUG_RFX */
/* #undef WITH_DEBUG_SCARD */
/* #undef WITH_DEBUG_SVC */
/* #undef WITH_DEBUG_TRANSPORT */
/* #undef WITH_DEBUG_WND */
/* #undef WITH_DEBUG_X11 */
/* #undef WITH_DEBUG_X11_CLIPRDR */
/* #undef WITH_DEBUG_X11_LOCAL_MOVESIZE */
/* #undef WITH_DEBUG_XV */

#endif
