#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_LINE 512


int get_counter(char *sysfs_filename, char *target)
{
	FILE *fp;
	char *str = malloc(MAX_LINE);
	char *token_name;
	char *token_value;
	int i_token;
	int result = 0;
	fp = fopen(sysfs_filename, "r");
	
	if(!fp) {
		printf("File %s is not open: %s\n", sysfs_filename, strerror(errno));
		return -1;
	}
	while(fgets(str, MAX_LINE, fp)!=NULL)
		{
			str[strlen(str)-1] = '\0';
			token_name = strtok(str, "\t");
			token_value = strtok(NULL, "\t");
			i_token = atoi(token_value);
			if(strcmp(token_name, target)==0){
				result = i_token;
				break;
			}
		}
		free(str);
		fclose(fp);
		
		return result;
}
