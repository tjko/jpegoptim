/* win32_compat.h
 *
 * compatibility stuff for Windows
 *
 * Thanks to Javier Gutiérrez Chamorro for Windows support.
 */

#ifndef WIN32_COMPAT_H
#define WIN32_COMPAT_H 1

#ifdef	__cplusplus
extern "C" {
#endif


#include <process.h>
#include <errno.h>
#include <io.h> /* _findfirst and _findnext set errno iff they return -1 */
#include <fcntl.h>
#include <sys/utime.h>

#define snprintf _snprintf
#define lstat stat

#define realpath(N,R) _fullpath((R),(N),MAXPATHLEN)
#define ftruncate(fildes,length) open(fildes, O_TRUNC|O_WRONLY)

#define set_filemode_binary(file) _setmode(_fileno(file), _O_BINARY)

#define round(x) ((int) (x))
#define getuid(x) 0
#define geteuid() 0
#define chown(outfname,st_uid,st_gid) 0

#ifndef S_ISREG
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef HOST_TYPE
#if _WIN64
#define HOST_TYPE "Win64"
#elif WIN32
#define HOST_TYPE "Win32"
#endif 
#endif


#ifdef	__cplusplus
}
#endif

#endif /* WIN32_COMPAT_H */
