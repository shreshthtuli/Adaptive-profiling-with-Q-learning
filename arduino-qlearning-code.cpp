/*
Author: Andrew August 
Date:   10/20/14

This program functions as the "brain" of a reinforcement learning
agent whose goal is to crawl forward using two arms attached to two
servos.
For use in Arduino UNO
*/

#include <Servo.h>
#include <NewPing.h>
#define trigPin 10
#define echoPin 6
#define maxDistance 300
#define servo1Pin 13
#define servo2Pin 12
#define trigPin 10

NewPing ultrasonic(trigPin, echoPin, maxDistance);
Servo servo1;        //lower joint
Servo servo2;        //upper joint

/////////////////////////////////////////////////////////////////////////////////

//Computational parameters
float gamma = 0.75;      //look-ahead weight
float alpha = 0.1;       //"Forgetfulness" weight.  The closer this is to 1 the more weight is given to recent samples.

//Parameters for getAction()
float epsilon;           //epsilon is the probability of choosing an action randomly.  1-epsilon is the probability of choosing the optimal action

//Servo1 parameters
//This servo moves up with increased angle
const int numTheta1States = 6;
float theta1InitialAngle = 90.0;
float theta1Max = 100.0;                          //Can't exceed 110 or else arm will hit bar   //Need theta1Max and theta1Min between 0 and 180 
float theta1Min = 80.0;
float deltaTheta1 = (theta1Max - theta1Min)/(float(numTheta1States)-1.0);    //The change in servo1's angle when an action is performed on it
int s1 = int((theta1InitialAngle - theta1Min)/deltaTheta1);                  //This is an integer between zero and numTheta1States-1 used to index the state number of servo1
float delayTime1 = 4.5*deltaTheta1;                                 //The time in ms for the servo to move deltaTheta1

//Servo2 parameters
//This servo moves down with increased angle
const int numTheta2States = 6;
float theta2InitialAngle = 90.0;                
float theta2Max = 160.0;                        //Need theta2Max and theta2Min between 0 and 180 
float theta2Min = 90.0;
float deltaTheta2 = (theta2Max - theta2Min)/(float(numTheta2States)-1.0);    //The change in servo2's angle when an action is performed on it
int s2 = int((theta2InitialAngle - theta2Min)/deltaTheta2);                  //This is an integer between zero and numTheta2States-1 used to index the state number of servo2
float delayTime2 = 4.5*deltaTheta2;

//Initialize Q to zeros
const int numStates = numTheta1States*numTheta2States;
const int numActions = 4;
float Q[numStates][numActions];

//Initialize the state number. The state number is calculated using the theta1 state number and 
//the theta2 state number.  This is the row index of the state in the matrix Q. Starts indexing at 0.
int s = int(s1*numTheta2States + s2);
int sPrime = s;

//Initialize vars for getDeltaDistanceRolled()
float distanceNew = 0.0;
float distanceOld = 0.0;
float deltaDistance = 0.0;

//These get used in the main loop
float r = 0.0;
float lookAheadValue = 0.0;
float sample = 0.0;
int a = 0;

void setup(){
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo1.write(theta1InitialAngle);
  servo2.write(theta2InitialAngle);
  delay(5000); 
}

//Returns an action 0, 1, 2, or 3
int getAction(){
  int action;
  float valMax = -10000000.0;
  float val;
  int aMax;
  float randVal;
  int allowedActions[4] = {-1, -1, -1, -1};  //-1 if action of the index takes you outside the state space.  +1 otherwise
  boolean randomActionFound = false;
  
  //find the optimal action.  Exclude actions that take you outside the allowed-state space.
  if((s1 + 1) != numTheta1States){
    allowedActions[0] = 1;
    val = Q[s][0];
    if(val > valMax){
      valMax = val;
      aMax = 0;
    }
  }
  if(s1 != 0){
    allowedActions[1] = 1;
    val = Q[s][1];
    if(val > valMax){
      valMax = val;
      aMax = 1;
    }
  }
  if((s2 + 1) != numTheta2States){
    allowedActions[2] = 1;
    val = Q[s][2];
    if(val > valMax){
      valMax = val;
      aMax = 2;
    }
  }
  if(s2 != 0){
    allowedActions[3] = 1;
    val = Q[s][3];
    if(val > valMax){
      valMax = val;
      aMax = 3;
    }
  }
  
  //implement epsilon greedy
  randVal = float(random(0,101));
  if(randVal < (1.0-epsilon)*100.0){    //choose the optimal action with probability 1-epsilon
    action = aMax;
  }else{
    while(!randomActionFound){
      action = int(random(0,4));        //otherwise pick an action between 0 and 3 randomly (inclusive), but don't use actions that take you outside the state-space
      if(allowedActions[action] == 1){
        randomActionFound = true;
      }
    }
  }    
  return(action);
}

//Given a and the global(s) find the next state.  Also keep track of the individual joint indexes s1 and s2.
void setSPrime(int action){  
  if (action == 0){
    //joint1++
    sPrime = s + numTheta2States;
    s1++;
  }else if (action == 1){
    //joint1--
    sPrime = s - numTheta2States;
    s1--;
  }else if (action == 2){
    //joint2++
    sPrime = s + 1;
    s2++;
  }else{
    //joint2--
    sPrime = s - 1;
    s2--;
  }
}


//Update the position of the servos (this is the physical state transition command)
void setPhysicalState(int action){
  float currentAngle;
  float finalAngle;
  if (action == 0){
    currentAngle = servo1.read();
    finalAngle = currentAngle + deltaTheta1;
    servo1.write(finalAngle);
    delay(delayTime1);
  }else if (action == 1){
    currentAngle = servo1.read();
    finalAngle = currentAngle - deltaTheta1;
    servo1.write(finalAngle);
    delay(delayTime1);
  }else if (action == 2){
    currentAngle = servo2.read();
    finalAngle = currentAngle + deltaTheta2;
    servo2.write(finalAngle);
    delay(delayTime2);
  }else{
    currentAngle = servo2.read();
    finalAngle = currentAngle - deltaTheta2;
    servo2.write(finalAngle);
    delay(delayTime2);
  }
}


//Get the reward using the distance the agent has moved since the last call
float getDeltaDistanceRolled(){
  //get current distance
  distanceNew = float(ultrasonic.ping());   
  deltaDistance = distanceNew - distanceOld;
  if (abs(deltaDistance) < 57.0 || abs(deltaDistance) > 230.0){         //don't count noise
    deltaDistance = 0.0;
  }
  distanceOld = distanceNew;
  return deltaDistance;
}


//Get max over a' of Q(s',a'), but be careful not to look at actions which take the agent outside of the allowed state space
float getLookAhead(){
  float valMax = -100000.0;
  float val;
  if((s1 + 1) != numTheta1States){
    val = Q[sPrime][0];
    if(val > valMax){
      valMax = val;
    }
  }
  if(s1 != 0){
    val = Q[sPrime][1];
    if(val > valMax){
      valMax = val;
    }
  }
  if((s2 + 1) != numTheta2States){
    val = Q[sPrime][2];
    if(val > valMax){
      valMax = val;
    }
  }
  if(s2 != 0){
    val = Q[sPrime][3];
    if(val > valMax){
      valMax = val;
    }
  }
  return valMax;
}

void printQ(){
  for(int i=0; i<numStates; i++){
    for(int j=0; j<numActions; j++){
      Serial.print(Q[i][j]);
      Serial.print(" ");
    }
    Serial.println(" ");
  }
  Serial.println(" ");
}

void initializeQ(){
  for(int i=0; i<numStates; i++){
    for(int j=0; j<numActions; j++){
      Q[i][j] = 10.0;               //Initialize to a positive number to represent optimism over all state-actions
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////

const int rollDelay = 200;                   //allow time for the agent to roll after it sets its physical state
const float explorationMinutes = 1.0;        //the desired exploration time in minutes 
const float explorationConst = (explorationMinutes*60.0)/((float(rollDelay))/1000.0);  //this is the approximate exploration time in units of number of times through the loop

int t = 0;
void loop(){
  t++;
  epsilon = exp(-float(t)/explorationConst);
  a = getAction();           //a is beween 0 and 3
  setSPrime(a);              //this also updates s1 and s2.
  setPhysicalState(a);
  delay(rollDelay);                      //put a delay after the physical action occurs so the agent has time to move/roll before measuring the new position (before calling getDeltaDistanceRolled)
  r = getDeltaDistanceRolled();
  lookAheadValue = getLookAhead();
  sample = r + gamma*lookAheadValue;
  Q[s][a] = Q[s][a] + alpha*(sample - Q[s][a]);
  s = sPrime;
  
  if(t == 2){                //need to reset Q at the beginning since a spurious value arises at the first initialization (something from the rangefinder..)
    initializeQ();
  }
  
  //if(t == int(explorationConst)){
  //  printQ();
  //}
}
