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
		write(&buf, shift);
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

		void decode(ibstream &in)
		{
			if (in.bit())
			{
				(left = new Node(0, 0, 0, 0))->decode(in);
				(right = new Node(0, 0, 0, 0))->decode(in);
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
	int encode(std::istream &in, obstream &out)
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
		unsigned int treeLength = out.tell();
		root->encode(out);

		unsigned int dataLength = out.tell();
		treeLength = dataLength - treeLength;

		// Get codes for all leafs
		std::unordered_map<char, std::vector<bool>> codes;
		for (auto it = frequences.begin(), end = frequences.end(); it != end; ++it) root->search(it->first, codes[it->first]);

		// Delete tree for not use useless memory
		delete root;

		// Prepare input stream for read a second time
		in.clear(0);
		in.seekg(0);

		// Encode all data with codes of leafs
		for (char c; in.get(c);)
		{
			auto code = codes[c];
			for (auto it = code.rbegin(), end = code.rend(); it != end; ++it) out.bit(*it);
		}

		dataLength = out.tell() - dataLength;

		// Write to beginning of the file
		// 4byte : length of tree
		// 4byte : length of data
		out.flush();
		out.write((char *)&treeLength, 31);
		out.write((char *)&dataLength, 31);

		std::cout << "Length of tree writted : " << treeLength << std::endl;
		std::cout << "Length of data writted : " << dataLength << std::endl;

		return 0;
	}

	int decode(ibstream &in, std::ostream &out)
	{
		// Read length of tree
		unsigned int treeLength;
		in.read((char *)&treeLength, 32);
		std::cout << "Length of tree readed : " << treeLength << std::endl;

		unsigned int dataLength;
		in.read((char *)&dataLength, 32);
		std::cout << "Length of data readed : " << dataLength << std::endl;

		Node *root = new Node(0, 0, 0, 0);
		root->decode(in);

		delete root;

		return 0;
	}
};

int main(int argc, char *argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	huffman h;

	std::istringstream iss("this is an example of a huffman tree - fuisofqdiwqp0kdwqjdmnavhiausjefojaefiaueifjPWKDSOADJMOADAUIJWDJAWDGRDG");
	std::ofstream ofs(argv[1], std::ofstream::binary);
	obstream obs(ofs);
	h.encode(iss, obs);
	ofs.close();

	std::ifstream ifs(argv[1], std::ifstream::binary);
	ibstream ibs(ifs);
	std::ostringstream oss;
	h.decode(ibs, oss);
	ifs.close();

	std::cout << oss.str() << std::endl;

	getchar();
	return 0;
}