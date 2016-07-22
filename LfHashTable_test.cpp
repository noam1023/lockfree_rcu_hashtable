/*
 * LfHashTable_test.cpp
 *
 *  Created on: Jul 20, 2016
 *      Author: noam
 */
#include <iostream>
#include "gtest/gtest.h"

#include "LfHashTable.h"
#include <map>


//#define  printf("%s %s %d\n", __FILE__, __func__, __LINE__);


struct Value{
	explicit Value(int x){v = x;}
	bool operator ==(const Value& other) const {
		return v == other.v;
	}
	int v;
	char garbage[100];
};

// for every type we use as key, must have a hash function
namespace std {
  template <> struct hash<Value>
  {
    size_t operator()(const Value & x) const
    {
      return x.v; // worthless as hash, but good enough for testing
    }
  };
}


TEST(URCU_hash, simple_map_iteration){
	typedef std::map<int,char*> M;
	M m;

	M::iterator it = m.begin();
	ASSERT_EQ(it, m.begin()); // idempotent operator
	ASSERT_EQ(m.begin(), m.end()); // the tablt is empty
	m.insert(M::value_type(3,(char*)"33"));
	int x = m.begin()->first;
	EXPECT_EQ(m.begin()->first, 3);
	EXPECT_EQ(m.begin()->second, "33");

}


TEST(URCU_hash, simple_iteration){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.init(1024, 1024, 1024);
	ht.registerThread();
	LfHashTable<int,int>::iterator it = ht.begin();
	ASSERT_EQ(it, ht.begin()); // idempotent operator
	ASSERT_EQ(ht.begin(), ht.end()); // the table is empty
	ht.insert(M::value_type(3,33) );
	int x = ht.begin()->first;
	EXPECT_EQ(ht.begin()->first, 3);
	EXPECT_EQ(ht.begin()->second, 33);
	ht.unregisterThread();
}

TEST(URCU_hash, init){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.init(1024, 1024, 1024);
}


TEST(URCU_hash, init2){
	LfHashTable<int,int> ht;
	EXPECT_THROW(ht.init(1024, 100, 1000), std::bad_alloc);
}

/*
// this one causes assert from within the lib (when compiled in debug)
TEST(URCU_hash, inverted_registration){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.unregisterThread();
	ht.init(1024, 1024, 1024);
	ht.registerThread();
	ht.insert(M::value_type(3,33));
	ht.registerThread();
}
*/

TEST(URCU_hash, insert_int){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.init(1024, 1024, 1024);
	ht.registerThread();
	ht.insert(M::value_type(3,33));
	ht.insert(M::value_type(4,44));
	M::iterator it = ht.find(3);
	ASSERT_TRUE(it != ht.end());
	ASSERT_EQ(it->second, 33);
	ASSERT_EQ(it->first, 3);
	ht.unregisterThread();
}

TEST(URCU_hash, insert_Value){
	typedef LfHashTable<Value,int> M;
	M ht;
	ht.init(1024, 1024, 1024);
	ht.registerThread();
	ht.insert(M::value_type(Value(3),33));
	ht.insert(M::value_type(Value(4),44));
	M::iterator it = ht.find(Value(3));
	ASSERT_TRUE(it != ht.end());
	ASSERT_EQ(it->second, 33);
	ASSERT_EQ(it->first, Value(3));
	ht.unregisterThread();
}

TEST(URCU_hash, insert_duplicate){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.registerThread();
	ht.init(1024, 1024, 1024);
	ht.insert(M::value_type(3,33));
	ASSERT_EQ(ht.find(3)->second, 33);
	ht.insert(M::value_type(3,55)); // should somehow fail
	ASSERT_EQ(ht.find(3)->second, 33);
	ht.unregisterThread();
}

// play with string -> Object



TEST(URCU_hash, map_insert_complexTypes){
	typedef std::map<std::string,Value> M;
	M ht;
	Value v1(11), v2(22);
	ht.insert(M::value_type(std::string("one"),v1));
	ht.insert(M::value_type(std::string("two"),v2));
	Value v3 = ht.find("one")->second;
	ASSERT_EQ(v3.v, 11);
	v3 = ht.find("two")->second;
	ASSERT_EQ(v3.v, 22);

	// this should blow a fuse:
	v3 = ht.find("an honest man")->second;
}

void print_kv(LfHashTable<std::string,Value>::value_type p){
	std::cout << p.first << ":" << p.second.v <<std::endl;
}
TEST(URCU_hash, insert_complexTypes){
	typedef LfHashTable<std::string,Value> M;
	M ht;

	//std::string s1("abc");
	//std::string s2("abc");
	//ASSERT_EQ(s1,s2); // verify object equality and not object identity is tested
	Value v1(11), v2(22);
	std::string s11("one");
	std::string s22("two");
	
	ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(M::value_type(s11,v1));
	ht.insert(M::value_type(s22,v2));
	//ht.for_each(print_kv);
	EXPECT_TRUE(ht.find(s11) != ht.end());
	EXPECT_TRUE(ht.find(s22) != ht.end());
	Value v3 = ht.find("one")->second;
	ASSERT_EQ(v3.v, 11);
	v3 = ht.find("two")->second;
	ASSERT_EQ(v3.v, 22);
	ht.unregisterThread();
}

TEST(URCU_hash, erase_int_value){
	typedef LfHashTable<int,Value> M;
	M ht;
	int s11 = 11;
	int s22 = 22;
	Value v1(111), v2(222);

	ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(M::value_type(s11,v1));
	ht.insert(M::value_type(s22,v2));
	ht.erase(11);
	EXPECT_EQ(ht.find(11), ht.end());
	v2 = ht.find(22)->second;
	ASSERT_EQ(v2.v, 222);
	ht.unregisterThread();
}

TEST(URCU_hash, erase_value_int){
	typedef LfHashTable<Value,int> M;
	M ht;
	int s11 = 11;
	int s22 = 22;
	Value v1(111), v2(222);
	Value v1a(111);
	ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(M::value_type(v1,s11));
	ht.insert(M::value_type(v2,s22));
	ASSERT_NE(ht.find(Value(111)), ht.end());
	ASSERT_NE(ht.find(Value(222)), ht.end());
	ht.erase(v1a);
	EXPECT_EQ(ht.find(v1), ht.end());
	int x = ht.find(v2)->second;
	ASSERT_EQ(x, 22);
	ht.unregisterThread();
}
TEST(URCU_hash, erase_string_value){
	typedef LfHashTable<std::string,Value> M;
	M ht;
	std::string s11("one");
	std::string s22("two");
	Value v1(11), v2(22);
	ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(M::value_type(s11,v1));
	ht.insert(M::value_type(s22,v2));
	ASSERT_NE(ht.find(s22), ht.end());
	ASSERT_NE(ht.find("one"), ht.end());
	ht.erase("one");
	ASSERT_EQ(ht.find("one"), ht.end());
	ASSERT_NE(ht.find("two"), ht.end());
	v2 = ht.find("two")->second;
	ASSERT_EQ(v2.v, 22);
	ht.unregisterThread();
}


std::set<int> golden_values_foreach;
void lambda(LfHashTable<int,int>::value_type p ){ golden_values_foreach.erase(p.second);}

TEST(URCU_hash, foreach_c_style){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(M::value_type(1,11));
	ht.insert(M::value_type(3,33));
	ht.insert(M::value_type(2,22));

	golden_values_foreach.insert(11);
	golden_values_foreach.insert(22);
	golden_values_foreach.insert(33);
	ht.for_each(lambda);

	ASSERT_TRUE(golden_values_foreach.empty());
	ht.unregisterThread();
}

TEST(URCU_hash, foreach_c11_style){
	typedef LfHashTable<int,int> M;
	M ht;
	ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(M::value_type(1,11));
	ht.insert(M::value_type(3,33));
	ht.insert(M::value_type(2,22));

	golden_values_foreach.insert(11);
	golden_values_foreach.insert(22);
	golden_values_foreach.insert(33);
	for(auto p : ht){
		EXPECT_EQ(golden_values_foreach.erase(p.second),1);
	}

	ASSERT_TRUE(golden_values_foreach.empty());
	ht.unregisterThread();
}



/*
 * here is the serious stuff.
 * launch K writer threads and M reader threads.
 * perform concurrent operations, and check that the hashtable state is consistent
 * after all threads have finished working.
 *
 * The write threads insert and delete nodes, while the reader reads them.
 * A correct state is if all nodes are valid at any point during and after the run.
 * each node contains <k=int, v=int with the same value>.
 *
 */
#include "Poco/Thread.h"
#include <atomic>

static bool stop = false;
std::atomic<int> numReaders(0);
std::atomic<int> numWriters(0);
LfHashTable<int,int> ht;
static int N = 10000;

class Writer :public Poco::Runnable{
public:
	void run(){
		++numWriters;
		int err = 0;
		ht.registerThread();
		//printf("writer starting\n");
		while(!stop){
			for(int i = 0; i < N;i++){
				try{
					ht.erase(i);
				}
				catch(std::logic_error & ex){
					printf("concurrent deletion?!\n");
				}
				catch(std::exception &ex){
					printf(" exception %s \n", ex.what());
					throw;
				}
				catch(...){
					printf("unknown exception thrown in erase() !!!\n");
					throw;
				}

			}
			for(int i = 0; i < N;i++){
				ht.insert(LfHashTable<int,int>::value_type(i,i));
			}
		}
		ht.unregisterThread();
		numWriters--;
	}
	int* nWriters;
};

void foo(LfHashTable<int,int>::value_type){}

class Reader :public Poco::Runnable{
public:
	void run(){
		++numReaders;
		//printf("reader starting\n");
		ht.registerThread();
		while(!stop){
			ht.for_each(foo);
		}
		ht.unregisterThread();
		numReaders--;
		//printf("reader exit\n");
	}

	int* nReaders;
};

std::set<int> values_check;
void endgame(LfHashTable<int,int>::value_type p){
	int i = p.second;
	if(values_check.find(i) == values_check.end())
		throw i;
	values_check.erase(i);
}


TEST(URCU_hash, read_writer_threads){
	ht.init(1024, 1024, 1024);


	ht.registerThread();
	// fill with data
	for(int i = 0; i < N;i++){
		ht.insert(LfHashTable<int,int>::value_type(i,i));
		values_check.insert(i);
	}



	// start writers
	int R = 2; // how many of each
	int W = 1; // I found out that a single deleter is supporter, so no point trying more (it did show errors consistently when W > 1)
	Poco::Thread r[R],w[W];


	Writer* pW = new Writer;

	stop = false;
	// start readers
	for(int i = 0; i<R;i++){
		Reader* p = new Reader; // will leak but we don't care
		r[i].start(*p);
	}
	for(int i = 0; i<W;i++){
		Writer* p = new Writer; // will leak but we don't care
		w[i].start(*p);
	}
	// wait for them to be ready
	while(numReaders+numWriters < R+W);
	// wait some time
	Poco::Thread::sleep(1*1000);
	stop = true;

	for(int i = 0; i<W;i++){
			w[i].join();
	}
	//wait for the threads to exit
	for(int i = 0; i<R;i++){
		r[i].join();
	}

	ASSERT_EQ(numReaders,0);
	ASSERT_EQ(numWriters,0);

	//printf("r %d, w %d\n", int(numReaders), int(numWriters));
	// After the writers exit, the table has to be in the same state as before they started
	try{
		ht.for_each(endgame);
	}
	catch(int &ex){
		printf("offending value %d \n", ex);
//		for(auto i : values_check){
//			printf("%d,",i);
//		}
//		printf("\n");
		ASSERT_TRUE(!"invalid value - not found in set");
	}
	// finish and verify validity of data
	if(!values_check.empty()){
		printf("there are %lu elements\n", values_check.size());
		for(auto i : values_check){
			printf("%d,",i);
		}
		printf("\n-----------\n");
	}
	ASSERT_TRUE(values_check.empty());
	ht.unregisterThread();

}
