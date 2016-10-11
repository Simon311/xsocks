#pragma once

//声明为不可复制的类
//Declare a class that can not be copied??
#define DECLARE_UNCOPYABLE(className)			\
	private:									\
		className(const className&);			\
		className& operator=(const className&);	

//声明为单例类
//Declare a singleton class??
#define DECLARE_SINGLETON(className)			\
	public:										\
		static className& GetInstanceRef()		\
		{										\
			static className s_instance;		\
			return s_instance;					\
		}										\
		~className();							\
												\
		BOOL Init();							\
		void Deinit();							\
	private:									\
		className();

#define XOR(a, b) (((a) && !(b)) || (!(a) && (b)))
