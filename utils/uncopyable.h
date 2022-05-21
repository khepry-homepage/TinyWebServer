#ifndef TINY_UNCOPYABLE_H
#define TINY_UNCOPYABLE_H

class Uncopyable {
protected:
	Uncopyable();
	~Uncopyable();
private: 
	Uncopyable(const Uncopyable&);
	Uncopyable& operator=(const Uncopyable&);
};

#endif