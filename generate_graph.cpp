#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stack>
#include <cassert>
#include <climits>
#include <vector>
#include <string>
#include <cstring>
using namespace std;

#include "config.h"
#include "sqlparser.h"
#include "hiredis/hiredis.h"
#include "md5.h"

#ifdef USE_REDIS_HASH

#define REDISCMD_SETNX_WD "HSETNX %s%s %s w:%d"
#define REDISCMD_SETNX_D "HSETNX %s%s %s %d"
#define REDISCMD_SET_D "HSET %s%s %s %d"
#define REDISCMD_GET "HGET %s%s %s"

#else

#define REDISCMD_SETNX_WD "SETNX %s%s%s w:%d"
#define REDISCMD_SETNX_D "SETNX %s%s%s %d"
#define REDISCMD_SET_D "SET %s%s%s %d"
#define REDISCMD_GET "GET %s%s%s"

#endif /* USE_REDIS_HASH */

// MediaWiki namespaces for articles and categories
#define NS_MAIN 0
#define NS_CATEGORY 14

redisContext *c;
redisReply *reply;

const int keylen_1 = 3; //hexlen1 = keylen_1 * 2
const int keylen_2 = 3; //hexlen2 = keylen_2 * 2

char hash_1[keylen_1*2 + 1];
char hash_2[keylen_2*2 + 1];

char hexchars[16] = {'0', '1', '2', '3', '4', '5', '6',
	'7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

void calc_md5(const char *str, int len)
{
	md5_state_t state;
	md5_byte_t digest[16];

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)str, len);
	md5_finish(&state, digest);

	for (int di = 0; di < keylen_1; ++di)
	{
		hash_1[di * 2] = hexchars[digest[di]>>4];
		hash_1[di*2+1] = hexchars[digest[di]&15];
	}
	for (int di = 0; di < keylen_2; ++di)
	{
		hash_2[di * 2] = hexchars[digest[keylen_1 + di]>>4];
		hash_2[di*2+1] = hexchars[digest[keylen_1 + di]&15];
	}
}

/*****
 * Database
 *
 * I would normally use redis for this data, but I was primarily trying to
 * reduce memory usage of redis server by storing certain keys in array rather
 * than in database.
 */
#ifdef USE_LOCAL_STORAGE

#define MAX_WIKI_PAGEID 30500000 // current value is 30480288
char wikistatus[MAX_WIKI_PAGEID+1]; // 0 - unknown, 1 - regular, 2 - redirect, 3 - resolved redirect
int wikigraphId[MAX_WIKI_PAGEID+1];
// 150MB of memory

#endif /* USE_LOCAL_STORAGE */

/*****
 * STAGE 1
 */
int graphNodes = 0;
int article_count, category_count, art_redirect_count, cat_redirect_count;

#define page_id 0 // int(8) unsigned NOT NULL AUTO_INCREMENT,
#define page_namespace 1 // int(11) NOT NULL DEFAULT '0',
#define page_title 2 // varbinary(255) NOT NULL DEFAULT '',
#define page_restrictions 3 // tinyblob NOT NULL,
#define page_counter 4 // bigint(20) unsigned NOT NULL DEFAULT '0',
#define page_is_redirect 5 // tinyint(1) unsigned NOT NULL DEFAULT '0',
#define page_is_new 6 // tinyint(1) unsigned NOT NULL DEFAULT '0',
#define page_random 7 // double unsigned NOT NULL DEFAULT '0',
#define page_touched 8 // varbinary(14) NOT NULL DEFAULT '',
#define page_latest 9 // int(8) unsigned NOT NULL DEFAULT '0',
#define page_len 10 // int(8) unsigned NOT NULL DEFAULT '0',

void page(const vector<string> &data)
{
	bool is_redir = data[page_is_redirect][0] == '1';
	int namespc = atoi(data[page_namespace].c_str());

	const char *prefix;
	if(namespc == NS_MAIN) //Articles
	{
		prefix = "a:";
		if(is_redir)
			art_redirect_count++;
		else
			article_count++;
	}
	else if(namespc == NS_CATEGORY) //Categories
	{
		prefix = "c:";
		if(is_redir)
			cat_redirect_count++;
		else
			category_count++;
	}
	else
		return; //other namespaces are not interesting

	int wikiId = atoi(data[page_id].c_str());
	assert(wikiId <= MAX_WIKI_PAGEID);

	const char *title = data[page_title].c_str();
	int title_len = data[page_title].size();
	calc_md5(title, title_len);

	int graphId;
	if(is_redir)
	{
		reply = (redisReply*)redisCommand(c, REDISCMD_SETNX_WD, prefix, hash_1, hash_2, wikiId );
	}
	else
	{
		graphId = ++graphNodes; //generate from local storage; could have use INCR from redis

		reply = (redisReply*)redisCommand(c, REDISCMD_SETNX_D , prefix, hash_1, hash_2, graphId );
	}
	if(reply->integer != 1)
	{
		freeReplyObject(reply);
		printf("Collission found!\nHow to fix?\n 1. Empty the database,\n");
		printf(" 2. If that fails, try adding salt to md5, or increase keylen_1/keylen_2.\n");
		printf("Title:'%s' key1=%s key2=%s\n", title, hash_1, hash_2);

		//reply = (redisReply*)redisCommand(c, REDISCMD_GET, prefix, hash_1, hash_2 );
		//printf("Collision with: '%s'\n", reply->str);
		//freeReplyObject(reply);
		exit(1);
	}
	freeReplyObject(reply);

	if(is_redir)
	{
#ifdef USE_LOCAL_STORAGE
		wikistatus[wikiId] = 2; //redirect
#else
		reply = (redisReply*)redisCommand(c, "SET w:%d r", wikiId );
		freeReplyObject(reply);
#endif
	}
	else
	{
#ifdef USE_LOCAL_STORAGE
		wikistatus[wikiId] = 1; //regular
		wikigraphId[wikiId] = graphId;
#else
		reply = (redisReply*)redisCommand(c, "SET w:%d %d", wikiId, graphId );
		freeReplyObject(reply);
#endif
	}
	return;
}

void stat_handler(double percent)
{
	printf("  %5.2lf%%\n", percent);
}

void main_stage1()
{
	SqlParser *parser = SqlParser::open(DUMPFILES"page.sql");
	parser->set_data_handler(page);
	parser->set_stat_handler(stat_handler);

	parser->run();
	parser->close();
	delete parser;
}

/*****
 * STAGE 2
 */

int cnt_still_redir;

void stat_handler2(double percent)
{
	printf("  %5.2lf%%   unresolved=%d\n", percent, cnt_still_redir);
}

#define rd_from 0 // int(8) unsigned NOT NULL DEFAULT '0',
#define rd_namespace 1// int(11) NOT NULL DEFAULT '0',
#define rd_title 2 // varbinary(255) NOT NULL DEFAULT '',

void redirect(const vector<string> &data)
{
	int wikiId = atoi(data[rd_from].c_str());
	// Check is redirect was resolved previously
#ifdef USE_LOCAL_STORAGE
	if(wikistatus[wikiId] != 2)
		return;
#else
	reply = (redisReply*)redisCommand(c, "GET w:%d", wikiId );
	if(reply->type == REDIS_REPLY_NIL || isdigit(reply->str[0]))
	{
		// Redirect is already resolved
		freeReplyObject(reply);
		return;
	}
	freeReplyObject(reply);
#endif
	int namespc = atoi(data[rd_namespace].c_str());

	const char *prefix;
	if(namespc == NS_MAIN) //Articles
		prefix = "a:";
	else if(namespc == NS_CATEGORY) //Categories
		prefix = "c:";
	else
		return; //other namespaces are not interesting

	const char *title = data[rd_title].c_str();
	int title_len = data[rd_title].size();
	calc_md5(title, title_len);

	// Check target
	reply = (redisReply*)redisCommand(c, REDISCMD_GET, prefix, hash_1, hash_2);
	assert(reply->type != REDIS_REPLY_NIL);
	if(!isdigit(reply->str[0]))
	{
		freeReplyObject(reply);
		// Target is still a redirect
		cnt_still_redir++;
	}
	else
	{
		// Target is valid page (or resolved redirect)
		int graphId = atoi(reply->str);
		freeReplyObject(reply);
#ifdef USE_LOCAL_STORAGE
		wikistatus[wikiId] = 3; //resolved redirect
		wikigraphId[wikiId] = graphId;
#else
		reply = (redisReply*)redisCommand(c, "SET w:%d %d", wikiId, graphId);
		freeReplyObject(reply);
#endif
		reply = (redisReply*)redisCommand(c, REDISCMD_SET_D, prefix, hash_1, hash_2, graphId);
		freeReplyObject(reply);
	}
}

void main_stage2()
{
	int prev_count;
	do
	{
		prev_count = cnt_still_redir;

		cnt_still_redir = 0;
		SqlParser *parser = SqlParser::open(DUMPFILES"redirect.sql");
		parser->set_data_handler(redirect);
		parser->set_stat_handler(stat_handler2);

		parser->run();
		parser->close();
		delete parser;
	}
	while(cnt_still_redir && cnt_still_redir != prev_count );
}


/*****
 * STAGE 3
 */
int edges, links;
FILE *graphout;

void stat_handler3(double percent)
{
	printf("  %5.2lf%% edges=%d  links=%d\n", percent, edges, links);
}

int cur_graphId;
vector<int> graphedges;

void write_graphId(int from_graphId)
{
	assert(from_graphId >= cur_graphId);

	if(from_graphId != cur_graphId)
	{
		if(cur_graphId && graphedges.size())
		{
			int len = graphedges.size();
			fwrite(&cur_graphId, 1, sizeof(int), graphout);
			fwrite(&len, 1, sizeof(int), graphout);
			for(int i=0;i<len;i++)
			{
				fwrite(&graphedges[i], 1, sizeof(int), graphout);
			}
		}
		cur_graphId = from_graphId;
	}
}

#define pl_from 0 // int(8) unsigned NOT NULL DEFAULT '0',
#define pl_namespace 1 // int(11) NOT NULL DEFAULT '0',
#define pl_title 2 // varbinary(255) NOT NULL DEFAULT '',

void pagelink(const vector<string> &data)
{
	links++;
	int wikiId = atoi(data[pl_from].c_str());

	// Check if this page exists and is regular (not redirect)
#ifdef USE_LOCAL_STORAGE
	if(wikistatus[wikiId] != 1) // If it is not regular page skip it
		return;
	int from_graphId = wikigraphId[wikiId];
#else
	reply = (redisReply*)redisCommand(c, "GET w:%d", wikiId );
	//TODO: also check if this was not a redirect
	if(reply->type == REDIS_REPLY_NIL || !isdigit(reply->str[0]))
	{
		// It's not there or not a page
		freeReplyObject(reply);
		return;
	}
	int from_graphId = atoi(reply->str);
	freeReplyObject(reply);
#endif

	write_graphId(from_graphId);

	int namespc = atoi(data[pl_namespace].c_str());

	const char *prefix;
	if(namespc == NS_MAIN) //Articles
		prefix = "a:";
	else if(namespc == NS_CATEGORY) //Categories
		prefix = "c:";
	else
		return; //other namespaces are not interesting

	const char *title = data[pl_title].c_str();
	int title_len = data[pl_title].size();
	calc_md5(title, title_len);

	reply = (redisReply*)redisCommand(c, REDISCMD_GET, prefix, hash_1, hash_2);
	if(reply->type != REDIS_REPLY_NIL && isdigit(reply->str[0]))
	{
		int to_graphId = atoi(reply->str);
		edges++;
		graphedges.push_back(to_graphId);
	}
	freeReplyObject(reply);
}

void main_stage3()
{
	SqlParser *parser = SqlParser::open(DUMPFILES"pagelinks.sql");
	parser->set_data_handler(pagelink);
	parser->set_stat_handler(stat_handler3);

	graphout = fopen("graph.bin", "wb");
	parser->run();
	// This is an ugly trick to force that last node to be written
	write_graphId(INT_MAX);

	fclose(graphout);

	parser->close();
	delete parser;
}

/*****
 * 
 */

int main(int argc, char *argv[])
{
#ifdef REDISUNIX
	c = redisConnectUnix(REDISUNIX);
#else
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	c = redisConnectWithTimeout((char*)REDISHOST, REDISPORT, timeout);
#endif
	if (c->err) {
		printf("Connection error: %s\n", c->errstr);
		exit(1);
	}

	main_stage1();

	printf("Articles: %d\n", article_count);
	printf("Article redirects: %d\n", art_redirect_count);
	printf("Categories: %d\n", category_count);
	printf("Category redirects: %d\n", cat_redirect_count);

	main_stage2();

	main_stage3();
	return 0;
}

