#include <zlib.h> 
#include <cstdio>
#include <vector>
#include <string>
#include <errno.h>
#include <cstdlib>
using namespace std;

#include "config.h"
#include "sqlparser.h"

void pagelinks(const vector<string> &data)
{
//	static int cnt = 0;
//	cnt++;
//	printf("%d: ", cnt);
//	for(int i=0; i<data.size(); i++)
//	{
//		printf("%s ", data[i].c_str());
//	}
//	printf("\n");

	static int oldid = -1;
	int id = atoi(data[0].c_str());
	if(id < oldid)
	{
		printf("id=%d oldid=%d\n", id, oldid);
		exit(0);
	}
}

int main(int argc, char *argv[])
{
	const char *fname = DUMPFILES "pagelinks.sql.gz";
	FILE *f = fopen(fname, "rb");
	if(f == NULL)
	{
		perror("fopen");
		fprintf (stderr, "failed to open file '%s'\n", fname) ;  
		return 1 ;  
	} 
	SqlParserGz parser(f);
	parser.set_data_handler(pagelinks);
	parser.run();
	fclose(f);
	return 0; 
}

