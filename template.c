#include <kipr/wombat.h>
#include <math.h>
#include <time.h>

#define PI 3.14159265359


//PID CONSTANTS
#define kP 1.0
#define kI 0.0
#define kD 0.0


//RANGE CONSTANT FOR GO TO LINE
#define kR 0.01


//CONSTANT MODIFYING DISTANCES ON ROOMBA
#define kMod 0.01


//LINE SENSOR VALUES
double blackValueL = 0.0;
double blackValueR = 0.0;
double whiteValueL = 0.0;
double whiteValueR = 0.0;
//#define comp
//#define roomba


//NON ROOMBA PORTS
#define LEFT_WHEEL 0
#define RIGHT_WHEEL 0


//LIGHT SENSOR PORT NUMBER
#define LIGHT_SENSOR 0


//LEFT LINE SENSOR PORT NUMBER
#define LEFT_LINE_SENSOR 0


//RIGHT LINE SENSOR PORT NUMBER
#define RIGHT_LINE_SENSOR 0

double dabs(double x) {
  return (x >= 0) ? x : -x;
};

void move_left(double speed) {
  #ifdef roomba
    create_drive_direct(speed, 0);
  #else
    motor(LEFT_WHEEL, speed);
  #endif
}

void move_right(double speed) {
  #ifdef roomba
    create_drive_direct(0, speed);
  #else
    motor(RIGHT_WHEEL, speed);
  #endif
}

void move(double left_speed, double right_speed) {
  #ifdef roomba
    create_drive_direct(left_speed, right_speed);
  #else
    move_left(left_speed);
    move_right(right_speed);
  #endif
}

void stop_moving_left() {
  move_left(0.0);
}

void stop_moving_right() {
  move_right(0.0);
}

void stop_moving() {
  move(0.0, 0.0);
}

#ifdef roomba
void turn_angle(double power, double angle) {
  double goal_angle = get_create_total_angle()+angle;
  double nPower = (angle > 0) ? power : -power;
  move(nPower, -nPower);
  if(angle > 0) {
    while(goal_angle > get_create_total_angle()) {
    };
  } else {
    while(goal_angle < get_create_total_angle()) {
    };
  };
  stop_moving();
}
#endif

double PID_control(double error, double previous_error, double integral, double dt) {
  double p = kP * error;
  double i = kI * integral;
  double d = kD * (error - previous_error) / dt;
  return p+i+d;
};

void move_distance(double left_speed, double right_speed, double distance) {
  #ifdef roomba
  clock_t cClock = clock();
  double cDist = 0;
  double error = 0;
  int oAngle = get_create_total_angle();
  set_create_total_angle(0); //change angle
  int pRoombaDist = get_create_distance();
  double integral = 0;
  while(cDist < distance) {
    double angle = get_create_total_angle() * PI / 180;
    double changeDistance = get_create_distance() - pRoombaDist;
    changeDistance *= kMod;
    pRoombaDist = get_create_distance();
    cDist += changeDistance * cos(angle);
    double pError = error;
    error += changeDistance * sin(angle);
    double dt = ((double)(clock() - cClock)) / CLOCKS_PER_SEC;
    cClock = clock();
    integral += error * dt;
    double control = PID_control(error, pError, integral, dt);
    move(left_speed*(1.0+control),right_speed*(1.0-control));
  };
  set_create_total_angle(get_create_total_angle() + oAngle); //reset angle
  #else
  move(left_speed, right_speed);
  double aSpeed = left_speed + right_speed;
  double time = distance / aSpeed;
  msleep(1000*time);
  stop_moving();
  #endif
};

void go_to_line(double left_speed, double right_speed) {
  move(left_speed,right_speed);
  float t = 0.0;
  clock_t cClock = clock();

  //average calculation
  double whiteValueLTemp = 0.0;
  double whiteValueRTemp = 0.0;
  #define go_to_line_whiteValueCalcTime 0.04
  while(t <= go_to_line_whiteValueCalcTime) {
    double dt = ((double)(clock() - cClock)) / CLOCKS_PER_SEC;
    cClock = clock();
    double mL = analog(LEFT_LINE_SENSOR);
    double mR = analog(RIGHT_LINE_SENSOR);
    whiteValueLTemp += mL * dt;
    whiteValueRTemp += mR * dt;
    t += dt;
  };
  whiteValueL = whiteValueLTemp/t;
  whiteValueR = whiteValueRTemp/t;

  //standard deviation calculation
  double stDevL = 0.0;
  double stDevR = 0.0;
  double stDev = 0.0;
  t = 0.0;
  cClock = clock();
  #define go_to_line_stDevCalcTime 0.04
  while(t <= go_to_line_stDevCalcTime) {
    double dt = ((double)(clock() - cClock)) / CLOCKS_PER_SEC;
    cClock = clock();
    double sqNumL = (analog(LEFT_LINE_SENSOR)-whiteValueL);
    double sqNumR = (analog(RIGHT_LINE_SENSOR)-whiteValueR);
    double sqNum = sqNumL+sqNumR;
    stDevL += dt*sqNumL*sqNumL;
    stDevR += dt*sqNumR*sqNumR;
    stDev += dt*sqNum*sqNum;
    t += dt;
  };
  stDevL /= t;
  stDevR /= t;
  stDev /= t;

  //go to line
  t = 0.0;
  float integralError = 0.0;//this should be the integral of error dt
  cClock = clock();
  while(dabs(integralError) <= kR*stDev) {
    double dt = ((double)(clock() - cClock)) / CLOCKS_PER_SEC;
    cClock = clock();
    double valL = analog(LEFT_LINE_SENSOR);
    double valR = analog(RIGHT_LINE_SENSOR);
    t += dt;
    integralError += (valL + valR - whiteValueL - whiteValueR)*dt*Speed;
    integralError *= exp(-dt*Speed);
  };

  //black value calculation
  blackValueL = analog(LEFT_LINE_SENSOR);
  blackValueR = analog(RIGHT_LINE_SENSOR);
};

void follow_line(double speed, double dist) {
  #ifdef roomba
  #define follow_line_condition get_create_distance() * kMod< dist
  int initDist = get_create_distance();
  set_create_distance(0);
  #else
  #define follow_line_condition t<=dist/speed
  double t = 0.0;
  #endif
  clock_t cClock = clock();
  double pError = 0.0;
  double Integral = 0.0;
  while(follow_line_condition) {
    double dt = ((double)(clock() - cClock)) / CLOCKS_PER_SEC;
    cClock = clock();
    #ifndef roomba
    t += dt;
    #endif
    float lSense = analog(LEFT_LINE_SENSOR);
    float rSense = analog(RIGHT_LINE_SENSOR);
    if(lSense > blackValueL) {
      blackValueL = lSense;
    };
    if(rSense > blackValueR) {
      blackValueR = rSense;
    };
    if(lSense < whiteValueL) {
      whiteValueL = lSense;
    };
    if(rSense < whiteValueR) {
      whiteValueR = rSense;
    };
    double error = (lSense-whiteValueL)/(blackValueL-whiteValueL);
    error -= (rSense-whiteValueR)/(blackValueL-whiteValueR);
    Integral += error*dt;
    double control = PID_control(error,pError,Integral,dt);
    pError = error;
    move(speed*(1.0-control),speed*(1.0+control));
  };
  stop_moving();
  #undef follow_line_condition
  #ifdef roomba
  set_create_distance(initDist + get_create_distance());
  #endif
};

void code() {
  //WRITE YOUR CODE HERE
};

int main() {
  #ifdef roomba
  create_connect();
  #endif
  enable_servos();
  #ifdef comp
  wait_for_light(LIGHT_SENSOR);
  shut_down_in(119);
  #endif
  code();
  disable_servos();
  #ifdef roomba
  create_disconnect();
  #endif
  return 0;
};
