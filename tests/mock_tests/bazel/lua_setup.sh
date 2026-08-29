# Module hook for the swss mock suites (sourced by //cc:test_runner.sh).
#
# Preloads bazel/swss_lua_path.cpp and points it at the lua scripts swsscommon
# ships, which the suites stage into runfiles instead of the /usr/share/swss the
# libswsscommon deb installs in Make's build container. Both are located rather
# than named by path so no canonical repo name is hardcoded here, the same way
# sysmgr's hook finds database_config.json.
#
# Exporting is enough: the runner launches the binary through `env`, which keeps
# the rest of the environment.
luascript=$(find "$RUNFILES" -name 'producer_state_table_apply_view.lua' 2>/dev/null | head -1)
if [ -n "$luascript" ]; then
    SWSS_LUA_DIR=$(cd "$(dirname "$luascript")" && pwd)
    export SWSS_LUA_DIR
fi

luashim=$(find "$RUNFILES" -name 'libswss_lua_path.so' 2>/dev/null | head -1)
if [ -n "$luashim" ]; then
    LD_PRELOAD="$(cd "$(dirname "$luashim")" && pwd)/libswss_lua_path.so${LD_PRELOAD:+:$LD_PRELOAD}"
    export LD_PRELOAD
fi
