#include "blueHats.h"

void packTarball(const char* sourceDir, const char* outPath) {
	struct archive* a = archive_write_new();
	archive_write_add_filter_gzip(a);
	archive_write_set_format_pax_restricted(a);
	archive_write_open_filename(a, outPath);

	DIR* dir = opendir(sourceDir);
	struct dirent* entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (strcmp(entry->d_name, ".bluehats") == 0)
			continue;
		if (strcmp(entry->d_name, ".git") == 0)
			continue;
		if (strcmp(entry->d_name, "node_modules") == 0)
			continue;
		if (strcmp(entry->d_name, "__pycache__") == 0)
			continue;

		char fullPath[512];
		snprintf(fullPath, sizeof(fullPath), "%s/%s", sourceDir, entry->d_name);
		
		struct stat st;
		stat(fullPath, &st);

		if (!S_ISREG(st.st_mode))
			continue;

		struct archive_entry* ae = archive_entry_new();
		archive_entry_set_pathname(ae, entry->d_name);
		archive_entry_set_size(ae, st.st_size);
		archive_entry_set_filetype(ae, AE_IFREG);
		archive_entry_set_perm(ae, 0644);
		archive_write_header(a, ae);

		FILE* f = fopen(fullPath, "rb");
		char	buffer[8192];
		size_t	num;

		while ((num = fread(buffer, 1, sizeof(buffer), f)) > 0)
			archive_write_data(a, buffer, num);
		fclose(f);
		archive_entry_free(ae);
	}
	closedir(dir);
	archive_write_close(a);
	archive_write_free(a);
}

void parsePackageInfo(const char* path, char* name, char* version) {
	FILE* f = fopen(path, "r");
	char line[256];
	int inPackage = 0;

	if (!f) {
		error("Toml not found", 1);
		exit(1);
	}
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = 0;

		if (strcmp(line, "[package]") == 0) {
			inPackage = 1;
			continue;
		}
		if (line[0] == '[') {
			inPackage = 0;
			continue;
		}
		if (!inPackage)
			continue;

		char* equal = strchr(line, '=');
		if (!equal)
			continue;
		*equal = '\0';

		char* key = line;
		char* val = equal + 1;
		while (*key == ' ')
			key++;
		while (*key == ' ' || *val == '"')
			val++;
		val[strcspn(val, "\"")] = '\0';

		if (strcmp(key, "name") == 0)
			strncpy(name, val, 63);
		if (strcmp(key, "version") == 0)
			strncpy(version, val, 31);
	}
	fclose(f);
}

char *sha256_file(const char* path) {
	FILE* f = fopen(path, "rb");
	unsigned char buffer[4096];
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	size_t num;

	while ((num = fread(buffer, 1, sizeof(buffer), f)) > 0)
		SHA256_Update(&ctx, buffer, num);
	fclose(f);

	unsigned char hash[32];
	SHA256_Final(hash, &ctx);

	char* hex = malloc(65);
	for (int i = 0; i < 32; i++)
		sprintf(hex + i * 2, "%02x", hash[i]);
	hex[64] = '\0';
	return hex;
}

void publish(const char* registry, const char* token) {
	char name[64] = {0};
	char version[32] = {0};

	parsePackageInfo("bluehats.toml", name, version);
	printf("Publishing %s@%s\n", name, version);

	char temporaryPath[128];
	snprintf(temporaryPath, sizeof(temporaryPath), "/tmp/%s-%s.tar.gz", name, version);
	packTarball(".", temporaryPath);

	char *checksum = sha256_file(temporaryPath);
	printf("Checksum: %s\n", checksum);

	char URL[256];
	snprintf(URL, sizeof(URL), "%s/publish", registry);

	char authHeader[128];
	snprintf(authHeader, sizeof(authHeader), "Authorization: Token Bearer %s", token);

	CURL* curl = curl_easy_init();
	struct curl_slist* headers = curl_slist_append(NULL, authHeader);
	
	curl_mime* form = curl_mime_init(curl);
	curl_mimepart* part = curl_mime_addpart(form);
	curl_mime_name(part, "name");
	curl_mime_data(part, name, CURL_ZERO_TERMINATED);

	part = curl_mime_addpart(form);
	curl_mime_name(part, "version");
	curl_mime_data(part, version, CURL_ZERO_TERMINATED);

	part = curl_mime_addpart(form);
	curl_mime_name(part, "tarball");
	curl_mime_filedata(part, temporaryPath);

	Buffer response = {malloc(1), 0};
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_perform(curl);

	if (strstr(response.data, "\"ok\":true"))
		printf("Published %s@%s successfully\n", name, version);
	else
		fprintf(stderr, "Publish failed: %s\n", response.data);
	free(response.data);
	free(checksum);
	curl_mime_free(form);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}