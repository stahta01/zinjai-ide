#ifndef JAVAVECTOR_H
#define JAVAVECTOR_H
#include "SingleList.h"

template<typename T> class JavaVectorIterator;

/// vector of dynamic objects, this container owns (will delete) the pointers you add
template<typename T>
class JavaVector : private SingleList<T*> {
	friend class JavaVectorIterator<T>;
public:
	JavaVector() {}
	JavaVector(const JavaVector &other) {
		(*this)=other;
	}
	JavaVector &operator=(const JavaVector &other) {
		Clear();
		SingleList<T*>::EnsureMemFor(other.GetSize());
		for(int i=0;i<other.GetSize();i++)
			Add(new T(*other[i]));
		return *this;
	}
	int NotFound() const { return SingleList<T*>::NotFound(); }
	int FindPtr(T * ptr) const { return SingleList<T*>::Find(ptr); }
	int FindObject(const T &obj) const { 
		for(int i=0;i<SingleList<T*>::GetSize();i++)
			if ((*SingleList<T>::operator[](i))==obj) return i;
		return SingleList<T>::NotFound();
	}
	void Remove(int i) { 
		delete SingleList<T*>::operator[](i);
		SingleList<T*>::Remove(i);
	}
	int GetSize() const { return SingleList<T*>::GetSize(); }
	void Add(T *ptr) { SingleList<T*>::Add(ptr); }
	T const * const operator[](int i) const { return SingleList<T*>::operator[](i); }
	T * const operator[](int i) { return SingleList<T*>::operator[](i); }
	void Clear() {
		for(int i=0;i<SingleList<T*>::GetSize();i++)
			delete SingleList<T*>::operator[](i);
		SingleList<T*>::Clear();
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

