/// file: kalman_filter_1d_demo.cpp
///
/// This is a simple 1D Kalman filter demo.
///
/// The goal is to estimate the 1D position of a 1D car.
/// (where is it on a line?)
///
/// The car tries to move 3m forward in each step.
/// However, it is very windy and there is much randomness
/// in this movement process (--> high process noise).
///
/// Further, we have only a very noisy 1D position sensor
/// to measure the 1D position of the car (--> high
/// measurement noise).
///
/// However, the 1D Kalman filter can give us a fairly good
/// estimate of the 1D position of the car by using
/// a weighted sum of the predicted position and the measured
/// position.
///
/// Note that the predicted position will be weighted by
/// the measurement noise: the higher the measurement noise,
/// the more we belief the prediction. The measurement will
/// be weighted by the current uncertainty about the prediction:
/// the higher the uncertainty about the prediction, the more
/// we belief the measurement. This makes sense!
///
/// ---
/// by Prof. Dr. J�rgen Brauer, www.juergenbrauer.org

#define VISUALIZATION false

#include "opencv2/opencv.hpp"

#include <iostream>
#include <conio.h>
#include <random>   // for random numbers & normal distribution
#include <time.h>

#define _USE_MATH_DEFINES
#include <math.h>   // for M_PI

#include "kalman_filter_1d.h"

#define IMG_WIDTH  800
#define IMG_HEIGHT 800
#define LINE_WIDTH 3
#define COL_TIME_AXIS      CV_RGB(255,255,255)   // WHITE
#define COL_GT_POS         CV_RGB(255,255,255)   // WHITE
#define COL_NAIVE_EST_POS  CV_RGB(255,255,0)     // YELLOW
#define COL_MEASUREMENT    CV_RGB(255,0,0)       // RED
#define COL_KF_EST_POS     CV_RGB(0,255,0)       // GREEN
#define COL_KF_UNCERTAINTY CV_RGB(0,255,255)     // CYAN


using namespace cv;


Mat image;


void draw_gaussian(int anchor_x, int anchor_y, double �, double sigma, Scalar col)
{
  const double visualization_scale = 5000.0;

  int prev_draw_x = -1;
  int prev_draw_y = -1;
  for (double x = (int)� - IMG_WIDTH; x <= (int)� + IMG_WIDTH; x++)
  {
    double y = 1.0 / (sigma * sqrt(2*M_PI)) * exp(-0.5 * pow((x - �) / sigma, 2.0));

    y = y * visualization_scale;

    int draw_x = anchor_x + (int)x;
    int draw_y = anchor_y - (int)y;

    // valid coords?
    if ((draw_x < 0) || (draw_x >= IMG_WIDTH) || (draw_y < 0) || (draw_y >= IMG_HEIGHT))
      continue;

    // 1st loop?
    if ((prev_draw_x == -1) && (prev_draw_y == -1))
    {
      prev_draw_x = draw_x;
      prev_draw_y = draw_y;
      continue;
    }

    line(image, Point(prev_draw_x, prev_draw_y), Point(draw_x, draw_y), col, 1, LINE_AA);

    prev_draw_x = draw_x;
    prev_draw_y = draw_y;    

  } // for all x coords to draw

} // draw_gaussian



void show_ground_truth_pos_vs_estimated_pos(double gt_pos,
                                            double naive_est_pos,
                                            double measurement,
                                            double kf_est_pos,
                                            double kf_uncertainty,
                                            double error_measurement,
                                            double error_naive,
                                            double error_kf)
{
  const int SPACE = 10;
  int h2 = (IMG_HEIGHT-SPACE) / 2;
  int h3 = (IMG_HEIGHT-SPACE) / 3;
  int h4 = (IMG_HEIGHT-SPACE) / 4;
  

  // 1. clear visualization image
  image = 0;
  
  // 2. draw time axis 4 times
  for (int i = 1; i < 4; i++)
    line(image, Point(0, i*h4), Point(IMG_WIDTH - 1, i*h4), COL_TIME_AXIS, LINE_WIDTH);

  // 3. show info what we display in which section
  char txt[100];  
  sprintf_s(txt, "Ground truth pos");
  putText(image, txt, Point(20, (int)(0.5*h4)), FONT_HERSHEY_SIMPLEX, 0.7, COL_GT_POS,        1);

  sprintf_s(txt, "Measured pos: error = %.2f", error_measurement);
  putText(image, txt, Point(20, (int)(1.5*h4)), FONT_HERSHEY_SIMPLEX, 0.7, COL_MEASUREMENT,   1);

  sprintf_s(txt, "Naive estimated pos: error = %.2f", error_naive);
  putText(image, txt, Point(20, (int)(2.5*h4)), FONT_HERSHEY_SIMPLEX, 0.7, COL_NAIVE_EST_POS, 1);

  sprintf_s(txt, "Kalman filtered pos: error = %.2f", error_kf);
  putText(image, txt, Point(20, (int)(3.5*h4)), FONT_HERSHEY_SIMPLEX, 0.7, COL_KF_EST_POS, 1);


  // 4. draw ground truth position
  line(image, Point((int)gt_pos, 0), Point((int)gt_pos, 1*h4), COL_GT_POS, LINE_WIDTH);
  
  // 5. draw measurement
  line(image, Point((int)measurement, 1*h4), Point((int)measurement, 2*h4), COL_MEASUREMENT, LINE_WIDTH);
  
  // 6. draw naive estimated position
  line(image, Point((int)naive_est_pos, 2*h4), Point((int)naive_est_pos, 3*h4), COL_NAIVE_EST_POS, LINE_WIDTH);

  // 7. draw Kalman filter estimated position
  line(image, Point((int)kf_est_pos, 3*h4), Point((int)kf_est_pos, 4*h4), COL_KF_EST_POS, LINE_WIDTH);

  // 8. visualize Kalman filter uncertainty by drawing the corresponding Gaussian
  //draw_gaussian((int)kf_est_pos, 4 * h4, 0.0, kf_uncertainty, COL_KF_UNCERTAINTY);

  
  // 9. show visualization image
  imshow("Kalman Filter 1D Demo : {Ground truth | measured | naive estimated | Kalman filtered} position of car", image);

} // show_ground_truth_pos_vs_estimated_pos



int main()
{
  srand( (unsigned int) time(NULL) ) ;

  // create visualization image
  image = Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC3);

  double �                 = 5.0;
  double sigma             = 0.75;

  // Try combinations (10,10) (50,10) (10,50) (50,50)
  double process_noise     = 10.0;  // EXPERIMENT HERE: make procecss noise larger or smaller! 
  double measurement_noise = 10.0;  // EXPERIMENT HERE: make measurement noise larger or smaller!


  double �_naive_est = �;
  
  kalman_filter_1d myFilter(�, sigma, process_noise, 1.0*measurement_noise);

  // prepare random number generator for process noise
  std::default_random_engine generator;
  std::normal_distribution<double> rnd_distribution_process_noise(0.0, process_noise);

  // prepare random number generator for measurement noise
  std::default_random_engine generator2;
  std::normal_distribution<double> rnd_distribution_measurement_noise(0.0, measurement_noise);

  int simulation_step = 0;  
  double error_measurement, error_naive, error_kf;
  error_measurement = error_naive = error_kf = 0.0;
  printf("initial sigma = %.2f\n", myFilter.get_current_uncertainty());

  while (simulation_step<100000)
  {
    // 1. define control signal: 3m forward
    double u = 3.0;

    // 2. update naive state estimate
    �_naive_est = �_naive_est + u;

    // 3. simulate new (ground truth) state, i.e., update state: control signal + Gaussian noise
    // 
    //    how to generate Gaussian noise?
    //    see http://www.cplusplus.com/reference/random/normal_distribution/
    //    for a nice and short example
    � = � + u + rnd_distribution_process_noise( generator );

    // 4. simulate sensor data
    double z = � + rnd_distribution_measurement_noise(generator2);

    // 5. predict new state (1st step of an 1D Kalman filter)
    myFilter.predict( u );

    // 6. correct new estimated state by help of sensor data (2nd step of an 1D Kalman filter)
    myFilter.correct_by_measurement( z );

    // 7. update moving average of ground truth pos vs.
    //    measured, naive estimated, Kalman filtered 1D pos estimate
    double N = (double)simulation_step;
    error_measurement = (error_measurement*N + abs(z - �))                                     / (N+1.0);
    error_naive       = (error_naive      *N + abs(�_naive_est - �))                           / (N+1.0);
    error_kf          = (error_kf         *N + abs(myFilter.get_current_state_estimate() - �)) / (N+1.0);

    // 8. visualize ground truth vs. estimated state
    if (VISUALIZATION)
    {
       system("cls");
       printf("Simulation step : %d\n", simulation_step);
       
       show_ground_truth_pos_vs_estimated_pos(�,
          �_naive_est,
          z,
          myFilter.get_current_state_estimate(),
          myFilter.get_current_uncertainty(),
          error_measurement,
          error_naive,
          error_kf
          );

       // 10. wait for user input
       printf("KF uncertainty: %.10f\n", myFilter.get_current_uncertainty());
       printf("Press any key to go next simulation step!\n");
       int c = waitKey();
       if (c == 29) // ESC pressed?
          break;
    }

    // 9. time goes by...
    simulation_step++;

  } // while

  printf("Error of KF after %d steps: %.2f\n",
     simulation_step, error_kf);

  printf("sigma at end = %.2f\n", myFilter.get_current_uncertainty());

  _getch();

} // main