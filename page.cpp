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

FILE *out;
redisContext *c;
redisReply *reply;

int article_count, category_count, aredirect_count, credirect_count;

const int keylen_1 = 3; //hexlen1 = keylen_1 * 2
const int keylen_2 = 3; //hexlen2 = keylen_2 * 2

char hex_output_1[keylen_1*2 + 1];
char hex_output_2[keylen_2*2 + 1];

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
		hex_output_1[di * 2] = hexchars[digest[di]>>4];
		hex_output_1[di*2+1] = hexchars[digest[di]&15];
	}
	for (int di = 0; di < keylen_2; ++di)
	{
		hex_output_2[di * 2] = hexchars[digest[keylen_1 + di]>>4];
		hex_output_2[di*2+1] = hexchars[digest[keylen_1 + di]&15];
	}
}

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

#define NS_MAIN 0
#define NS_CATEGORY 14

void page(const vector<string> &data)
{
	int namespc = atoi(data[page_namespace].c_str());
	bool is_redir = data[page_is_redirect][0] == '1';

	const char *prefix;
	if(namespc == NS_MAIN)
	{
		prefix = "p";
		if(is_redir)
			aredirect_count++;
		else
			article_count++;
	}
	else if(namespc == NS_CATEGORY)
	{
		prefix = "c";
		if(is_redir)
			credirect_count++;
		else
			category_count++;
	}
	else
		return;

	const char *title = data[page_title].c_str();
	int title_len = data[page_title].size();
	calc_md5(title, title_len);

	reply = (redisReply*)redisCommand(c,"HSETNX %s%s %s %b", prefix, hex_output_1, hex_output_2, title, title_len );
	if(reply->integer != 1)
	{
		freeReplyObject(reply);
		printf("Collission found!\nHow to fix? 1. Empty the database, 2. Try increasing keylen_1 or keylen_2.\n");
		printf("Title:'%s' key1=%s key2=%s\n", title, hex_output_1, hex_output_2);

		reply = (redisReply*)redisCommand(c,"HGET %s%s %s", prefix, hex_output_1, hex_output_2 );
		printf("Collision with: '%s'\n", reply->str);
		freeReplyObject(reply);
		exit(1);
	}
	freeReplyObject(reply);
	return;
}

void stat_handler(double percent)
{
	printf(" %5.2lf\n", percent);
}

int main(int argc, char *argv[])
{
#ifdef REDISUNIX
	c = redisConnectUnix(REDISUNIX);
#endif
	if (c->err) {
		printf("Connection error: %s\n", c->errstr);
		exit(1);
	}

	SqlParser *parser = SqlParser::open(DUMPFILES"page.sql");
	parser->set_data_handler(page);
	parser->set_stat_handler(stat_handler);

	parser->run();
	parser->close();
	delete parser;

	printf("Articles: %d\n", article_count);
	printf("Article redirects: %d\n", aredirect_count);
	printf("Categories: %d\n", category_count);
	printf("Category redirects: %d\n", credirect_count);
	return 0;
}

/*

    reply = (redisReply*)redisCommand(c,"HEXISTS %s%s %s", prefix, hex_output1, hex_output2);
	int exists = reply->integer;
	freeReplyObject(reply);
	if(exists)
	{
		printf("'%s' %s %s\n", title, hex_output1, hex_output2);

		reply = (redisReply*)redisCommand(c,"HGET %s%s %s", prefix, hex_output1, hex_output2 );
		printf("HGET (binary API): '%s'\n", reply->str);
		freeReplyObject(reply);
		exit(1);
		return;
	}

int main(void) {
    unsigned int j;

    reply = redisCommand(c,"PING");
    printf("PING: %s\n", reply->str);
    freeReplyObject(reply);

    reply = redisCommand(c,"SET %s %s", "foo", "hello world");
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    reply = redisCommand(c,"SET %b %b", "bar", 3, "hello", 5);
    printf("SET (binary API): %s\n", reply->str);
    freeReplyObject(reply);

    reply = redisCommand(c,"GET foo");
    printf("GET foo: %s\n", reply->str);
    freeReplyObject(reply);

    reply = redisCommand(c,"INCR counter");
    printf("INCR counter: %lld\n", reply->integer);
    freeReplyObject(reply);
    reply = redisCommand(c,"INCR counter");
    printf("INCR counter: %lld\n", reply->integer);
    freeReplyObject(reply);

    reply = redisCommand(c,"DEL mylist");
    freeReplyObject(reply);
    for (j = 0; j < 10; j++) {
        char buf[64];

        snprintf(buf,64,"%d",j);
        reply = redisCommand(c,"LPUSH mylist element-%s", buf);
        freeReplyObject(reply);
    }

    reply = redisCommand(c,"LRANGE mylist 0 -1");
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (j = 0; j < reply->elements; j++) {
            printf("%u) %s\n", j, reply->element[j]->str);
        }
    }
    freeReplyObject(reply);

    return 0;
}
*/
