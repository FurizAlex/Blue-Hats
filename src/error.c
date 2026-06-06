#include "blueHats.h"

int error(char* filename, int errorCode) {
	if (!filename)
		printf("No File Found");
	fprintf(stderr, "Blue Hats Error: %s\n", filename);
	return errorCode;
}