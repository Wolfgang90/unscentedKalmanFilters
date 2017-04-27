#define _USE_MATH_DEFINES

#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <cmath>
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {

  is_initialized_ = false;

  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;
  
  n_x_ = 5;

  n_aug_ = n_x_ + 2;

  // initial state vector
  x_ = VectorXd(n_x_);

  // initial covariance matrix
  P_ = MatrixXd::Identity(n_x_, n_x_);
  P_(0,0) = 0.5;
  P_(1,1) = 0.5;

  //create matrix with predicted sigma points as columns
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ +1);
  Xsig_pred_.fill(0.0);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.8;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.6;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  lambda_ = 3 - n_aug_;

  weights_ = VectorXd(2*n_aug_ + 1);
  
  //set weights
  double weight_0 = lambda_ / (lambda_ + n_aug_);
  weights_(0) = weight_0;
  for (int i = 1; i < 2 * n_aug_ + 1; i++) {
    double weight = 0.5 / (n_aug_ + lambda_);
    weights_(i) = weight;

  //measurement noise covariance matrix radar
  R_radar_ = MatrixXd(3,3);
  R_radar_ << std_radr_ * std_radr_, 0,                         0,
              0,                     std_radphi_ * std_radphi_, 0,
              0,                     0,                         std_radrd_ * std_radrd_;

  //measurement noise covariance matrix laser
  R_laser_ = MatrixXd(2,2);
  R_laser_ << std_laspx_ * std_laspx_, 0,
              0,                       std_laspy_ * std_laspy_;
  }

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if(!is_initialized_) {

    if(meas_package.sensor_type_ == MeasurementPackage::LASER){
      
      double px = meas_package.raw_measurements_[0];
      double py = meas_package.raw_measurements_[1];

      x_ << px, py, 0, 0, 0;

    } else if (meas_package.sensor_type_ == MeasurementPackage::RADAR){

      double ro = meas_package.raw_measurements_[0];
      double phi = meas_package.raw_measurements_[1];

      double px = cos(phi) * ro;
      double py = sin(phi) * ro;
 
      if(fabs(px) < 0.0001) {
        px = 0.0001;
      }

      if(fabs(py) < 0.0001) {
        py = 0.0001;
      }

      x_ << px, py, 0, 0, 0;
    }



    time_us_ = meas_package.timestamp_;

    is_initialized_ = true;
    return;
  }

  /**
  * Prediction
  */
  double dt = (meas_package.timestamp_ - time_us_)/ 1000000.0; //dt - expressed in seconds
  
  time_us_ = meas_package.timestamp_;

  Prediction(dt);  

  /**
  * Update
  */
  if (meas_package.sensor_type_ == MeasurementPackage::LASER){
    if (use_laser_) UpdateLidar(meas_package);
  } else if (meas_package.sensor_type_ == MeasurementPackage::RADAR){
    if (use_radar_) UpdateRadar(meas_package);
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  /**
  Generate matrix with augmented sigma points
  */
  
  //create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);
  x_aug.fill(0.0);

  //crate augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  P_aug.fill(0.0);

  //create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  //create augmented mean state
  x_aug.head(n_x_) = x_;

  //create augmented covariance matrix
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(n_x_, n_x_) = std_a_ * std_a_;
  P_aug(n_x_ + 1, n_x_ + 1) = std_yawdd_ * std_yawdd_;

  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  //create augmented sigma points
  Xsig_aug.col(0) = x_aug;
  for (int i = 0; i < n_aug_; i++) {
    Xsig_aug.col(i+1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
  }

  /**
  Generate matrix with sigma point prediction
  */

  //predict sigma points
  for (int i = 0; i < 2 * n_aug_ +1; i++){
    
    //extract values for better readability
    const double p_x = Xsig_aug(0,i);
    const double p_y = Xsig_aug(1,i);
    const double v = Xsig_aug(2,i);
    const double yaw = Xsig_aug(3,i);
    const double yawd = Xsig_aug(4,i);
    const double nu_a = Xsig_aug(5,i);
    const double nu_yawdd = Xsig_aug(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001){
      px_p = p_x + v/yawd * (sin(yaw + yawd * delta_t) - sin(yaw));
      py_p = p_y + v/yawd * (cos(yaw) - cos(yaw + yawd * delta_t));
    }else{
      px_p = p_x + v * delta_t * cos(yaw);
      py_p = p_y + v * delta_t * sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd * delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
    py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
    v_p = v_p + nu_a * delta_t;

    yaw_p = yaw_p + 0.5 * nu_yawdd * delta_t * delta_t;
    yawd_p = yawd_p + nu_yawdd * delta_t;
    
    //Write predicted sigma points to Xsig_pred_
    Xsig_pred_(0,i) =px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }

  
  /**
  Predict mean and covariance
  */


  
  //predicted state mean
  x_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ +1; i++) { //iterate over sigma points
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }

  //predict state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){ //iterate over sigma points

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    normalizeAngle(&x_diff(3));

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }
}


/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

#include "measurement_package.h"
  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

  //laser measurement dimensions
  int n_z = 2;

  /**
  * Predict measurement
  */

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = Xsig_pred_.topRows(n_z); //Extracts the px and py rows from Xsig_pred_

  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){
    VectorXd z_diff = Zsig.col(i) - z_pred;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  S = S + R_laser_;


  /**
  * Update state
  */

  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //create cross correlation matrix
  Tc.fill(0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z = meas_package.raw_measurements_;
  VectorXd z_diff = z -z_pred;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K * S * K.transpose();

  NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  //radar measurement dimensions
  int n_z = 3;

  /**
  * Predict measurement
  */

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ +1; i++) {
    
    //extract values for better readability
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw) * v;
    double v2 = sin(yaw) * v;

    //measurement model
    Zsig(0,i) = sqrt(p_x * p_x + p_y * p_y); //r
    Zsig(1,i) = atan2(p_y, p_x); //phi
    Zsig(2,i) = (p_x * v1 + p_y * v2) / sqrt(p_x * p_x + p_y * p_y); //r_dot
  }

  
  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    normalizeAngle(&z_diff(1));

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  S = S + R_radar_;
  

  /**
  * Update state
  */
  
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);

  //calculate cross correlation matrix
  Tc.fill(0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    normalizeAngle(&z_diff(1));

    //state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    normalizeAngle(&x_diff(3));

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }
  
  //Kalman gain K
  MatrixXd K = Tc * S.inverse();

  //redidual
  VectorXd z = meas_package.raw_measurements_;
  VectorXd z_diff = z - z_pred;

  //angle normalization
  normalizeAngle(&z_diff(1));

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K * S * K.transpose();

  NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;
}

void UKF::normalizeAngle(double *angle) {
  while (*angle > M_PI) *angle -= 2. * M_PI;
  while (*angle < -M_PI) *angle += 2. * M_PI;
  /*
  if (fabs(*angle) > M_PI){
    *angle = fmod(*angle, M_PI);
  }
  */
}

