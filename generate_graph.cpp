#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <stack>
#include <cassert>
#include <climits>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
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

void calc_hash(const char *str, int len)
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

class BufferedWriter
{
	static const int BWSIZE = 1024*1024;
	unsigned int buffer[BWSIZE];
	FILE *f;
	size_t pos;
	int bitpos;

public:
	BufferedWriter(const char *name)
		:pos(0), f(fopen(name, "wb")), bitpos(0)
	{
		if(!f)
		{
			perror("fopen");
			exit(1);
		}
	}
	~BufferedWriter()
	{
		close();
	}
	void flush()
	{
		size_t res = fwrite(buffer, 4, pos, f);
		if(res != pos)
		{
			perror("fopen");
			exit(1);
		}
		pos = 0;
	}
	void close()
	{
		if(bitpos)
		{
			pos++;
			bitpos=0;
		}

		if(f)
		{
			flush();
			fclose(f);
			f = NULL;
		}
	}
	void writeUint(unsigned int val)
	{
		assert(bitpos == 0);//don't mix bits with uints

		buffer[pos++] = val;
		if(pos == BWSIZE)
			flush();
	}
	void writeBit(bool val)
	{
		if(bitpos == 0)
			buffer[pos] = val;
		else
			buffer[pos] |= (val << bitpos);

		if(++bitpos == 32)
		{
			bitpos = 0;
			if(++pos == BWSIZE)
				flush();
		}
	}
};

/**************
 * Database
 *
 * I would normally use redis for this data, but I was primarily trying to
 * reduce memory usage of redis server by storing certain keys in an array
 * rather than the database.
 */
#ifdef USE_LOCAL_STORAGE

#define MAX_WIKI_PAGEID 30500000 // current value is 30480288

#pragma pack(push)
#pragma pack(1)     /* set alignment to 1 byte boundary */
struct wikistatus_t
{
	unsigned int type:3; // 0 - unknown, 1 - regular, 2 - redirect, 3 - resolved redirect
	unsigned int is_category:1; // 0 - article, 1 - category
};
#pragma pack(pop)

wikistatus_t wikistatus[MAX_WIKI_PAGEID+1]; 

int wikigraphId[MAX_WIKI_PAGEID+1];
//Total of 150MB of memory

#ifdef USE_REDIS_HASH
#error "Redis hashes currently don't work, is_category is not implemented"
int broken[-2]; 
#endif

#endif /* USE_LOCAL_STORAGE */

/**************
 * STAGE 1
 * Create graph nodes and store relevant data about nodes
 */
int graphNodes = 0;
int article_count, category_count, art_redirect_count, cat_redirect_count;

BufferedWriter *node_is_category;

/* SQL schema */
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

	int wikiId = atoi(data[page_id].c_str());
#ifdef USE_LOCAL_STORAGE
	assert(wikiId <= MAX_WIKI_PAGEID);
#endif

	const char *prefix;
	if(namespc == NS_MAIN) //Articles
	{
		prefix = "a:";
		if(is_redir)
			art_redirect_count++;
		else
		{
			article_count++;
		}
	}
	else if(namespc == NS_CATEGORY) //Categories
	{
		prefix = "c:";
		if(is_redir)
			cat_redirect_count++;
		else
		{
			category_count++;
		}
#ifdef USE_LOCAL_STORAGE
		wikistatus[wikiId].is_category = 1;
#else
		int not_implemented[-2];
#endif
	}
	else
		return; //other namespaces are not interesting

	const char *title = data[page_title].c_str();
	int title_len = data[page_title].size();
	calc_hash(title, title_len);

	int graphId;
	if(is_redir)
	{
		reply = (redisReply*)redisCommand(c, REDISCMD_SETNX_WD, prefix, hash_1, hash_2, wikiId );
#ifdef DEBUG
		printf("redirect %s%s  key=%s%s %s (wikiId=%d)\n", prefix, title, prefix, hash_1, hash_2, wikiId);
#endif
	}
	else
	{
		graphId = ++graphNodes; //generate from local storage; could have use INCR from redis
		node_is_category->writeBit(namespc == NS_CATEGORY);

		reply = (redisReply*)redisCommand(c, REDISCMD_SETNX_D , prefix, hash_1, hash_2, graphId );
#ifdef DEBUG
		printf("graph[%d] = %s%s  (wikiId=%d)\n", graphId, prefix, title, wikiId);
#endif
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
		wikistatus[wikiId].type = 2; //redirect
#endif
		reply = (redisReply*)redisCommand(c, "SET w:%d %s%s%s", wikiId, prefix, hash_1, hash_2);
		freeReplyObject(reply);
	}
	else
	{
#ifdef USE_LOCAL_STORAGE
		wikistatus[wikiId].type = 1; //regular
		wikigraphId[wikiId] = graphId;
#else
		reply = (redisReply*)redisCommand(c, "SET w:%d %d", wikiId, graphId );
		freeReplyObject(reply);
#endif
	}
	return;
}

bool stat_handler(double percent)
{
	printf("  %5.2lf%%\n", percent);
	return (percent < STOP_AFTER_PRECENT);
}

void main_stage1()
{
	SqlParser *parser = SqlParser::open(DUMPFILES"page.sql");
	parser->set_data_handler(page);
	parser->set_stat_handler(stat_handler);
	node_is_category = new BufferedWriter("graph_nodeiscat.bin");
	node_is_category->writeBit(0); //graphId starts from 1

	parser->run();

	node_is_category->close();
	delete node_is_category;

	parser->close();
	delete parser;
}

/**************
 * STAGE 2
 * Resolve redirects
 */

int cnt_still_redir;

bool stat_handler2(double percent)
{
	printf("  %5.2lf%%  unresolved=%d\n", percent, cnt_still_redir);
	return (percent < STOP_AFTER_PRECENT);
}

/* SQL schema */
#define rd_from 0 // int(8) unsigned NOT NULL DEFAULT '0',
#define rd_namespace 1// int(11) NOT NULL DEFAULT '0',
#define rd_title 2 // varbinary(255) NOT NULL DEFAULT '',

void redirect(const vector<string> &data)
{
	int wikiId = atoi(data[rd_from].c_str());
	// Check is redirect was resolved previously
#ifdef USE_LOCAL_STORAGE
	if(wikistatus[wikiId].type != 2)
		return;
#else
	reply = (redisReply*)redisCommand(c, "GET w:%d", wikiId );
	assert(reply->type != REDIS_REPLY_NIL);
	if(isdigit(reply->str[0]))
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
	calc_hash(title, title_len);

#ifdef DEBUG
	printf("(wikiId=%d) redirected to %s%s\n", wikiId, prefix, title);
#endif

	// Check target
	reply = (redisReply*)redisCommand(c, REDISCMD_GET, prefix, hash_1, hash_2);

	if(reply->type == REDIS_REPLY_NIL || !isdigit(reply->str[0]))
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
		// Need to get prefix:hash of a page based on its wikiId
		reply = (redisReply*)redisCommand(c, "GET w:%d", wikiId);
		assert(reply->type == REDIS_REPLY_STRING);
		assert(reply->str[1] == ':');
		if(reply->str[0] == 'a')
			prefix = "a:";
		else
			prefix = "c:";
		strncpy(hash_1, reply->str+2, keylen_1*2);
		strncpy(hash_2, reply->str+2+keylen_1*2, keylen_2*2);
		freeReplyObject(reply);

		reply = (redisReply*)redisCommand(c, REDISCMD_SET_D, prefix, hash_1, hash_2, graphId);
		freeReplyObject(reply);

#ifdef USE_LOCAL_STORAGE
		wikistatus[wikiId].type = 3; //resolved redirect
		wikigraphId[wikiId] = graphId;

		// This key is no longer needed
		reply = (redisReply*)redisCommand(c, "DEL w:%d", wikiId);
		freeReplyObject(reply);
#else
		reply = (redisReply*)redisCommand(c, "SET w:%d %dr", wikiId, graphId);
		// This extra 'r' at the end denotes that it is a resolved redirect
		freeReplyObject(reply);
#endif
	}
}

void main_stage2()
{
	// Can handle multiple redirects (up to 6 levels)
	const int REDIR_MAX = 6;
	int prev_count;
	int i=0;
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
		printf("Unresolved redirects: %d\n", cnt_still_redir);
		i++;
	}
	while(cnt_still_redir && cnt_still_redir != prev_count && i < REDIR_MAX);
}


/**************
 * STAGE 3
 * Handle pagelinks in order to generate directed graph between articles
 */
unsigned int num_edges, pagelink_rows;
FILE *graphout;
int skipped_catlinks, skipped_fromcat_links;

int cur_graphId;
vector<int> graphedges;

BufferedWriter *node_begin_list;
BufferedWriter *edge_list;

void write_graphId(int from_graphId)
{
	// Assert that pagelinks are exported with: mysqldump --order-by-primary
	assert(from_graphId >= cur_graphId);

	if(from_graphId != cur_graphId)
	{
		for(int i=cur_graphId; i< from_graphId; i++)
		{
			// For each node write the beginning of its edge list
			node_begin_list->writeUint(num_edges);
		}

		for(size_t i=0;i<graphedges.size();i++)
		{
			edge_list->writeUint(graphedges[i]);
		}
		num_edges += graphedges.size();
		graphedges.clear();

		cur_graphId = from_graphId;
	}
}

/* SQL schema */
#define pl_from 0 // int(8) unsigned NOT NULL DEFAULT '0',
#define pl_namespace 1 // int(11) NOT NULL DEFAULT '0',
#define pl_title 2 // varbinary(255) NOT NULL DEFAULT '',

void pagelink(const vector<string> &data)
{
	pagelink_rows++;
	int wikiId = atoi(data[pl_from].c_str());

	// Check if this page exists and is regular (not redirect)
#ifdef USE_LOCAL_STORAGE
	if(wikistatus[wikiId].type != 1) // If it is not regular page skip it
		return;
	if(wikistatus[wikiId].is_category)
	{
		skipped_fromcat_links++;
		return; //skip links from categories
	}
	int from_graphId = wikigraphId[wikiId];
#else
	reply = (redisReply*)redisCommand(c, "GET w:%d", wikiId );
	int replyLen = strlen(reply->str);
	assert(reply->type != REDIS_REPLY_NIL);//if there is no such page

	// Check if this is a regular page
	if( !isdigit(reply->str[0]) //redirects start with non-digit
	||  !isdigit(reply->str[replyLen-1]) ) //resolved redirects end with non-digit
	{
		freeReplyObject(reply);
		return;
	}
	int from_graphId = atoi(reply->str);
	freeReplyObject(reply);
#endif /* USE_LOCAL_STORAGE */

	write_graphId(from_graphId);

	int namespc = atoi(data[pl_namespace].c_str());

	const char *prefix;
	if(namespc == NS_MAIN) //Articles
		prefix = "a:";
	else if(namespc == NS_CATEGORY) //Categories
	{
		// Links to categories are ignored
		// I only focus on inter-article links and category inclusion links
		skipped_catlinks++;
		return;
	}
	else
		return; //other namespaces are not interesting

	const char *title = data[pl_title].c_str();
	int title_len = data[pl_title].size();
	calc_hash(title, title_len);

	reply = (redisReply*)redisCommand(c, REDISCMD_GET, prefix, hash_1, hash_2);
	if(reply->type != REDIS_REPLY_NIL && isdigit(reply->str[0]))
	{
		int to_graphId = atoi(reply->str);

#ifdef DEBUG
	printf("link from graphId=%d (wikiId=%d)  to=%s%s graphId=%d  type=%d\n",
			from_graphId, wikiId, prefix, title, to_graphId, wikistatus[to_graphId].type);
#endif
		graphedges.push_back(to_graphId);
	}
	freeReplyObject(reply);
}

void main_stage3()
{
	SqlParser *parser = SqlParser::open(DUMPFILES"pagelinks.sql");
	parser->set_data_handler(pagelink);
	parser->set_stat_handler(stat_handler);

	node_begin_list = new BufferedWriter("graph_nodes.bin");
	edge_list = new BufferedWriter("graph_edges.bin");

	parser->run();

	// Trick to force last nodes to be written
	while(cur_graphId <= graphNodes + 1)
		write_graphId(cur_graphId + 1);

	node_begin_list->close();
	delete node_begin_list;

	edge_list->close();
	delete edge_list;

	parser->close();
	delete parser;
}

/*****
 * 
 */

int main(int argc, char *argv[])
{
#ifdef REDIS_UNIXSOCKET
	c = redisConnectUnix(REDIS_UNIXSOCKET);
#else
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	c = redisConnectWithTimeout((char*)REDISHOST, REDISPORT, timeout);
#endif
	if (c->err) {
		printf("Connection error: %s\n", c->errstr);
		exit(1);
	}

	printf("Starting stage 1\n");
	main_stage1();

	reply = (redisReply*)redisCommand(c, "SET s:count:Articles %d", article_count); freeReplyObject(reply);
	reply = (redisReply*)redisCommand(c, "SET s:count:Article_redirects %d", art_redirect_count); freeReplyObject(reply);
	reply = (redisReply*)redisCommand(c, "SET s:count:Categories %d", category_count); freeReplyObject(reply);
	reply = (redisReply*)redisCommand(c, "SET s:count:Category_redirects %d", cat_redirect_count ); freeReplyObject(reply);
	reply = (redisReply*)redisCommand(c, "SET s:count:Graph_nodes %d", graphNodes ); freeReplyObject(reply);

	printf("Starting stage 2\n");
	main_stage2();

	printf("Starting stage 3\n");
	main_stage3();
	reply = (redisReply*)redisCommand(c, "SET s:count:Edges %d", num_edges ); freeReplyObject(reply);
	reply = (redisReply*)redisCommand(c, "SET s:count:Ignored_links_from_category %d", skipped_fromcat_links ); freeReplyObject(reply);
	reply = (redisReply*)redisCommand(c, "SET s:count:Ignored_links_to_category %d", skipped_catlinks ); freeReplyObject(reply);
	return 0;
}

