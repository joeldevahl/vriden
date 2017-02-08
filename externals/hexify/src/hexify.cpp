#include <cstdio>

int main(int argc, char **argv)
{
	if (argc != 2)
		return -1;

	FILE *input = fopen(argv[1], "rb");

	int i = 0;
	while(1)
	{
		int c = fgetc(input);
		if(feof(input))
			break;
		printf("0x%x, ", c);
		i = (i+1)&0xf;
		if(i == 0)
			printf("\n\t");
	}

	fclose(input);

	printf("\n");
	return 0;
}
