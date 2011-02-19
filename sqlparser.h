/*
 * I love python, but it was too damn slow for this task. It turns out that 
 * handling utf-8 is easier than I thought. In this particular case,
 * no handling required.
 *
 * How so? All interesing ASCII characters for SQL are < 128 and all special
 * bytes in UTf-8 have value >= 128. I can copy utf-8 strings like a regular
 * single-byte array of characters.
 */

#include <cstdio>
#include <vector>
#include <string>
#include <cctype>
#include <cassert>
#include <zlib.h>

using namespace std;

const int BSIZE = 8*1024*1024;

class SqlParser {
protected:
	FILE *f;
	int index;
	char buffer[BSIZE];
	int buffer_size;
	off_t fsize;

	void (*stat_handler)(double);
	void (*data_handler)(const vector<string>&);

public:
	static SqlParser* open(const char *fname);

	SqlParser(FILE* file)
		: f(file),
		buffer_size(BSIZE),
		index(BSIZE),
		stat_handler(null_stat_h),
		data_handler(null_data_h)
	{
		fseeko(f, 0, SEEK_END);
		fsize = ftello(f);
		fseeko(f, 0, SEEK_SET);
	}
	void close()
	{
		fclose(f);
	}
	static void null_stat_h(double percent)
	{
	}
	void set_stat_handler(void (*handler)(double))
	{
		stat_handler = handler;
	}
	static void null_data_h(const vector<string>&)
	{
	}
	void set_data_handler( void (*handler)(const vector<string>&) )
	{
		data_handler = handler;
	}
	void run()
	{
		while(1)
		{
			char c = get();
			if(eof()) break;
			if(isspace(c) || c == ';') continue;

			if(c == '-' and peek() == '-') //--comment
				skip_after('\n');
			else if(c == '/' and peek() == '*')
			{
				get();
				skip_after('*','/'); /* comment */
			}
			else if(isalpha(c))
			{
				if(toupper(c) == 'I')
				{
					insert_command();
				}
				else
				{
					skip_command(); //don't care
				}
			}
			else
			{
				printf("Wrong beginning '%c' (%d)\n", c, c);
				return;
			}
		}
	}
private:
	void insert_command()
	{
		while(peek() != '(') get();
		while(1)
		{
			data_brackets();
			char c = get();
			if(c == ',')
				continue;
			if(c == ';')
				break;
			printf("Unexpected: '%c'\n", c);
			assert(false);
		}
	}
	void data_brackets()
	{
		assert(peek() == '(');
		get();
		vector<string> data;
		while(1)
		{
			char c = peek();
			if(c == '\'')
				data.push_back( get_quoted_string() );
			else if(isdigit(c) || c == '-')
				data.push_back( get_number() );
			else if(c == 'N')
			{
				assert(get() == 'N');
				assert(get() == 'U');
				assert(get() == 'L');
				assert(get() == 'L');
			}
			else
			{
				printf("Unexpected: '%c'\n", c);
				assert(false);
			}

			c = get();
			if(c == ',')
				continue;
			if(c == ')')
				break;
			printf("Unexpected: '%c'\n", c);
			assert(false);
		}
		data_handler(data);
	}
	void skip_command()
	{
		while(1)
		{
			char c = peek();
			if(c == ';')
			{
				get();
				break;
			}
			if(c == '-')
			{
				get();
				if(peek() == '-')
				{
					skip_after('\n');
				}
			}
			else if(c == '/')
			{
				get();
				if(peek() == '*')
				{
					get();
					skip_after('*','/');
				}
			}
			else if(c == '\'')
			{
				get_quoted_string();
			}
			else
			{
				assert(c != '"');
				get();
			}
		}
	}
	string get_quoted_string()
	{
		assert(peek() == '\'');
		get();
		string out;
		char c;
		while((c=get()) != '\'')
		{
			if(c == '\\')
				out += get();
			else
				out += c;
		}
		return out;
	}
	string get_number()
	{
		string out;
		if(peek() == '-')
			out += get();
		while(isdigit(peek()))
		{
			out += get();
		}
		if(peek() == '.')
		{
			out += get();
			while(isdigit(peek()))
			{
				out += get();
			}
		}
		if(peek() == 'e')
		{
			out += get();
			if(peek() == '-')
				out += get();
			while(isdigit(peek()))
				out += get();
		}
		return out;
	}
	bool eof() const
	{
		return buffer_size == 0;
	}
	char get()
	{
		if(++index >= buffer_size)
		{
			index = 0;
			read_buffer();
		}
		//printf("%c", buffer[index] );
		return buffer[index];
	}
	char peek()
	{
		if(index+1 >= buffer_size)
		{
			index = -1;
			read_buffer();
		}
		return buffer[index+1];
	}
	void skip_after(char c1)
	{
		while(get() != c1 && buffer_size);
	}
	void skip_after(char c1, char c2)
	{
		char prev = get(), current=get();
		while((prev != c1 || current != c2) && buffer_size)
		{
			prev = current;
			current = get();
		}
	}
	virtual void read_buffer()
	{
		buffer_size = fread(buffer, 1, BSIZE, f);
		stat_handler( 100.0 * ftello(f) / fsize );
	}
};

class SqlParserGz : public SqlParser {
	gzFile fd;
public:
	SqlParserGz(FILE *f)
	: SqlParser (f)
	{
		fd = gzdopen(fileno(f), "rb");
	}
	virtual void read_buffer()
	{
		SqlParser::buffer_size = gzread(fd, SqlParser::buffer, BSIZE);
		SqlParser::stat_handler( 100.0 * lseek(fileno(SqlParser::f), 0, SEEK_CUR) / SqlParser::fsize );
	}
};

SqlParser* SqlParser::open(const char *fname)
{
	FILE *f = fopen(fname, "rb");
	if(f)
		return new SqlParser(f);

	char fzname[100];
	snprintf(fzname, 99, "%s.gz", fname);
	f = fopen(fzname, "rb");
	if(f)
		return new SqlParserGz(f);

	perror("fopen");
	fprintf (stderr, "failed to open file '%s' and '%s'\n", fname, fzname) ;  
	exit(1);
}

