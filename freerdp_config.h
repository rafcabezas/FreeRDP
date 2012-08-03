#ifndef __FREERDP_CONFIG_H
#define __FREERDP_CONFIG_H

#define FREERDP_VERSION "1.0"
#define FREERDP_VERSION_FULL "1.0.0"
#define FREERDP_VERSION_MAJOR 1
#define FREERDP_VERSION_MINOR 0
#define FREERDP_VERSION_REVISION 0

/* Include files */
#define HAVE_SYS_PARAM_H
#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H
#endif
#define HAVE_NETDB_H
#define HAVE_FCNTL_H
#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H
#endif
#define HAVE_LIMITS_H
#ifndef HAVE_STDINT_H
#define HAVE_STDINT_H
#endif
#define HAVE_STDBOOL_H
#ifndef HAVE_INTTYPES_H
#define HAVE_INTTYPES_H
#endif

/* Endian */
#undef BIG_ENDIAN

//Remoter:
#define FREERDP_CONFIG_DIR "/dev/null/"
#define FREERDP_KEYMAP_PATH ""
#define PLUGIN_PATH ""
#define EXT_PATH    ""
#define KEYMAP_PATH ""
#ifndef OBJC_INLINE
#define OBJC_INLINE
#endif
#define STATIC_PLUGINS
#define GIT_REVISION 12345
//#define freerdp_log(instance,...) printf(__VA_ARGS__)
void freerdp_log(void *inst, const char * text, ...);

/* Options */
/* #undef WITH_DEBUG_TRANSPORT */
/* #undef WITH_DEBUG_CHANMAN */
/* #undef WITH_DEBUG_SVC */
/* #undef WITH_DEBUG_DVC */
/* #undef WITH_DEBUG_KBD */
/* #undef WITH_DEBUG_NLA */
/* #undef WITH_DEBUG_NEGO */
/* #undef WITH_DEBUG_CERTIFICATE */
/* #undef WITH_DEBUG_LICENSE */
/* #undef WITH_DEBUG_GDI */
/* #undef WITH_DEBUG_ASSERT */
/* #undef WITH_DEBUG_RFX */
/* #undef WITH_PROFILER */
/* #undef WITH_SSE2 */
/* #undef WITH_SSE2_TARGET */
/* #undef WITH_NEON */
#undef WITH_DEBUG_X11
#undef WITH_DEBUG_X11_CLIPRDR
#undef WITH_DEBUG_X11_LOCAL_MOVESIZE
/* #undef WITH_DEBUG_RAIL */
/* #undef WITH_DEBUG_XV */
/* #undef WITH_DEBUG_SCARD */
/* #undef WITH_DEBUG_ORDERS */
/* #undef WITH_DEBUG_REDIR */
/* #undef WITH_DEBUG_CLIPRDR */
/* #undef WITH_DEBUG_WND */
#endif
