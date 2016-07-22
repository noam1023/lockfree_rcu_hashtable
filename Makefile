CPPFLAGS=-g -O2 -DNDEBUG
CXXFLAGS= -std=c++11 #-D_LGPL_SOURCE
      
OBJS = $(patsubst %.cpp,%.o, $(patsubst %.c,%.o,$(SRCS)))

TEST_SRCS=  LfHashTable_test.cpp 
	
TEST_OBJS = $(patsubst %.cpp,%.o, $(patsubst %.c,%.o,$(TEST_SRCS)))

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $< 


test: $(TEST_OBJS)
	$(CXX)   -g -O0  -o test \
	   $^  -lPocoFoundation -lgtest_main -lurcu-cds -lurcu-qsbr -lgtest -pthread


clean:
	rm -f *.o  test_server

