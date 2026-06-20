//
// Created by Perfare on 2020/7/4.
//

#ifndef ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
#define ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H

void il2cpp_api_init(void *handle);

void il2cpp_dump(const char *outDir);

// generic file-driven engine: execute one command line, write result to outPath
void il2cpp_engine_cmd(const char *line, const char *dataDir, const char *outPath);

// embedded Lua scripting engine
void il2cpp_lua_init(const char *dataDir);
void il2cpp_lua_run_file(const char *path);
void il2cpp_lua_tick();

#endif //ZYGISK_IL2CPPDUMPER_IL2CPP_DUMP_H
