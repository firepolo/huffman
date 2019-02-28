#include <iostream>
#include <sstream>
#include <fstream>
#include <functional>
#include <queue>
#include <unordered_map>
#include <crtdbg.h>

class ibstream
{
public:
	ibstream(std::istream &stream) : in(stream), shift(7), cursor(0)
	{
		in.read(&buf, 1);
	}

	bool bit()
	{
		bool b = (buf >> shift) & 1;
		if (--shift < 0)
		{
			shift = 7;
			in.read(&buf, 1);
		}
		return b;
	}

	void read(char *p, char size)
	{
		for (char s = 0, *c; s < size; ++s)
		{
			c = p + (s >> 3);
			*c = (*c << 1) | bit();
		}
	}

private:
	std::istream &in;

	char buf;
	char shift;
	unsigned int cursor;
};

class obstream
{
public:
	obstream(std::ostream &stream) : out(stream), buf(0), shift(0), cursor(0)
	{
	}

	void flush()
	{
		write(&buf, 8 - shift);
		cursor = 0;
		shift = 0;
		out.clear(0);
		out.seekp(0);
	}

	void seek(unsigned int pos)
	{
		out.seekp(pos);
	}

	inline unsigned int tell()
	{
		return cursor;
	}

	void bit(bool bit)
	{
		buf = (buf << 1) | bit;
		++cursor;
		if (++shift < 8) return;
		out << buf;
		shift = buf = 0;
	}

	void write(char *p, char s)
	{
		for (char i = 0; s >= 0; --s) bit((*(p + (i++ >> 3)) >> (s & 7)) & 1);
	}

private:
	std::ostream &out;

	char buf;
	char shift;
	unsigned int cursor;
};

class huffman
{
private:
	struct Node
	{
		Node(int score, char code, Node *left, Node *right) : score(score), code(code), left(left), right(right)
		{
		}

		~Node()
		{
			if (!left) return;
			delete left;
			delete right;
		}

		// Obtain bits code of specified character
		// vector<bool> correspond to binary values for character founded
		bool search(char search, std::vector<bool> &bits)
		{
			if (!left) return code == search;
			if (left->search(search, bits))
			{
				bits.push_back(false);
				return true;
			}
			if (right->search(search, bits))
			{
				bits.push_back(true);
				return true;
			}
			return false;
		}

		// Read and decode character in input stream
		// bit 0 = goto left node
		// bit 1 = goto right node
		// Stop loop when there are a leaf
		char read(ibstream &in)
		{
			Node *node = this;
			while (node->left) node = in.bit() ? node->right : node->left;
			return node->code;
		}

		// Decode recursivly node (same method that the encoding)
		void decode(ibstream &in, int length)
		{
			if (--length == 0) return;
			if (in.bit())
			{
				(left = new Node(0, 0, 0, 0))->decode(in, length);
				(right = new Node(0, 0, 0, 0))->decode(in, length);
			}
			else in.read(&code, 8);
		}

		// Encode recursivly node with following method
		// bit 1 = node follow of left and right nodes
		// bit 0 = leaf follow of character
		void encode(obstream &out)
		{
			out.bit(left);
			if (left)
			{
				left->encode(out);
				right->encode(out);
			}
			else out.write(&code, 7);
		}

		int score;
		char code;
		Node *left;
		Node *right;
		char _reserved[3]; // for alignment
	};

public:
	void encode(std::istream &in, obstream &out)
	{
		// Parse file for get frequency of each character
		std::unordered_map<char, int> frequences;
		for (char c; in.get(c);) ++frequences[c];

		// Initialize priority_queue for sort leaf by frequency ASC
		auto lower = [](Node *l, Node *r) { return l->score > r->score; };
		std::priority_queue<Node *, std::vector<Node *>, decltype(lower)> queue(lower);
		for (auto it = frequences.begin(), end = frequences.end(); it != end; ++it) queue.push(new Node(it->second, it->first, 0, 0));

		// Build tree
		while (queue.size() > 1)
		{
			Node *left = queue.top();
			queue.pop();
			Node *right = queue.top();
			queue.pop();
			queue.push(new Node(left->score + right->score, 0, left, right));
		}

		// Get root of tree
		Node *root = queue.top();

		// Prepare "2 * uint32" for write length of tree and data
		// 1 : length of tree
		// 2 : length of data
		out.seek(8);

		// Encode tree and write in output stream
		root->encode(out);

		unsigned int treeLength = out.tell();

		// Get codes for all leafs
		std::unordered_map<char, std::vector<bool>> codes;
		for (auto it = frequences.begin(), end = frequences.end(); it != end; ++it) root->search(it->first, codes[it->first]);

		// Delete tree for not use useless memory
		delete root;

		// Prepare input stream for read a second time
		in.clear(0);
		in.seekg(0, in.end);
		unsigned int dataLength = (unsigned int)in.tellg();
		in.seekg(0);

		// Encode all data with codes of leafs
		for (char c; in.get(c);)
		{
			auto code = codes[c];
			for (auto it = code.rbegin(), end = code.rend(); it != end; ++it) out.bit(*it);
		}

		// Write to beginning of the file
		// 4byte : length of tree
		// 4byte : length of data
		out.flush();
		out.write((char *)&treeLength, 31);
		out.write((char *)&dataLength, 31);
	}

	void decode(ibstream &in, std::ostream &out)
	{
		// Read length of tree
		unsigned int treeLength;
		in.read((char *)&treeLength, 32);

		// Read length of data
		unsigned int dataLength;
		in.read((char *)&dataLength, 32);

		Node *root = new Node(0, 0, 0, 0);
		root->decode(in, treeLength);

		while (dataLength--)
		{
			char c = root->read(in);
			out << c;
		}

		delete root;
	}
};

class App
{
public:
	int run(int argc, char *argv[])
	{
		if (argc < 4) goto Exit;

		for (int i = 0; i < argc; ++i) std::cout << i << ") " << argv[i] << std::endl;

		ifs.open(argv[2], ifs.binary);
		if (!ifs.is_open()) return exit("First argument will be a source file");
		ofs.open(argv[3], ofs.binary);
		if (!ofs.is_open()) return exit("First argument will be a destination file");

		huffman h;

		if (strncmp(argv[1], "encode", 6) == 0)
		{
			obstream obs(ofs);
			h.encode(ifs, obs);
			return exit("Encoded done");
		}

		if (strncmp(argv[1], "decode", 6) == 0)
		{
			ibstream ibs(ifs);
			h.decode(ibs, ofs);
			return exit("Decoded done");
		}
	
	Exit:
		return exit("List of commands :\nhuffman encode [source file] [destination file]\nhuffman decode [source file] [destination file]\n");
	}

private:
	int exit(const char *message)
	{
		if (ifs.is_open()) ifs.close();
		if (ofs.is_open()) ofs.close();
		std::cout << message << std::endl;
		return 0;
	}

private:
	std::ifstream ifs;
	std::ofstream ofs;
};

int main(int argc, char *argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	App app;
	return app.run(argc, argv);
}