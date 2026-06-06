#include "blueHats.h"
#define REGISTRY "http://bluehats:8080"

void printUsage() {
	printf("Blue Hats - FOSP Package Manager\n\n");
	printf("usage:\n");
	printf("  bluehats init				create a new x.toml\n");
	printf("  bluehats install			install all dependencies\n");
	printf("  bluehats add <package>	add a dependency\n");
	printf("  bluehats publish <token>  publish a package to the registry\n");
}

void init() {
	FILE *f = fopen("bluehats.toml", "w");
	if (!f) {
		error("failed to create bluehats.toml\n", 1);
		return;
	}
	char name[64], version[32], author[64];

	printf("package name: ");
	fgets(name, sizeof(name), stdin);
	printf("version: ");
	fgets(version, sizeof(version), stdin);
	printf("author: ");
	fgets(author, sizeof(author), stdin);

	name[strcspn(name, "\n")] = 0;
	version[strcspn(version, "\n")] = 0;
	author[strcspn(author, "\n")] = 0;

	fprintf(f, "[package]\n");
	fprintf(f, "name	= \"%s\"\n", name);
	fprintf(f, "version	= \"%s\"\n", version);
	fprintf(f, "author	= \"%s\"\n", author);
	fprintf(f, "entry	= \"main\"\n\n");
	fprintf(f, "[dependencies]\n");

	fclose(f);
	printf("Created bluehats.toml\n");
}

int main(int argc, char** argv) {
	if (argc <= 1 || argc >= 4) {
		error("arguments not met <must be 2 or 3 strictly>", 1);
		return 1;
	}
	if (strcmp(argv[1], "init") == 0)
		init();
	else if (strcmp(argv[1], "install") == 0)
		install(REGISTRY);
	else if (strcmp(argv[1], "add") == 0) {
		if (argc < 3) {
			error("Usage: bluehats add <package>", 1);
			return 1;
		}
		add(REGISTRY, argv[2]);
	} else {
		fprintf(stderr, "Unknown command: %s\n", argv[1]);
		printUsage();
		return 1;
	}
	return 0;
}