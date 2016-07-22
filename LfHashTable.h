/*
 * LfHashTable.h
 *
 *  Created on: Jul 20, 2016
 *      Author: noam
 */

#ifndef LFHASHTABLE_H_
#define LFHASHTABLE_H_

#ifdef _DEBUG
#define DEBUG_RCU
#endif

#include <memory>
#include <stdexcept>
#include <urcu-qsbr.h> 		/* RCU flavor */
#include <urcu/rculfhash.h>	/* RCU Lock-free hash table */
#include <functional>
//#include "jhash.h"


//-------------------------------------------------------------------

/** A hashtable(key->value) using URCU: single writer, multiple reader threads
 *
 * This implementation requires that all reader threads periodically
 * indicate quiescent states via either rcu_quiescent_state() for a momentary quiescent state
 *  or using rcu_thread_offline()/rcu_thread_online() pairs for extended quiescent states.
 * https://lwn.net/Articles/573424/#URCU%20Flavors
 *
 * see sample code in C at http://git.lttng.org/?p=userspace-rcu.git;a=blob;f=doc/examples/rculfhash/cds_lfht_lookup.c
 *
  The class exposes (most of) the std::map semantics

  note: one of the change vs the demo code, is that here I keep both key and value in the node.
  This is needed for example for "Mymap::iterator it = map.begin(); foo(it->first)
 if you get (in DEBUG run):
 "Assertion `(rcu_reader_qsbr).ctr' failed." it probably means you forgot to call registerThread()

  <code>
    LfHashTable<std::string,Value> ht;
    ht.registerThread();
	ht.init(16, 1024, 1024);
	ht.insert(std::string("one"),Value(11));
	ht.insert(std::string("two"),Value(22));
	ht.erase("one");
	EXPECT_EQ(ht.find("one"), ht.end() );
	ht.unregisterThread();
   </code>

   TODO:
   * finish iterators
   * implement destroy() of the table
   * change insert() to return value_type ( like std )
 */
template <class K, class V>
class LfHashTable {
public: typedef std::pair<K,V> value_type;

private:
	std::hash<K> hash_fn; // if you get compile error...

	/*
	 * Nodes populated into the hash table.
	 */
	struct mynode {
		value_type kv;	/* Node content */
		struct cds_lfht_node node;	/* Chaining in hash table */
		struct rcu_head rcu_head;	/* For call_rcu() */
	};

	static
	int match(struct cds_lfht_node *ht_node, const void *_key){
		struct mynode *node = caa_container_of(ht_node, struct mynode, node);
		const K *key = reinterpret_cast<const K*>(_key);

		//std::cout << "comparing "<< *key << " vs " << node->kv.first << std::endl;
		return *(const_cast<K*>(key)) == node->kv.first;
	}

	static
	void free_node(struct rcu_head *head){
		struct mynode *node = caa_container_of(head, struct mynode, rcu_head);
		free(node);
	}

	struct cds_lfht *ht;	/* Hash table */

public:
	LfHashTable() :ht(nullptr){}

	/**
	 * initialize the table. must be called before any insert/find/erase
	 *
	@init_size: number of buckets to allocate initially. Must be power of two.
	@min_nr_alloc_buckets: the minimum number of allocated buckets.
	                        (must be power of two)
	@max_nr_buckets: the maximum number of hash table buckets allowed.
	                  (must be power of two, 0 is accepted, means "infinite")
	 */
	void init(unsigned long init_size,
		      unsigned long min_nr_alloc_buckets,
		      unsigned long max_nr_buckets ) throw(std::bad_alloc){
		ht = cds_lfht_new(init_size, max_nr_buckets, 0,	CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING,	NULL);
		if(ht == NULL)
			throw std::bad_alloc();
	}




	// -----------------------------------------------------------------------
	class iterator : public std::iterator<std::input_iterator_tag, V, std::ptrdiff_t, V*, V& >{
		struct mynode *pos;
		cds_lfht* ht; // the container I am iterating on

		public:
			cds_lfht_iter iter; // todo: make it private

			explicit iterator(cds_lfht* ht_ ) :  ht(ht_) {
			}

			iterator& operator++() {
				cds_lfht_next(ht, &iter);
				return *this;
			}
			iterator operator++(int) { iterator retval = *this; ++(*this); return retval;}
			bool operator==(iterator other) const {
				return cds_lfht_iter_get_node(const_cast<cds_lfht_iter*>(&iter)) ==
					   cds_lfht_iter_get_node(const_cast<cds_lfht_iter*>(&(other.iter)));
			}
			bool operator!=(iterator other) const {return !(*this == other);}

			value_type& operator*() const {

				cds_lfht_node* ht_node  = cds_lfht_iter_get_node(const_cast<cds_lfht_iter*>(&iter));
				mynode * node = caa_container_of(ht_node, struct mynode, node);
				//value_type pair(node->kv.first, node->value);
				return node->kv;
			}
			value_type* operator -> () {
				// area of voodoo. beware.
				mynode* pNode = caa_container_of(cds_lfht_iter_get_node(&iter),mynode, node);
				return   &pNode->kv;
			}

	}; // class iterator



	/**
	 * each thread that use read  MUST register before
	 */
	void registerThread(){
		rcu_register_thread();
	}

	void unregisterThread(){
		rcu_unregister_thread();
	}

	/** announce when a reader thread is not busy reading.
	 *
	 * for example, in an eventloop::OnIdle(), call rcu_quiescent_state()
	 */
	void rcu_quiescent_state(){rcu_quiescent_state();}
	void rcu_thread_offline(){rcu_thread_offline();}
	void rcu_thread_online(){rcu_thread_online();}

	/** insert <k,v> into the hashtable
	* Note  malloc() called for the node object
	*/
	void insert(value_type pair)  throw(std::bad_alloc) {
		struct mynode *node = (mynode *)malloc(sizeof(*node));
		if(node == NULL)
			throw std::bad_alloc();
		cds_lfht_node_init(&node->node);
		node->kv = pair;
		unsigned long hash = hash_fn(node->kv.first); // in the sample code, the hashing is on VALUE

		/*
		 * cds_lfht_add() needs to be called from RCU read-side
		 * critical section.
		 */
		rcu_read_lock();
		cds_lfht_add(ht, hash, &node->node);
		//printf("Add (key: %d)\n", node->value);
		rcu_read_unlock();

	}

	/**
	 * find a key in the table.
	 */
	iterator find(K key){
		struct cds_lfht_node *ht_node;
		struct cds_lfht_iter iter;	/* For iteration on hash table */
		struct mynode *node;
		V* pV;
		unsigned long hash = hash_fn(key);
		rcu_read_lock();
		cds_lfht_lookup(ht, hash, match, &key, &iter);
		ht_node = cds_lfht_iter_get_node(&iter);
		rcu_read_unlock();

		if (!ht_node) {
			return end();
		}
		// else {
//			node = caa_container_of(ht_node, struct mynode, node);
//			//printf("(key %d value: TBD) found\n", node->value);
//			pV = &(node->kv.second);
//		}

		iterator it(ht);
		it.iter = iter;
		return it;
	}

	/**
	 * erase a <key->value> from the map
	 * Can be called from a single thread at a time.
	 * @return how many elements where erased
	 * throw logic_error if concurrent deletion is detected. in this case the deletion has failed.
	 */
	int erase(K key) throw (std::logic_error) {
		cds_lfht_iter iter;
		cds_lfht_node *ht_node;
		bool found = false;
		bool deleted = false;
		int ret;

		unsigned long hash = hash_fn(key);
		rcu_read_lock();
		cds_lfht_lookup(ht, hash, match, &key, &iter);
		ht_node = cds_lfht_iter_get_node(&iter);
		if (ht_node) {
			found = true;
			ret = cds_lfht_del(ht, ht_node);
			if (ret) {
				printf(" (concurrently deleted)");
			} else {
				struct mynode *del_node = caa_container_of(ht_node, mynode, node);
				call_rcu(&del_node->rcu_head, free_node);
				deleted = true;
			}
		} else {
			found = false;
		}
		rcu_read_unlock();

		if(!found)
			return 0;
		if(!deleted)
			throw std::logic_error("concurrently deleted in LfHashTable");

		return 1; 
	} //erase

	typedef void (*UnaryFunction)(value_type) ; // UnaryFunction is a function taking arg of type value_type

	/** iterate on all values in the table
	 *  MUST NOT forcefully break out of the loop since it will miss an internal call to rcu_read_unlock()
	 *
	 * <code>
	 * void lambda(int i){ printf("%d ", i);}
	 * table.for_each(lambda);
	 * </code>
	 */
	void for_each( UnaryFunction f){
		struct mynode *node;
		cds_lfht_iter iter;
		cds_lfht_node *ht_node;
		rcu_read_lock();
		cds_lfht_for_each_entry(ht, &iter, node, node) {
			(f)(node->kv);
		}
		rcu_read_unlock();
	}


	iterator begin() {
		iterator it(ht);
		cds_lfht_first(ht, &it.iter);// " Output in "*iter". *iter->node set to NULL if table is empty."
		return it;
	}

	iterator end() {
		iterator it(ht);
		it.iter.node = NULL;
		return it;
	}
};

#endif /* LFHASHTABLE_H_ */
