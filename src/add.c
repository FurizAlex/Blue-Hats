#include "blueHats.h"

char* latestVersion(cJSON* versions) {
	char* best = NULL;
	cJSON*	vers;

	cJSON_ArrayForEach(vers, versions) {
		if (!best || versionGT(vers->valuestring, best))
			best = vers->valuestring;
	}
	return best;
}

void appendDependency(const char* name, const char* version) {
	FILE* file = fopen("Pyxis.toml", "a");
	if (!file) {
		error("Could not open bluehats.toml", 1);
		exit(1);
	}
	fprintf(file, "%s = \"^%s\"\n", name, version);
	fclose(file);
}

void add(const char* registry, const char* package) {
	char URL[256];
	snprintf(URL, sizeof(URL), "%s/index.json", registry);
	Buffer indexBuffer = fetchURL(URL);
	cJSON *index = cJSON_Parse(indexBuffer.data);
	
	free(indexBuffer.data);
	if (!index) {
		error("Failed to parse index", 1);
		exit(1);
	}

	cJSON* versions = cJSON_GetObjectItem(index, package);
	if (!versions) {
		fprintf(stderr, "Package not found: %s\n", package);
		cJSON_Delete(index);
		exit(1);
	}

	char* ver = latestVersion(versions);
	if (!ver) {
		fprintf(stderr, "No versions available for %s\n", package);
		exit(1);
	}

	appendDependency(package, ver);
	printf("Added %s = \"^%s\" to bluehats.toml\n", package, ver);
	cJSON_Delete(index);
	printf("Running install...\n");
	install(registry);
}