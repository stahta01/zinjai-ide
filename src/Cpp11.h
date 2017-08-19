#ifndef CPP11_H
#define CPP11_H

/// crappy workaround for the need of lambda functions (mac port use some old pre c++11 gcc)
class GenericAction {
public:
	virtual void Run()=0;
	virtual ~GenericAction(){};
};

#define _LAMBDA_0(Name,Action) \
	class Name : public GenericAction {\
	public: void Run() override Action };

#define _LAMBDA_1(Name,Type,Arg,Action) \
	class Name : public GenericAction {\
	public: Name(Type arg) : Arg(arg) {} \
	public: void Run() override Action \
	private: Type Arg; };

/// crappy workaround for the need of lambda functions (mac port use some old pre c++11 gcc)
template<typename T>
class GenericActionEx {
public:
	virtual void Do(T arg)=0;
	virtual ~GenericActionEx(){};
};

#define _LAMBDAEX_0(Name,Type,ArgName,Action) \
	class Name : public GenericActionEx<Type> {\
	public: void Do(Type ArgName) Action };

#define _LAMBDAEX_1(Name,Type,ArgName,AType1,Attrib1,Action) \
	class Name : public GenericActionEx<Type> {\
	public: Name(AType1 at1) : Attrib1(at1) {} \
	public: void Do(Type ArgName) Action \
	private: AType1 Attrib1; };

#define _LAMBDAEX_2(Name,Type,ArgName,AType1,Attrib1,AType2,Attrib2,Action) \
	class Name : public GenericActionEx<Type> {\
	public: Name(AType1 at1, AType2 at2) : Attrib1(at1), Attrib2(at2) {} \
	public: void Do(Type ArgName) Action \
	private: AType1 Attrib1; AType2 Attrib2; };


#define _CAPTURELIST_3(LType,LName,VType1,VName1,VType2,VName2,VType3,VName3) \
	struct LType { VType1 VName1; VType2 VName2; VType3 VName3; } LName; \
	LName.VName1=VName1; LName.VName2=VName2; LName.VName3=VName3;

#define _CAPTURELIST_4(LType,LName,VType1,VName1,VType2,VName2,VType3,VName3,VType4,VName4) \
	struct LType { VType1 VName1; VType2 VName2; VType3 VName3; VType4 VName4; } LName; \
	LName.VName1=VName1; LName.VName2=VName2; LName.VName3=VName3; LName.VName4=VName4;

#define _CAPTURELIST_5(LType,LName,VType1,VName1,VType2,VName2,VType3,VName3,VType4,VName4,VType5,VName5) \
	struct LType { VType1 VName1; VType2 VName2; VType3 VName3; VType4 VName4; VType5 VName5; } LName; \
	LName.VName1=VName1; LName.VName2=VName2; LName.VName3=VName3; LName.VName4=VName4; LName.VName5=VName5;


#if __cplusplus >= 201103

#	define _CPP11_ENABLED 1
#include <memory>
using std::unique_ptr;
using std::make_unique;
	
#else

// missing keywords
#include <cstddef>
#define nullptr NULL
#define override


// unique_ptr emulation for c++03

template<typename T> class make_unique;

template<class T>
class unique_ptr {
	T *m_ptr;
	unique_ptr(const unique_ptr &other); /// forbiden
	unique_ptr& operator=(const unique_ptr &other); /// forbiden
public:
	unique_ptr(const make_unique<T> &mu) : m_ptr(mu.m_ptr) {}
	unique_ptr& operator=(const make_unique<T> &mu) { reset(mu.m_ptr); return *this; }
	unique_ptr() : m_ptr(nullptr) {}
	unique_ptr(T *ptr) : m_ptr(ptr) {}
	void reset(T *ptr) { delete m_ptr; m_ptr = ptr; }
	T *release() { T *p = m_ptr; m_ptr = nullptr; return p; }
	T &operator*() { return *m_ptr; }
	T *operator->() { return m_ptr; }
	operator const T*() { return m_ptr; }
	~unique_ptr() { delete m_ptr; }
	T *get() const { return m_ptr; }
};

template<typename T>
class make_unique {
	T *m_ptr;
	friend class unique_ptr<T>;
public:
	explicit make_unique() : m_ptr(new T()) {}
	template<typename T1>
	explicit make_unique(const T1 &t1) : m_ptr(new T(t1)) {}
	template<typename T1, typename T2>
	explicit make_unique(const T1 &t1, const T2 &t2) : m_ptr(new T(t1,t2)) {}
	template<typename T1, typename T2, typename T3>
	explicit make_unique(const T1 &t1, const T2 &t2, const T3 &t3) : m_ptr(new T(t1,t2,t3)) {}
	operator T*() { return m_ptr; }
};

#endif // cpp11


#endif
