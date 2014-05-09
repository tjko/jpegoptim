/* win32_compat.h
 *
 * compatibility stuff for Windows
 */

#ifndef _WIN32_COMPAT_H
#define _WIN32_COMPAT_H 1

#ifdef	__cplusplus
extern "C" {
#endif


#include <process.h>
#include <errno.h>
#include <io.h> /* _findfirst and _findnext set errno iff they return -1 */
#include <fcntl.h>
#include <sys/utime.h>

#define snprintf _snprintf
#define lstat _stat

#define realpath(N,R) _fullpath((R),(N),MAXPATHLEN)
#define ftruncate(fildes,length) open(fildes, O_TRUNC|O_WRONLY)

#define round(x) ((int) (x))
#define getuid(x) 0
#define geteuid() 0
#define chown(outfname,st_uid,st_gid) 0
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)

#ifndef HOST_TYPE
#if _WIN64
#define HOST_TYPE "Win64"
#else if WIN32
#define HOST_TYPE "Win32"
#endif 
#endif


#ifdef	__cplusplus
}
#endif

#endif /* _WIN32_COMPAT_H */
