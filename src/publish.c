#include "blueHats.h"

#define GITHUB_USER "User"
#define GITHUB_REPO = "bluehats-registry"

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

char *createRelease(const char* name, const char* version, const char* token) {
	CURL* curl = curl_easy_init();
	Buffer response = {malloc(1), 0};

	char URL[256];
	snprintf(URL, sizeof(URL)),
		"https://api.github.com/repos/%s/%s/release", GITHUB_USER, GITHUB_REPO;
	
	char body[256];
	snprintf(body, sizeof(body), "{\"tag_name\":\"%s-%s\",\"name\":\"%s %s\}", name, version, name, version);

	char auth[256];
	snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, auth);
	headers = curl_slist_append(headers, "Content-Type: applications/json");
	headers = curl_slist_append(headers, "User-Agent: bluehats");

	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	char* key = strstr(response.data, "\"upload_url\"");
	char* start = strchr(key, '"') + 1;

	start = strchr(start, '"') + 1;

	char* end = strchr(start, '{');
	end--;

	int len = end - start;
	char* uploadURL = malloc(len + 1);

	strncpy(uploadURL, start, len);
	uploadURL[len] = '\0';

	free(response.data);
	return uploadURL;
}

void uploadAsset(const char* uploadURL, const char *tarballPath, const char* name, const char* version, const char* token) {
	CURL* curl = curl_easy_init();
	Buffer response = {malloc(1), 0};

	char URL[512];
	snprintf(URL, sizeof(URL), "%s?name=%s-%s.tar.gz", uploadURL, name, version);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, auth);
	headers = curl_slist_append(headers, "Content-Type: applications/json");
	headers = curl_slist_append(headers, "User-Agent: bluehats");

	FILE* f = fopen(tarballPath, "rb");
	fseek(f, 0, SEEK_END);

	long size = ftell(f);
	rewind(f);

	char* data = malloc(size);
	fread(data, 1, size, f);
	fclose(f);

	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_perform(curl);

	free(data);
	free(response.data);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}

void updateIndex(const char* name, const char* version, const char* token) {
	char URL[256];
	snprintf(URL, sizeof(URL), "https://api.github.com/repos/%s/%s/contents/index.json", GITHUB_USER, GITHUB_REPO);
	
	char auth[256];
	snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, auth);
	headers = curl_slist_append(headers, "User-Agent: bluehats");

	CURL* curl = curl_easy_init();
	Buffer response = {malloc(1), 0};
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	cJson* json = cJSON_Parse(response.data);
	const char* sha = cJSON_GetObjectItem(json, "sha")->valuestring;
	const char* content = cJSON_GetObjectItem(json, "content")->valuestring;
	free(response.data);

	char* indexString = base64_decode(content);
	cJSON* index = cJSON_Parse(indexString);
	free(indexString);

	cJSON* versions = cJSON_GetObjectItem(index, name);
	if (!versions) {
		versions = cJSON_CreateArray();
		cJSON_AddItemToObject(index, name, versions);
	}
	cJSON_AddItemToArray(versions, cJSON_CreateString(version));

	char* updated = cJSON_Print(index);
	char* encoded = base64_encode((unsigned char *)updated, strlen(updated));
	free(updated);
	cJSON_Delete(index);

	char body[4096];
	snprintf(body, sizeof(body), "{\"message\":\"publish %s@%s\",\"content\":\"%s\",\"sha\":\"%s\"}",
		name, version, encoded, sha);
	free(encoded);

	curl = curl_easy_init();
	response.data = malloc(1);
	response.size = 0;
	headers = curl_slist_append(NULL, auth);
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "User-Agent: bluehats");

	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_perform(curl);

	free(response.data);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	cJSON_Delete(json);
}

void publish(const char* registry, const char* token) {
	char name[64] = {0};
	char version[32] = {0};

	parsePackageInfo("bluehats.toml", name, version);
	printf("Publishing %s@%s\n", name, version);

	char temporaryPath[128];
	snprintf(temporaryPath, sizeof(temporaryPath), "/tmp/%s-%s.tar.gz", name, version);
	packTarball(".", temporaryPath);

	char* uploadURL = createRelease(name, version, token);
	uploadAsset(uploadURL, temporaryPath, name, version, token);
	free(uploadURL);

	updateIndex(name, version, token);
	printf("Done! %s@%s published successfully\n", name, version);
}