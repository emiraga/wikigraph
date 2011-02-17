#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stack>
#include <cassert>
#include <climits>
#include <vector>
#include <string>
using namespace std;

#include "config.h"
#include "sqlparser.h"

void categories(const vector<string> &data)
{
	static int cnt = 0;
	cnt++;
	if(abs(atoi(data[0].c_str()) - cnt) < 5)
	{
		return;
	}
	printf("%d: ", cnt);
	for(int i=0;i<data.size();i++)
	{
		printf("%s ", data[i].c_str());
	}
	printf("\n");

}

int main(int argc, char *argv[])
{
	FILE *f = fopen(DUMPFILES "category.sql", "rb");
	SqlParser<categories> parser(f);
	parser.run();
	fclose(f);
	return 0;
}

