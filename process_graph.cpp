#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <cassert>
#include <time.h>
#include <string>
#include <algorithm>

#include <getopt.h>
#include <unistd.h>

using namespace std;

#include "hiredis/hiredis.h"
#include "config.h"

#define RESULTS_EXPIRE 60 // Seconds
#define MAXNODES 4250000 // maximum number of graph nodes

int node_begin_list[MAXNODES];
#define node_end_list(x) (node_begin_list[(x)+1])
int *edges;

int num_nodes;

void load_graph()
{
	FILE *f = fopen("graph_edges.bin", "rb");
	if(!f)
	{
		fprintf(stderr, "file not found, graph_edges.bin\n" );
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	off_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	edges = new int[fsize]; // May God have mercy on our souls
	if(!edges)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}
	size_t res = fread(edges, 1, fsize, f);
	if(res != fsize)
	{
		fprintf(stderr, "Read error res=%u\n", res);
		exit(1);
	}
	fclose(f);

	f = fopen("graph_nodes.bin", "rb");
	if(!f)
	{
		fprintf(stderr, "file not found, graph_nodes.bin\n" );
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	num_nodes = (fsize/4) - 2; //two are guard value at the beginning and end

	// Assure that we don't exceed maximum number of nodes
	assert(num_nodes < MAXNODES);

	res = fread(node_begin_list, 1, fsize, f);
	if(res != fsize)
	{
		fprintf(stderr, "Read error res=%u\n", res);
		exit(1);
	}
	fclose(f);
	printf("Graph lodaded with %d nodes.\n", num_nodes);
}

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
	for(int i=1;i<DIST_ARRAY;i++)
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

vector<int> scc_tarjan()
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
		if(nodeindex[k] == -1)
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
					if(nodeindex[dest] == -1)
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
	return scc_result;
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
				int node = atoi(job+1);
				if(node < 1 || node > num_nodes)
				{
					result = "{error:'Node out of range'}";
				}
				else
				{
					vector<int> cntdist = get_distances(node);
					result = "{count_dist:" + to_json(cntdist) + "}";
				}
#ifdef DEBUG
					usleep(1000 * 100);
#endif
			}
			break;
			case 'S':{ // Sizes of strongly connected components
				vector<int> components = scc_tarjan();
				result = "{component_sizes:" + to_json(components) + "}";
			}
			break;
			case 'P':{ // PING 'Are you still there?' (in GLaDOS voice)
				result = "{still_alive:'This was a triumph.'}";
			}
			break;
			case '.':{ // Job that does not produce any result
				no_result = true;
				// Used to test a crashing client
			}
			break;
			default:
				result = "{error:'Unknown command'}";
		}
		if(no_result)
			continue;

		/* Set results */
		reply = (redisReply*)redisCommand(c,"SETEX result:%s %d %b", job, RESULTS_EXPIRE, result.c_str(), result.size());
		freeReplyObject(reply);

		/* Announce the results to channel */
		reply = (redisReply*)redisCommand(c,"PUBLISH announce:%s %b", job, result.c_str(), result.size());
		freeReplyObject(reply);

		time_t t_end = clock();
		if(is_parent)
		{
			printf("Time to complete %.5lf: %s\n", double(t_end - t_start)/CLOCKS_PER_SEC, result.c_str());
		}
	}

	delete[] edges;
	return 0;
}

