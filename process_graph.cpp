#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <cassert>
#include <time.h>
#include <string>
#include <algorithm>
#include <climits>

#include <getopt.h>
#include <unistd.h>

using namespace std;

#include "hiredis/hiredis.h"
#include "config.h"

#define RESULTS_EXPIRE 60 // Seconds
#define MAXNODES 4250000 // maximum number of graph nodes
typedef pair<int,int> pii;
typedef unsigned int uint;

int num_nodes;
uint node_begin_list[MAXNODES];
#define node_end_list(x) (node_begin_list[(x)+1])
uint node_iscat[MAXNODES/32];
#define IS_CATEGORY(x) (node_iscat[(x)>>5] & (1<<((x)&31)))

uint *edges;

int dist[MAXNODES];
int queue[MAXNODES];
const int DIST_ARRAY = 100; /* threshold to use array for distances */

vector<int> get_distances(int start)
{
	int dist_count[DIST_ARRAY] = {0};
	map<int, int> dist_count_m;

	memset(dist, -1, sizeof(dist));
	dist[start] = 0;
	queue[0] = start;
	dist_count[0]++;
	int queuesize = 1;
	for(int top=0;top<queuesize;top++)
	{
		int node = queue[top];
		for(int i=node_begin_list[node]; i<node_end_list(node); i++)
		{
			int target = edges[i];
			if(dist[target] == -1)
			{
				dist[target] = dist[node] + 1;
				queue[queuesize++] = target;

				if(dist[target] < DIST_ARRAY)
					dist_count[dist[target]]++;
				else
					dist_count_m[dist[target]]++;
			}
		}
	}
	vector<int> result;
	for(int i=0;i<DIST_ARRAY;i++)
	{
		if(!dist_count[i])
			return result;

		result.push_back(dist_count[i]);
	}
	for(map<int, int>::iterator i = dist_count_m.begin(); i!=dist_count_m.end(); i++)
	{
		result.push_back(i->second);
	}
	return result;
}

vector<pii> count_items(vector<int> &v)
{
	sort(v.begin(), v.end());
	v.push_back(INT_MAX);
	vector<pii> result;

	int cnt = 0;
	for(size_t i=0; i<v.size(); i++)
	{
		if(i && v[i-1] != v[i])
		{
			result.push_back(pii(v[i-1], cnt));
			cnt = 1;
		}
		else
		{
			cnt++;
		}
	}
	return result;
}

int* get_int_array(int size, int init)
{
	int* res = (int*)malloc(sizeof(int)*size);
	if(! res)
	{
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	memset(res, init, sizeof(int)*size);
	return res;
}

vector<pii> scc_tarjan()
{
	int tindex = 1;
	int top = -1;
	int top_scc = -1;

	int *lowindex = dist; //reuse some memory
	int *nodeindex = get_int_array(MAXNODES, -1);
	int *instack = get_int_array(MAXNODES, 0);
	int *stackI = get_int_array(MAXNODES, 0);
	int *stacknode = get_int_array(MAXNODES, 0);
	int *stack = queue; //reuse some memory

	vector<int> scc_result;

	for(int k=1; k<=num_nodes; k++)
	{
#ifdef DEBUG
		printf("processing node %d\n", k);
#endif
		// This node should not be previously visited and not be a category
		if(nodeindex[k] == -1 && !IS_CATEGORY(k))
		{
			if(k&255 == 0)printf("%d\n", k);
			// Push to execution stack
			stacknode[++top] = k;
			stackI[top] = -1;

			// Process stack
			while(top >= 0)
			{
				int node = stacknode[top];
				int &i = stackI[top];

				if(i == -1) // Uninitialized
				{
					// Initialize the node
					nodeindex[node] = tindex++;
					lowindex[node] = nodeindex[node];

					stack[++top_scc] = node;
					instack[node] = true;
					i=node_begin_list[node];
				}
				else if(i < node_end_list(node))
				{
					// New location of REF1 (see bellow)
					int dest = edges[i];
					lowindex[node] = min(lowindex[node], lowindex[dest]);
					i++;
				}
				for( ; i<node_end_list(node); i++)
				{
					int dest = edges[i];
					if(nodeindex[dest] == -1) // Not visited
					{
						// Push dest to execution stack
						stacknode[++top] = dest;
						stackI[top] = -1;
						break; // Original REF1 code
					}
					else if(instack[dest]) //dest belongs to this component
					{
						lowindex[node] = min(lowindex[node], lowindex[dest]);
					}
				}
				if(i == node_end_list(node))
				{
					if(lowindex[node] == nodeindex[node])
					{
						// We found one component
						int count = 0;
#ifdef DEBUG
						printf("Component:[");
#endif
						int sccnode;
						do
						{
							sccnode = stack[top_scc--];
							instack[sccnode] = false;
							count++;

#ifdef DEBUG
							printf(" %d", sccnode);
#endif
						}
						while(sccnode != node);
#ifdef DEBUG
						printf(" ]\n");
#endif
						scc_result.push_back(count);
					}
					// Remove node from execution stack
					top--;
				}
			}
			assert(top_scc == -1);
		}
	}
	//free(lowindex);
	free(nodeindex);
	free(instack);
	free(stackI);
	free(stacknode);
	//free(stack);

	return count_items(scc_result);
}

uint* file_to_array(const char *fname, uint *array)
{
	FILE *f = fopen(fname, "rb");
	if(!f)
	{
		fprintf(stderr, "file not found, %s\n", fname);
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	off_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	if(!array)
	{
		array = new uint[fsize];
		if(!array)
		{
			fprintf(stderr, "new/malloc failed\n");
			exit(1);
		}
	}

	size_t res = fread(array, 1, fsize, f);
	if(res != fsize)
	{
		fprintf(stderr, "Read error res=%u\n", res);
		exit(1);
	}
	fclose(f);
	return array;
}

void load_graph()
{
	edges = file_to_array("graph_edges.bin", NULL);

	file_to_array("graph_nodes.bin", node_begin_list);

	file_to_array("graph_nodeiscat.bin", node_iscat);
}

string to_json(const vector<int> &v)
{
	string msg = "[";
	for(int i=0; i<v.size(); i++)
	{
		if(i) msg += ",";
		char msgpart[20];
		sprintf(msgpart, "%d", v[i]);
		msg += string(msgpart);
	}
	msg += "]";
	return msg;
}

string to_json(const vector<pii> &v)
{
	string msg = "[";
	for(int i=0; i<v.size(); i++)
	{
		if(i) msg += ",";
		char msgpart[31];
		snprintf(msgpart, 30, "[%d,%d]", v[i].first, v[i].second);
		msg += string(msgpart);
	}
	msg += "]";
	return msg;
}

void print_help(char *prg)
{
	printf("Usage: %s [options]\n", prg);
	printf("\n");
	printf("-f N\tStart N background workers\n");
	printf("-r HOST\tName of the host of redis server, default:%s\n", REDISHOST);
	printf("-p PORT\tPort of redis server, default:%d\n", REDISPORT);
	printf("-h\tShow this help\n");
	printf("\n");
	printf("Visit https://github.com/emiraga/wikigraph for more info.\n");
}

int main(int argc, char *argv[])
{
	int fork_off = 0;

	char redis_host[51] = REDISHOST;
	int redis_port = REDISPORT;

	while(1)
	{
		int option = getopt(argc, argv, "f:r:p:h");
		if( option == -1 )
			break;
		switch(option)
		{
			case 'f':
				fork_off = atoi(optarg);
			break;
			case 'r':
				strncpy(redis_host, optarg, 50);
			break;
			case 'p':
				redis_port = atoi(optarg);
			break;
			case 'h':
				print_help(argv[0]);
				return 0;
			break;
			default:
				print_help(argv[0]);
				return 1;
		}
	}
	if(argc != optind)
	{
		print_help(argv[0]);
		return 1;
	}

	// Should be called before fork()'s to save memory (copy-on-write)
	load_graph();

	bool is_parent = true;
	if(fork_off)
	{
		for(int i=0; i<fork_off; i++)
		{
			if(!fork())
			{
				is_parent = false;
				break;
			}
		}
		if(is_parent)
		{
			printf("Started %d worker(s).\n", fork_off);
			return 0;
		}
	}

	redisContext *c;
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	c = redisConnectWithTimeout(redis_host, redis_port, timeout);
	if (c->err) {
		printf("Connection error: %s\n", c->errstr);
		exit(1);
	}

	redisReply *reply = (redisReply*)redisCommand(c,"GET s:count:Graph_nodes");
	assert(reply->type == REDIS_REPLY_STRING);

	num_nodes = atoi(reply->str);
	freeReplyObject(reply);

	if(is_parent)
	{
		printf("Number of nodes %d\n", num_nodes);
	}

	// Assure that we don't exceed maximum number of nodes
	assert(num_nodes < MAXNODES);

	while(1)
	{
		// Wait for a job on the queue
		redisReply *reply = (redisReply*)redisCommand(c,"BRPOPLPUSH queue:jobs queue:running 0");

		char job[101];
		strncpy(job, reply->str, 100 );
		job[100] = 0;
		freeReplyObject(reply);

		if(is_parent)
		{
			printf("Request: %s\n", job);
		}

		time_t t_start = clock();
		string result;
		bool no_result = false;
		switch(job[0])
		{
			case 'D':{ //count distances from node
#ifdef DEBUG
				// Simulate slow processing
				usleep(1000 * 100);
#endif
				int node = atoi(job+1);
				if(node < 1 || node > num_nodes)
				{
					result = "{\"error\":\"Node out of range\"}";
					break;
				}
				if(IS_CATEGORY(node))
				{
					result = "{\"error\":\"Node is category\"}";
					break;
				}
				vector<int> cntdist = get_distances(node);
				result = "{\"count_dist\":" + to_json(cntdist) + "}";
			}
			break;
			case 'S':{ // Sizes of strongly connected components
				vector<pii> components = scc_tarjan();
				result = "{\"components\":" + to_json(components) + "}";
			}
			break;
			case 'P':{ // PING 'Are you still there?' (in GLaDOS voice)
				result = "{\"still_alive\":\"This was a triumph.\"}";
			}
			break;
#ifdef DEBUG
			case '.':{ // Job that does not produce any result
				no_result = true;
				// Used to test a crashing client
			}
			break;
#endif
			default:
				result = "{\"error\":\"Unknown command\"}";
		}
		if(no_result)
			continue;

		/* Set results */
		reply = (redisReply*)redisCommand(c,"SETEX result:%s %d %b", job, RESULTS_EXPIRE, result.c_str(), result.size());
		freeReplyObject(reply);

		/* Announce the results to channel */
		reply = (redisReply*)redisCommand(c,"PUBLISH announce:%s %b", job, result.c_str(), result.size());
		freeReplyObject(reply);

		if(is_parent)
		{
			time_t t_end = clock();
			printf("Time to complete %.5lf: %s\n", double(t_end - t_start)/CLOCKS_PER_SEC, result.c_str());
		}
	}

	delete[] edges;
	return 0;
}

