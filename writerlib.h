#include <cstdio>
using namespace std;

// Write data to the file, only buffered
class BufferedWriter
{
	static const int BWSIZE = 1024*1024;// Buffer size
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
		if(f == NULL)
			return;

		// Flush written bits
		if(bitpos)
		{
			pos++;
			// Extra bits will be filled with zeroes
			bitpos=0;
		}
		flush(); // Buffers
		fclose(f);
		f = NULL;
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

class GraphWriter
{
private:
	BufferedWriter *node_begin_list;
	BufferedWriter *edge_list;
	int cur_graphId;
	vector<unsigned int> graph_edges;
	int num_edges, num_nodes;

public:
	GraphWriter(const char *node_fname, const char *edge_fname, int num_nodes)
		: num_edges(0), cur_graphId(0), num_nodes(num_nodes)
	{
		node_begin_list = new BufferedWriter(node_fname);
		edge_list = new BufferedWriter(edge_fname);
		node_begin_list->writeUint(0);
		node_begin_list->writeUint(0); //node 1 starts with zero's edge
	}
	void close()
	{
		if(node_begin_list)
		{
			// Trick to force last nodes to be written
			while(cur_graphId <= num_nodes + 1)
				write_node(cur_graphId + 1);

			node_begin_list->close();
			delete node_begin_list;
			node_begin_list = NULL;

			edge_list->close();
			delete edge_list;
			edge_list = NULL;
		}
	}
	~GraphWriter()
	{
		close();
	}

	void write_node(int from_graphId)
	{
		// Assert that tables are exported with: mysqldump --order-by-primary
		assert(from_graphId >= cur_graphId);

		if(from_graphId != cur_graphId)
		{
			for(size_t i=0;i<graph_edges.size();i++)
			{
				edge_list->writeUint(graph_edges[i]);
				printf("WE: %u\n", graph_edges[i]);
			}
			num_edges += graph_edges.size();
			graph_edges.clear();

			node_begin_list->writeUint(num_edges);
			cur_graphId++;
		}
	}

	void write_edge(unsigned int to_graphId)
	{
		graph_edges.push_back(to_graphId);
	}

private:
	GraphWriter(const GraphWriter&);
	GraphWriter operator=(const GraphWriter&);
};

