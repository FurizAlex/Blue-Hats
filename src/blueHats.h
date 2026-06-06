#ifndef BLUE_HATS_H
#define BLUE_HATS_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>
#include <openssl/sha.h>
#include "cjson/cJSON.h"

typedef struct {
	char name[64];
	char version[32];
}	Dependency;

typedef struct {
	char*	data;
	size_t	size;
}	Buffer;

int		parseToml(const char* path, Dependency* deps, int* count);
void 	parseVersion(const char* v, int* major, int* minor, int* patch);
int 	versionSatifies(const char* constraint, const char* candidate);
int 	versionGT(const char* a, const char* b);
void 	resolveVersions(Dependency* deps, int count, const char* indexData);
Buffer	fetchURL(const char* url);
void	downloadFile(const char* url, const char* outPath);
char*	sha256File(const char* path);
void 	unpackTarball(const char *tarballPath, const char* outDir);
int 	parseLock(const char* path, Dependency *deps);
void 	writeLock(Dependency* deps, int count);
size_t	WriteCb(void* pointer, size_t size, size_t nmemb, Buffer* buffer);

void 	packTarball(const char* sourceDir, const char* outPath);
void 	parsePackageInfo(const char* path, char* name, char* version);
char 	*sha256_file(const char* path);

char* 	latestVersion(cJSON* versions);
void 	appendDependency(const char* name, const char* version);

int		error(char* filename, int errorCode);
void 	printUsage();

void install(const char* registry);
void add(const char* registry, const char* package);
void publish(const char* registry, const char* token);
void init();

#endif