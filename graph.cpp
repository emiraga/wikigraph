#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <cassert>
#include "hiredis/hiredis.h"
#include <time.h>
#include <string>
#include <algorithm>

using namespace std; // sue me

typedef pair<int, int> pii;

#define MAXNODES 9000000 /* maximum number of articles */

const int DIST_ARRAY = 100; /* threshold for which distances to use array */

int list[MAXNODES][2];
int dist[MAXNODES];
int queue[MAXNODES];
int *edges;

void load_graph()
{
	FILE *f = fopen("graph_pages.bin", "rb");
	if(!f)
	{
		fprintf(stderr, "file not found, graph_pages.bin\n");
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	printf("File size %ld\n", fsize);

	edges = (int*)malloc(fsize);
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

	int num_nodes = 0;

	fsize /= sizeof(int);
	for(int i=0; i<fsize; )
	{
		num_nodes++;
		int node = edges[i];
		int listsize = edges[i+1];
		list[node][0] = i + 2; //start of list
		i += listsize + 2;
		list[node][1] = i; //end of list
	}
	fclose(f);
	printf("Graph loaded with %d nodes.\n", num_nodes);
}

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
		for(int i=list[node][0]; i<list[node][1]; i++)
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
		if(dist_count[i])
			result.push_back(dist_count[i]);
	}
	for(map<int, int>::iterator i = dist_count_m.begin(); i!=dist_count_m.end(); i++)
	{
		result.push_back(i->second);
	}
	return result;
}

/*

Recursive version of tarjan's SCC, not used since it will overflow the stack on wiki-sized graph

int tindex;

int nodeindex[MAXNODES];
int stack[MAXNODES];

int lowindex[MAXNODES];
bool instack[MAXNODES];
vector<int> scc_result;
int top;

void scc_tarjan(int node)
{
	lowindex[node] = nodeindex[node] = tindex++;
	stack[top++] = node;
	instack[node] = true;
	for(int i=list[node][0]; i<list[node][1]; i++)
	{
		int dest = edges[i];
		if(nodeindex[dest] == -1)
		{
			scc_tarjan(dest);
			lowindex[node] = min(lowindex[node], lowindex[dest]);
		}
		else if(instack[dest])
		{
			lowindex[node] = min(lowindex[node], lowindex[dest]);
		}
	}
	if(lowindex[node] == nodeindex[node])
	{
		int count = 0;
		int sccnode;
		do
		{
			sccnode = stack[--top];
			instack[sccnode] = false;
			count++;
		}
		while(sccnode != node);
		scc_result.push_back(count);
	}
}

void scc()
{
	memset(nodeindex, -1, sizeof(nodeindex));
	memset(instack, 0, sizeof(instack));
	tindex = 1;
	top = 0;
	scc_result.clear();

	for(int i=0;i<MAXNODES;i++)
	{
		if(nodeindex[i] == -1 && list[i][0])
		{
			scc_tarjan(i);
		}
	}
}

*/

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

	printf("Begin alloc\n");
	int *lowindex = dist;
	int *nodeindex = get_int_array(MAXNODES, -1);
	int *instack = get_int_array(MAXNODES, 0);
	int *stackindex = get_int_array(MAXNODES, 0);
	int *stacknode = get_int_array(MAXNODES, 0);
	int *stack = queue;
	printf("End alloc\n");

	vector<int> scc_result;

	for(int k=0;k<MAXNODES;k++)
	{
		if(nodeindex[k] == -1 && list[k][0])
		{
			if(k&255 == 0)printf("%d\n", k);
			// Push to execution stack
			stacknode[++top] = k;
			stackindex[top] = 0;

			// Process stack
			while(top >= 0)
			{
				int node = stacknode[top];
				int &i = stackindex[top];
				if(i == 0)
				{
					// Initialize the node
					nodeindex[node] = lowindex[node] = tindex++;
					stack[++top_scc] = node;
					instack[node] = true;
					i=list[node][0];
				}
				else if(i < list[node][1])
				{
					// New location of REF1 (see bellow)
					int dest = edges[i];
					lowindex[node] = min(lowindex[node], lowindex[dest]);
					i++;
				}
				for( ; i<list[node][1]; i++)
				{
					int dest = edges[i];
					if(nodeindex[dest] == -1)
					{
						// Push dest to execution stack
						stacknode[++top] = dest;
						stackindex[top] = 0;
						break;
						// Original REF1 code
					}
					else if(instack[dest]) //dest belongs to this component
					{
						lowindex[node] = min(lowindex[node], lowindex[dest]);
					}
				}
				if(i == list[node][1])
				{
					if(lowindex[node] == nodeindex[node])
					{
						// We found one component
						int count = 0;
						int sccnode;
						do
						{
							sccnode = stack[top_scc--];
							instack[sccnode] = false;
							count++;
						}
						while(sccnode != node);
						scc_result.push_back(count);
					}
					// Remove node from execution stack
					top--;
				}
			}
		}
	}
	//free(lowindex);
	free(nodeindex);
	free(instack);
	free(stackindex);
    free(stacknode);
	//free(stack);
}

void load_sample_graph()
{
	edges = (int*)malloc(50*4);
	if(!edges)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}
	memset(edges,0,50*4);

	list[1][0] = 1;
	list[1][1] = 2;

	list[2][0] = 2;
	list[2][1] = 3;

	edges[1] = 2;
	edges[2] = 1;
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

int main(void)
{
	load_graph();
	//load_sample_graph();

	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	redisContext *c = redisConnectWithTimeout((char*)"127.0.0.2", 6379, timeout);
	if (c->err) {
		printf("Connection error: %s\n", c->errstr);
		exit(1);
	}

	while(1)
	{
		redisReply *reply = (redisReply*)redisCommand(c,"BRPOPLPUSH q:jobs q:running 0");
		printf("Request: %s\n", reply->str);
		char job[101]; job[100] = 0;
		strncpy(job, reply->str, 100 );
		freeReplyObject(reply);

		time_t t_start = clock();
		string result;
		switch(job[0])
		{
			case 'A':{
				int node = atoi(job+1);
				vector<int> cntdist = get_distances(node);
				result = "{count_dist:" + to_json(cntdist) + "}";
			}
			break;
			case 'S':{
				vector<int> components = scc_tarjan();
				sort(components.rbegin(), components.rend());
				result = "{component_sizes:" + to_json(components) + "}";
			}
			break;
			default:
				fprintf(stderr, "Unknown command '%c'\n", reply->str[0]);
				result = "{error:'Unknown command'}";
		}

		/* Set results */
		reply = (redisReply*)redisCommand(c,"SET result:%s %b", job, result.c_str(), result.size());
		printf("SET (binary API): %s\n", reply->str);
		freeReplyObject(reply);

		/* Announce the results to channel */
		reply = (redisReply*)redisCommand(c,"PUBLISH announce:%s %b", job, result.c_str(), result.size());
		printf("SET (binary API): %lld\n", reply->integer);
		freeReplyObject(reply);

		time_t t_end = clock();
		printf("Time to complete %.5lf\n", double(t_end - t_start)/CLOCKS_PER_SEC);
	}

	return 0;
}

