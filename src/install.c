#include "blueHats.h"

int parseToml(const char* path, Dependency* deps, int* count) {
	FILE* f = fopen(path, "r");
	char line[256];
	int inDeps = 0;
	*count = 0;

	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = 0;
		if (strcmp(line, "[dependencies]") == 0) {
			inDeps = 1;
			continue;
		}
		if (line[0] == '[') {
			inDeps = 0;
			continue;
		}
		if (inDeps && strchr(line, '=')) {
			char *eq = strchr(line, '=');
			*eq = '\0';
			char *key = line;
			char *val = eq + 1;

			while (*key == ' ') key++;
			while (*val == ' ' || *val == '"') val++;
			val[strcspn(val, "\"")] = '\0';

			strncpy(deps[*count].name, key, 63);
			strncpy(deps[*count].version, val, 31);
			(*count)++;
        }
    }
	fclose(f);
	return 0;
}

void parseVersion(const char* v, int* major, int* minor, int* patch) {
	while (*v && !isdigit(*v))
		v++;
	sscanf(v, "%d.%d.%d", major, minor, patch);
}

int versionSatifies(const char* constraint, const char* candidate) {
	char	type = '^';
	int		firstMajor, firstMinor, firstPatch;
	int		secondMajor, secondMinor, secondPatch;

	if (constraint[0] == '^' || constraint[0] == '~')
		type = constraint[0];
	else if (isdigit(constraint[0]))
		type = '=';
	parseVersion(constraint, &firstMajor, &firstMinor, &firstPatch);
	parseVersion(candidate, &secondMajor, &secondMinor, &secondPatch);
	if (type == '=')
		return firstMajor == secondMajor && firstMinor == secondMinor && firstPatch == secondPatch;
	if (type == '^') {
		if (secondMajor != firstMajor)
			return 0;
		if (secondMinor < firstMinor)
			return 0;
		if (secondMinor == firstMinor && secondPatch < firstPatch)
			return 0;
		return 1;
	}
	if (type == '~') {
		if (secondMajor != firstMajor)
			return 0;
		if (secondMinor != firstMinor)
			return 0;
		if (secondPatch < firstPatch)
			return 0;
		return 1;
	}
	return 0;
}

int versionGT(const char* a, const char* b) {
	int		secondMajor, secondMinor, secondPatch;
	int		thirdMajor, thirdMinor, thirdPatch;
	parseVersion(a, &secondMajor, &secondMinor, &secondPatch);
	parseVersion(b, &thirdMajor, &thirdMinor, &thirdPatch);
	
	if (secondMajor != thirdMajor)
		return secondMajor > thirdMajor;
	if (secondMinor != thirdMinor)
		return secondMinor > thirdMinor;
	return secondPatch > thirdPatch;
}

void resolveVersions(Dependency* deps, int count, const char* indexData) {
	cJSON* index = cJSON_Parse(indexData);
	if (!index) {
		fprintf(stderr, "Failed to parse index.json\n");
		exit(1);
	}
	for (int i = 0; i < count; i++) {
		cJSON* versions = cJSON_GetObjectItem(index, deps[i].name);
		if (!versions) {
			fprintf(stderr, "Package not found %s\n", deps[i].name);
			exit(1);
		}
		char	best[32] = {0};
		int		found = 0;
		cJSON*	ver;
		
		cJSON_ArrayForEach(ver, versions) {
			const char* v = ver->valuestring;
			if (!versionSatifies(deps[i].version, v))
				continue;
			if (!found || versionGT(v, best)) {
				strncpy(best, v, 31);
				found = 1;
			}
		}
		if (!found) {
			fprintf(stderr, "No matching version for %s@%s\n", deps[i].name, deps[i].version);
			exit(1);
		}
		strncpy(deps[i].version, best, 31);
		printf("Resolved %s → %s\n", deps[i].name, deps[i].version);
	}
	cJSON_Delete(index);
}

size_t WriteCb(void* pointer, size_t size, size_t nmemb, Buffer* buffer) {
	size_t newSize = buffer->size + size * nmemb;

	buffer->data = realloc(buffer->data, newSize + 1);
	memcpy(buffer->data + buffer->size, pointer, size * nmemb);
	buffer->size = newSize;
	buffer->data[buffer->size] = '\0';
	return size * nmemb;
}

Buffer fetchURL(const char* url) {
	CURL *curl = curl_easy_init();
	Buffer buffer = {malloc(1), 0};
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return buffer;
}

void downloadFile(const char* url, const char* outPath) {
	Buffer buffer = fetchURL(url);
	FILE *f = fopen(outPath, "wb");
	fwrite(buffer.data, 1, buffer.size, f);
	fclose(f);
	free(buffer.data);
}

char* parseChecksum(const char* json) {
	char* key = strstr(json, "\"checksum\"");
	char* colon = strchr(key, ':');
	char* quote = strchr(colon, '"') + 1;
	char* end = strchr(quote, '"');
	int len = end - quote;
	char* result = malloc(len + 1);

	if (!key)
		return NULL;
	strncpy(result, quote, len);
	result[len] = '\0';
	return result;
}

char* sha256File(const char* path) {
	FILE *f = fopen(path, "rb");
	unsigned char buffer[4096];
	unsigned char hash[32];
	SHA256_CTX	ctx;
	SHA256_Init(&ctx);
	size_t	num;
	char	*hex = malloc(65);

	while ((num = fread(buffer, 1, sizeof(buffer), f)) > 0)
		SHA256_Update(&ctx, buffer, num);
	fclose(f);
	SHA256_Final(hash, &ctx);
	for (int i = 0; i < 32; i++)
		sprintf(hex + i * 2, "%02x", hash[i]);
	hex[64] = '\0';
	return hex;
}

void unpackTarball(const char *tarballPath, const char* outDir) {
	struct archive* a = archive_read_new();
	archive_read_support_format_tar(a);
	archive_read_support_filter_gzip(a);
	archive_read_open_filename(a, tarballPath, 10240);

	struct archive_entry* entry;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		char outPath[512];
		snprintf(outPath, sizeof(outPath), "%s/%s", outDir, archive_entry_pathname(entry));
		archive_entry_set_pathname(entry, outPath);

		struct archive* write = archive_write_disk_new();
		archive_write_disk_set_options(write, ARCHIVE_EXTRACT_TIME);
		archive_write_header(write, entry);

		const void *buffer;
		size_t		size;
		la_int64_t	offset;
		while (archive_read_data_block(a, &buffer, &size, &offset) == ARCHIVE_OK)
			archive_write_data_block(write, buffer, size, offset);
		archive_write_finish_entry(write);
		archive_write_free(write);
	}
	archive_read_free(a);
}

int parseLock(const char* path, Dependency *deps) {
	FILE*	f = fopen(path, "r");
	char	line[256];
	int		count = 0;
	Dependency *cur = NULL;

	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = 0;
		if (strcmp(line, "[[package]]") == 0) {
			cur = &deps[count++];
			continue;
		}
		if (!cur)
			continue;
		if (strncmp(line, "name = ", 7) == 0) {
			char* val = line + 7;
			while (*val == '"')
				val++;
			val[strcspn(val, "\"")] = '\0';
			strncpy(cur->name, val, 63);
		}
		if (strncmp(line, "version = ", 10) == 0) {
			char* val = line + 10;
			while (*val != '"')
				val++;
			val[strcspn(val, "\"")] = '\0';
			strncpy(cur->version, val, 31);
		}
	}
	fclose(f);
	return count;
}

void writeLock(Dependency* deps, int count) {
	FILE* f = fopen("bluehats.lock", "w");
	for (int i = 0; i < count; i++) {
		fprintf(f, "[[package]]\n");
		fprintf(f, "name = \"%s\"\n", deps[i].name);
		fprintf(f, "version = \"%s\"\n\n", deps[i].version);
	}
	fclose(f);
}

void install(const char* registry) {
	Dependency	deps[64];
	char	indexURL[256];
	int		count = 0;
	
	count = parseLock("bluehats.lock", deps);
	if (count < 0) {
		count = 0;
		parseToml("bluehats.toml", deps, &count);
		snprintf(indexURL, sizeof(indexURL), "%s/index.json", registry);
		Buffer	indexBuffer = fetchURL(indexURL);
		resolveVersions(deps, count, indexBuffer.data);
		free(indexBuffer.data);
		writeLock(deps, count);
	}
	for (int i = 0; i < count; i++) {
		printf("Installing %s@%s...\n", deps[i].name, deps[i].version);
		
		char URL[256], temporaryPath[128];
		snprintf(URL, sizeof(URL), "%s/packages/%s/%s.tar.gz", registry, deps[i].name, deps[i].version);
		snprintf(temporaryPath, sizeof(temporaryPath), "/tmp/%s-%s.tar.gz", deps[i].name, deps[i].version);
		downloadFile(URL, temporaryPath);

		char metaURL[256];
		snprintf(metaURL, sizeof(metaURL), "%s/packages/%s/%s.json", registry, deps[i].name, deps[i].version);
		Buffer meta = fetchURL(metaURL);
		char* expectedChecksum = parseChecksum(meta.data);
		free(meta.data);

		char* actual = sha256_file(temporaryPath);
		if (strcmp(actual, expectedChecksum) != 0) {
			fprintf(stderr, "Checksum mismatch for %s!\n", deps[i].name);
			free(actual);
			free(expectedChecksum);
			exit(1);
		}
		free(actual);
		free(expectedChecksum);

		char outDir[128];
		snprintf(outDir, sizeof(outDir), ".bluehats/%s", deps[i].name);
		unpackTarball(temporaryPath, outDir);
		printf(" ✓ %s@%s\n", deps[i].name, deps[i].version);
	}
}