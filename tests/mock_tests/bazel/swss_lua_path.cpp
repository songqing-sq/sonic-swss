// Bazel-only shim -- deliberately absent from tests/mock_tests/Makefile.am.
//
// swsscommon reads its lua scripts through loadLuaScript(), which hardcodes the
// directory the libswsscommon deb installs them into: readTextFile("/usr/share/
// swss/" + path) (sonic-swss-common/common/redisapi.h). The constructors of
// ProducerStateTable, ConsumerStateTable, ConsumerTable and CounterTable all go
// through it, so nearly every suite here would throw in a bazel test: there are
// no installed debs, / is not writable, no unprivileged userns is available for
// a bind mount (see the sonic-eventd/sonic-sysmgr hooks), and the path is not
// configurable. Interposing loadLuaScript/readTextFile is not an option either
// -- unlike the swsscommon methods mock_table.cpp replaces, both are header
// inlines compiled straight into libswsscommon.
//
// So the redirect happens one level below them, in the file-opening entry points
// libstdc++ reaches for on behalf of std::ifstream: a path under /usr/share/swss
// is looked up in $SWSS_LUA_DIR instead, which bazel/lua_setup.sh points at the
// scripts staged in runfiles. Anything else -- including a script that was not
// staged -- is forwarded untouched, so with the variable unset this is a no-op
// and swsscommon's error message keeps naming the file it asked for.
//
// This is built as a shared object and LD_PRELOADed rather than linked into the
// test binaries, because some suites interpose on the same entry points
// themselves -- teammgrd/teammgr_ut.cpp defines fopen to hand teammgr.cpp fake
// pid files. Linking both definitions into one executable does not link at all;
// preloading puts this one behind the suite's own, exactly where its
// dlsym(RTLD_NEXT) fallback lands.

// RTLD_NEXT needs _GNU_SOURCE; the toolchain already passes it here, so only
// define it when it is missing rather than provoking a redefinition warning.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// Nothing here reaches for the C++ library: the shim is preloaded into every
// suite, so the fewer objects it drags into the link the less there is to stage.

namespace
{
    const char INSTALLED_LUA_DIR[] = "/usr/share/swss/";

    // Returns path unchanged unless it names a script that was staged, in which
    // case out holds the staged location and is returned instead.
    const char *redirected(const char *path, char *out, size_t out_size)
    {
        const size_t prefix_len = sizeof(INSTALLED_LUA_DIR) - 1;
        if (path == NULL || strncmp(path, INSTALLED_LUA_DIR, prefix_len) != 0)
        {
            return path;
        }

        const char *dir = getenv("SWSS_LUA_DIR");
        if (dir == NULL || *dir == '\0')
        {
            return path;
        }

        int written = snprintf(out, out_size, "%s/%s", dir, path + prefix_len);
        if (written < 0 || static_cast<size_t>(written) >= out_size)
        {
            return path;
        }
        if (access(out, R_OK) != 0)
        {
            return path;
        }
        return out;
    }

    typedef int (*open_fn)(const char *, int, ...);
    typedef FILE *(*fopen_fn)(const char *, const char *);

    int forward_open(const char *symbol, const char *path, int flags, mode_t mode)
    {
        open_fn real = reinterpret_cast<open_fn>(dlsym(RTLD_NEXT, symbol));
        char staged[PATH_MAX];
        return real(redirected(path, staged, sizeof(staged)), flags, mode);
    }

    FILE *forward_fopen(const char *symbol, const char *path, const char *mode)
    {
        fopen_fn real = reinterpret_cast<fopen_fn>(dlsym(RTLD_NEXT, symbol));
        char staged[PATH_MAX];
        return real(redirected(path, staged, sizeof(staged)), mode);
    }

    mode_t creation_mode(int flags, va_list ap)
    {
        // The mode argument only exists when the flags ask for a file to be
        // created; reading it otherwise would consume an argument that was
        // never passed.
        return (flags & (O_CREAT | O_TMPFILE)) ? va_arg(ap, mode_t) : 0;
    }
}

// Which of these libstdc++ ends up calling for a std::ifstream is a build
// detail of the library, not something the suites can pin down -- the
// filebuf implementation Debian ships goes through fopen, while others issue
// open directly -- and large-file support decides between the plain and the 64
// spelling. All four are cheap to define, so none of it has to be guessed at.
extern "C" int open(const char *path, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    mode_t mode = creation_mode(flags, ap);
    va_end(ap);

    return forward_open("open", path, flags, mode);
}

extern "C" int open64(const char *path, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    mode_t mode = creation_mode(flags, ap);
    va_end(ap);

    return forward_open("open64", path, flags, mode);
}

extern "C" FILE *fopen(const char *path, const char *mode)
{
    return forward_fopen("fopen", path, mode);
}

extern "C" FILE *fopen64(const char *path, const char *mode)
{
    return forward_fopen("fopen64", path, mode);
}
