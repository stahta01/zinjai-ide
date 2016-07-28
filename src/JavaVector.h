#ifndef JAVAVECTOR_H
#define JAVAVECTOR_H
#include "SingleList.h"
#include "Cpp11.h"

template<typename T> class JavaVectorIterator;

/// vector of dynamic objects, this container owns (will delete) the pointers you add
template<typename T>
class JavaVector : private SingleList<T*> {
	typedef SingleList<T*> Base;
	friend class JavaVectorIterator<T>;
public:
	JavaVector() {}
	JavaVector(const JavaVector &other) {
		(*this)=other;
	}
	JavaVector &operator=(const JavaVector &other) {
		Clear();
		Base::EnsureMemFor(other.GetSize());
		for(int i=0;i<other.GetSize();i++)
			Add(new T(*other[i]));
		return *this;
	}
	int NotFound() const { return Base::NotFound(); }
	int FindPtr(T * ptr) const { return Base::Find(ptr); }
	int FindObject(const T &obj) const { 
		for(int i=0;i<Base::GetSize();i++)
			if (Base::operator[](i) && Base::operator[](i)==obj) return i;
		return Base::NotFound();
	}
	T *Release(int i) { 
		T* ptr = Base::operator[](i);
		Base::operator[](i)=nullptr;
		return ptr;
	}
	void Set(int i, T *ptr) {
		delete Base::operator[](i);
		Base::operator[](i) = ptr;
	}
	void Remove(int i) { 
		delete Base::operator[](i);
		Base::Remove(i);
	}
	int GetSize() const { return Base::GetSize(); }
	void Add(T *ptr) { Base::Add(ptr); }
	T const * const operator[](int i) const { return Base::operator[](i); }
	T * const operator[](int i) { return Base::operator[](i); }
	void Clear() {
		for(int i=0;i<Base::GetSize();i++)
			delete Base::operator[](i);
		Base::Clear();
	}
	~JavaVector() { Clear(); }
};


template<typename T>
class JavaVectorIterator {
	JavaVector<T> *m_vector;
	int m_pos;
public:
	JavaVectorIterator(JavaVector<T> &v) : m_vector(&v), m_pos(0) { }
	bool IsValid() const { return m_pos<m_vector->GetSize(); }
	void Next() { ++m_pos; }
	T * const operator->() { return (*m_vector)[m_pos]; }
	operator T*() const { return (*m_vector)[m_pos]; }
};

#endif

