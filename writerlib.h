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
	vector<int> graphedges;

public:
	GraphWriter()
	{
		node_begin_list = new BufferedWriter("graph_nodes.bin");
		edge_list = new BufferedWriter("graph_edges.bin");
	}
	void close()
	{

		// Trick to force last nodes to be written
		while(cur_graphId <= graphNodes + 1)
			write_graphId(cur_graphId + 1);

		if(node_begin_list)
		{
			node_begin_list->close();
			delete node_begin_list;
			node_begin_list = NULL;
		}

		if(edge_list)
		{
			edge_list->close();
			delete edge_list;
			edge_list = NULL;
		}
	}
	~GraphWriter()
	{
		close();
	}

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
		graphedges.push_back(to_graphId);

private:
	GraphWriter(const GraphWriter&);
	GraphWriter operator=(const GraphWriter&);
};

