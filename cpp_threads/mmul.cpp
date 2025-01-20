#include <iostream>
#include <thread>
#include <vector>

using namespace std;

const int numThreads=4;
const int matrixSize=1024;

__thread int x=0;

vector<vector<int> > a(matrixSize, vector<int> (matrixSize));
vector<vector<int> > b(matrixSize, vector<int> (matrixSize));
vector<vector<int> > c(matrixSize, vector<int> (matrixSize));

void mm(int myId) {  
  int i,j,k;  
  double sum;  

  cout << "mm() called by thread " << myId << endl;

  for (i=0; i<10; i++) {
    cout << "thread(" << myId << ") x=" << x++ << endl;
  }

  // compute bounds for this thread
  int startrow = myId * matrixSize/numThreads;  
  int endrow = (myId+1) * (matrixSize/numThreads) - 1;  
  
  // matrix mult over the strip of rows for this thread  
  for (i = startrow; i <= endrow; i++) {    
    for (j = 0; j < matrixSize; j++) {      
      sum = 0.0;      
      for (k = 0; k < matrixSize; k++) {	
	sum = sum + a[i][k] * b[k][j];      
      }      
      c[i][j] = sum;    
    }  
  }
}

int main(int argc, char *argv[]) {  
  int i, j;
  thread threads[numThreads];  
  
  for (i=0; i < matrixSize; i++) {
    for (j=0; j < matrixSize; j++) {
      a[i][j] = 2;
      b[i][j] = 3;
      c[i][j] = 0;
    }
  }
  
  // Create threads   
  for (i = 0; i < numThreads; i++) {
    threads[i] = thread(mm, i);
  }  

  //Wait for threads to finish
  for (i = 0; i < numThreads; i++) {    
    threads[i].join();
  } 

  int checksum = 0;
  for (i=0; i < matrixSize; i++) {
    for (j=0; j < matrixSize; j++) {
      checksum += c[i][j];
    }
  }

  cout << "Checksum = " << checksum << endl;
}

