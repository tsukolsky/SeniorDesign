/*******************************************************************************\
| myVector.h
| Initial Build: 4/8/2013
| Last Revised: 4/8/2013
|================================================================================
| Description: Minimal class to replace std::vector
|--------------------------------------------------------------------------------
| Revisions:
|================================================================================
| *NOTES: (1)-Tested. Works like a charm. when pushing, [0] value is the FIRST IN, NOT LAST IN.
|		Structure is FILO where, if you push four things: 0, 1->0, 2->1->0, 3->2->1->0
| 		are the indices.
\*******************************************************************************/
#include <inttypes.h>
// Minimal class to replace std::vector
template<typename Data>
class myVector {
	size_t d_size; // Stores no. of actually stored objects
	size_t d_capacity; // Stores allocated capacity
	Data *d_data; // Stores data
	public:
		myVector() : d_size(0), d_capacity(0), d_data(0) {}; // Default constructor
		myVector(myVector const &other) : d_size(other.d_size), d_capacity(other.d_capacity), d_data(0) { d_data = (Data *)malloc(d_capacity*sizeof(Data)); memcpy(d_data, other.d_data, d_size*sizeof(Data)); }; // Copy constuctor
		~myVector() { free(d_data); }; // Destructor
		myVector &operator=(myVector const &other) { free(d_data); d_size = other.d_size; d_capacity = other.d_capacity; d_data = (Data *)malloc(d_capacity*sizeof(Data)); memcpy(d_data, other.d_data, d_size*sizeof(Data)); return *this; }; // Needed for memory management
		void push_back(Data const &x) { if (d_capacity == d_size) resize(); d_data[d_size++] = x; }; // Adds new value. If needed, allocates more space
		Data pop_back(){return d_data[--d_size];}
		size_t size() const { return d_size; }; // Size getter
		Data const &operator[](size_t idx) const { return d_data[idx]; }; // Const getter
		Data &operator[](size_t idx) { return d_data[idx]; }; // Changeable getter
   private:
		void resize() { d_capacity = d_capacity ? d_capacity*2 : 1; Data *newdata = (Data *)malloc(d_capacity*sizeof(Data)); memcpy(newdata, d_data, d_size * sizeof(Data)); free(d_data); d_data = newdata; };// Allocates double the old space
};
