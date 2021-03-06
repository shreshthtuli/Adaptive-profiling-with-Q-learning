/*
Author: Shreshth Tuli
Date:   26/02/2018

This program functions as the "brain" of a reinforcement learning
agent whose goal is to provide the optimal values of number of buffers and
number of threads for efficient profiling.
For use in Alleria profiler code
*/

#include <iostream>
#include <random>
#include <math.h>
#include <string>
#include <stdlib.h>
#include <windows.h>

using namespace std;

/////////////////////////////////////////////////////////////////////////////////

//Computational parameters
float gamma_q = 0.75;      //look-ahead weight
float alpha = 0.20;       //"Forgetfulness" weight.  The closer this is to 1 the more weight is given to recent samples.
                         //A high value is kept because of a highly dynamic situation, we cannot keep it very high as then the system might not converge

//Parameters for getAction()
float epsilon;           //epsilon is the probability of choosing an action randomly.  1-epsilon is the probability of choosing the optimal action

//Variable 1 Parameters - Number of threads
const int numTheta1States = 9;
float theta1InitialCount = 2;
int theta1CurrentCount = floor(theta1InitialCount);
float theta1Max = 8;                          
float theta1Min = 0;
float deltaTheta1 = 1;   
int s1 = int((theta1CurrentCount - theta1Min)/deltaTheta1);                  //This is an integer between zero and numTheta1States-1 used to index the state number of threads

//Initialize Q to zeros
const int numStates = numTheta1States;
const int numActions = 3;
float Q[numStates][numActions];

//Initialize the state number. The state number is calculated using the theta1 state number and 
//the theta2 state number.  This is the row index of the state in the matrix Q. Starts indexing at 0.
int s = int(s1);
int sPrime = s;

//Initialize vars for getDeltaDistance()
float distanceNew = 0.0;
float distanceOld = 0.0;
float deltaDistance = 0.0;

//These get used in the main loop
float r = 0.0;
float lookAheadValue = 0.0;
float sample = 0.0;
int a = 0;

//Returns an action 0, 1, ... 4 : NONE, theta++, theta--
int getAction(){
  int action;
  float valMax = -10000000.0;
  float val;
  int aMax;
  float randVal;
  int allowedActions[3] = {1, -1, -1};  //-1 if action of the index takes you outside the state space.  +1 otherwise
  s1 = theta1CurrentCount;
  boolean randomActionFound = false;
  
  //find the optimal action.  Exclude actions that take you outside the allowed-state space.
  val = Q[s1][0];
  if(val > valMax){
    valMax = val;
    aMax = 0;
  }
  if((s1 + 1) < numTheta1States){
    allowedActions[1] = 1;
    val = Q[s][1];
    if(val > valMax){
      valMax = val;
      aMax = 1;
    }
  }
  if(s1 > 0){
    allowedActions[2] = 1;
    val = Q[s][2];
    if(val > valMax){
      valMax = val;
      aMax = 2;
    }
  }
  
  //implement epsilon greedy
  randVal = float(rand() % 101);
  if(randVal < (1.0-epsilon)*100.0){    //choose the optimal action with probability 1-epsilon
    action = aMax;
  }else{
    while(!randomActionFound){
      action = int(rand() % 3);        //otherwise pick an action between 0 and 3 randomly (inclusive), but don't use actions that take you outside the state-space
      if(allowedActions[action] == 1){
        randomActionFound = true;
      }
    }
  }    
  return(action);
}

//Given a and the global(s) find the next state.  Also keep track of the individual joint indexes s1 and s2.
void setSPrime(int action){  //NONE, theta1++&&theta2++, theta1--, theta2++
  if(action ==0){
    //NONE
    sPrime = s;
  }
  else if (action == 1 && (s+1)<numTheta1States){
    //theta1++
    sPrime = s + 1;
    s1++;
  }else if (action == 2 && s>0){
    //theta1--
    sPrime = s - 1;
    s1--;
}
}

//Update the number of threads and buffers (this is the physical state transition command) 
void setPhysicalState(int action){
  float currentCount;
  float finalCount;

  if (action == 1){
    theta1CurrentCount = theta1CurrentCount + deltaTheta1;
    ///const char* str = ("pin -t ./MemTrace/membuffer_threadpool -num_processing_threads " + std::to_string(theta1CurrentCount) + " -- profiler.exe").c_str();
    ///system(str);
    //theta1 write finalCount
  }else if (action == 2){
    theta1CurrentCount = theta1CurrentCount - deltaTheta1;
    ///const char* str = ("pin -t ./MemTrace/membuffer_threadpool -num_processing_threads " + std::to_string(theta2CurrentCount) + " -- profiler.exe").c_str();
    ///system(str);
    //theta1 write finalCount
  }
}


//Get the reward using the increase in performance since the last call
float getDeltaDistance(){
  //get current performance
  //cin >> distanceNew;// read current performance
  myfile << distanceNew << endl;
  deltaDistance = distanceNew - distanceOld;

  distanceOld = distanceNew;
  return deltaDistance;
}


//Get max over a' of Q(s',a'), but be careful not to look at actions which take the agent outside of the allowed state space
float getLookAhead(){
  float valMax = -100000.0;
  float val;
  s1 = theta1CurrentCount;
  val = Q[sPrime][0];
  if(val > valMax){
    valMax = val;
  }
  if((s1 + 1) < numTheta1States){
    val = Q[sPrime][1];
    if(val > valMax){
      valMax = val;
    }
  }
  if(s1 > 0){
    val = Q[sPrime][2];
    if(val > valMax){
      valMax = val;
    }
  }
  return valMax;
}

void initializeQ(){
  for(int i=0; i<numStates; i++){
    for(int j=0; j<numActions; j++){
      Q[i][j] = 10.0;               //Initialize to a positive number to represent optimism over all state-actions
    }
  }
  s = 2;
}


/////////////////////////////////////////////////////////////////////////////////

const int readDelay = 10;                   //allow time for the agent to execute after it sets its physical state
const float explorationMinutes = 1;        //the desired exploration time in minutes 
const float explorationConst = (explorationMinutes*60.0)/((float(readDelay))/1000.0);  //this is the approximate exploration time in units of number of times through the loop

int t = 0;
int main(){
  epsilon = 1;
  while (epsilon>0.05){
    t++;
    epsilon = exp(-float(t)/explorationConst);
    a = getAction();           //a is beween 0 and 4
    setSPrime(a);              //this also updates s1 and s2.
    setPhysicalState(a);
    Sleep(readDelay);                      //put a delay after the physical action occurs so the agent has some delay between two performance reads 
    r = getDeltaDistance();
    lookAheadValue = getLookAhead();
    sample = r + gamma_q*lookAheadValue;
    Q[s][a] = Q[s][a] + alpha*(sample - Q[s][a]);
    s = sPrime;
    
    if(t == 2){                //need to reset Q at the beginning since a spurious value may arise at the first initialization
      initializeQ();
    }
  }  
}

/*
//Get the reward using the increase in performance since the last call
float getDeltaDistance(ptit, atwt){
  //get current performance
  distanceNew = 1000.0f - (50.0f*ptit) - (50.0f*atwt);
  deltaDistance = distanceNew - distanceOld;

  distanceOld = distanceNew;
  return deltaDistance;
}
*/