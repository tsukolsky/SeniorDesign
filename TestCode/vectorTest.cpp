//This is a test of the "myVector.h" class. Tested on 4/14. Works. see "myVector.h" for comments. 

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include "myVector.h"

using namespace std;

int main(){
	char *messages[4] = {"hello.","two.","three.","four."};
	myVector<char *> theVector;

	cout << "\n\nTesting push\n";
	for (int i=0; i<4; i++){
		theVector.push_back(messages[i]);
		cout << "Pushed " << messages[i] << endl;
	}
	
	cout << "\n\nSize of vector is " << theVector.size() << endl;
	cout << "\nTesting [] indexing.\n";
	for (int j=0; j<4; j++){
		cout << "vector[" << j << "]= " << theVector[j] << endl;
	}
	
	cout << "\n\nTesting Pop\n";
	for (int k=0; k<4; k++){
		char *tempString;
		tempString=theVector.pop_back();	
		cout << "Popping, got: " << tempString << endl;
	}

	cout << "\n\nEnd of test.\n";

	return 0;
}
