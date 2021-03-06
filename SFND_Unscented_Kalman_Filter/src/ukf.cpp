#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF()
{
  // Initialize UKF on first process measurement call
  is_initialized_ = false;

  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.5;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 2.0;

  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

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

  /**
   * End DO NOT MODIFY section for measurement noise values 
   */

  /**
   */

  // State dimension
  n_x_ = 5;

  // Augmented state dimension
  n_aug_ = 7;

  // Sigma point matrix without augmentation for noise
  Xsig_ = MatrixXd(n_x_, 2 * n_x_ + 1);

  // Augmented sigma point matrix
  Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  // predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

  // Weights of sigma points
  weights_ = VectorXd(2 * n_aug_ + 1);

  // Sigma point spreading parameter
  lambda_ = 3 - n_x_;

  // Measurement dimension radar
  n_z_radar_ = 3;

  // Measurement dimension lidar
  n_z_lidar_ = 2;

  // mean predicted measurement vector radar
  z_pred_r_ = VectorXd(n_z_radar_);

  // measurement covariance matrix S radar
  S_r_ = MatrixXd(n_z_radar_, n_z_radar_);

  // sigma point matrix in radar measurement dimension
  Zsig_radar = MatrixXd(n_z_radar_, 2 * n_aug_ + 1);

  // sigma point matrix in radar measurement dimension
  Zsig_lidar = MatrixXd(n_z_lidar_, 2 * n_aug_ + 1);

  // mean predicted measurement vector lidar
  z_pred_l_ = VectorXd(n_z_lidar_);

  // measurement covariance matrix S lidar
  S_l_ = MatrixXd(n_z_lidar_, n_z_lidar_);
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package)
{
  /**
   * Processes lidar and radar measurements
   * measurements.
   */
  if (!is_initialized_)
  {
    InitializeUKF(meas_package);
    return;
  }

  // Calculate time since last measurement in seconds
  double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;

  // Prediction
  Prediction(delta_t);

  // Update
  if (meas_package.sensor_type_ == MeasurementPackage::LASER)
  {
    PredictLidarMeasurement();
    UpdateLidar(meas_package);
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
  {
    PredictRadarMeasurement();
    UpdateRadar(meas_package);
  }
}

void UKF::InitializeUKF(MeasurementPackage meas_package)
{
  is_initialized_ = true;

  // set time of first measurement;
  time_us_ = meas_package.timestamp_;

  // initialize covariance matrix
  P_ << 1, 0, 0, 0, 0,
      0, 1, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 0.5, 0,
      0, 0, 0, 0, 0.5;

  // initialize state vector x with px and py from the first measurement.
  double px, py;
  if (meas_package.sensor_type_ == MeasurementPackage::LASER)
  {
    px = meas_package.raw_measurements_[0];
    py = meas_package.raw_measurements_[1];
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
  {
    double rho = meas_package.raw_measurements_[0]; // range in m
    double phi = meas_package.raw_measurements_[1]; // angle in rad

    px = cos(phi) * rho;
    py = sin(phi) * rho;
  }

  x_ << px, py, 0.0, 0.0, 0.0;

  // initialize weights
  double weight_0 = lambda_ / (lambda_ + n_aug_);
  weights_(0) = weight_0;

  for (int i = 1; i < (2 * n_aug_ + 1); ++i)
  {
    double weight = 0.5 / (n_aug_ + lambda_);
    weights_(i) = weight;
  }
}

void UKF::Prediction(double delta_t)
{
  /**
   * Estimates the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */
  GenerateAugmentedSigmaPoints();
  SigmaPointPrediction(delta_t);
  PredictMeanAndCovariance();
}

void UKF::GenerateAugmentedSigmaPoints()
{

  Xsig_aug_.fill(0.0);

  // create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);
  x_aug.fill(0.0);
  x_aug.head(n_x_) = x_;

  // create augmented covariance matrix
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(n_x_, n_x_) = std_a_ * std_a_;
  P_aug(n_x_ + 1, n_x_ + 1) = std_yawdd_ * std_yawdd_;

  // create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  // create augmented sigma points
  Xsig_aug_.col(0) = x_aug;

  for (int i = 0; i < n_aug_; ++i)
  {
    Xsig_aug_.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
    Xsig_aug_.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
  }
}

void UKF::SigmaPointPrediction(double delta_t)
{
  Xsig_pred_.fill(0.0);

  // predict sigma points
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    double p_x = Xsig_aug_(0, i);
    double p_y = Xsig_aug_(1, i);
    double v = Xsig_aug_(2, i);
    double yaw = Xsig_aug_(3, i);
    double yawd = Xsig_aug_(4, i);
    double nu_a = Xsig_aug_(5, i);
    double nu_yawdd = Xsig_aug_(6, i);

    // predicted location
    double px_p, py_p;

    // avoid division by zero
    if (fabs(yawd) > 0.001)
    {
      px_p = p_x + v / yawd * (sin(yaw + yawd * delta_t) - sin(yaw));
      py_p = p_y + v / yawd * (cos(yaw) - cos(yaw + yawd * delta_t));
    }
    else
    {
      px_p = p_x + v * delta_t * cos(yaw);
      py_p = p_y + v * delta_t * sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd * delta_t;
    double yawd_p = yawd;

    // add noise
    px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
    py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
    v_p = v_p + nu_a * delta_t;

    yaw_p = yaw_p + 0.5 * nu_yawdd * delta_t * delta_t;
    yawd_p = yawd_p + nu_yawdd * delta_t;

    // write predicted sigma point values into matrix
    Xsig_pred_(0, i) = px_p;
    Xsig_pred_(1, i) = py_p;
    Xsig_pred_(2, i) = v_p;
    Xsig_pred_(3, i) = yaw_p;
    Xsig_pred_(4, i) = yawd_p;
  }
}

void UKF::PredictMeanAndCovariance()
{
  // predicted state mean
  x_.fill(0.0);

  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }

  // predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    // angle normalization
    while (x_diff(3) > M_PI)
    {
      x_diff(3) -= 2. * M_PI;
    }
    while (x_diff(3) < -M_PI)
    {
      x_diff(3) += 2. * M_PI;
    }

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }
}

void UKF::PredictRadarMeasurement()
{

  Zsig_radar.fill(0.0);
  // transform sigma points into measurement space
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    // extract state vector values from sigma points
    double p_x = Xsig_pred_(0, i);
    double p_y = Xsig_pred_(1, i);
    double v = Xsig_pred_(2, i);
    double yaw = Xsig_pred_(3, i);

    double v1 = cos(yaw) * v;
    double v2 = sin(yaw) * v;

    // measurement model
    Zsig_radar(0, i) = sqrt(p_x * p_x + p_y * p_y);                         // rho
    Zsig_radar(1, i) = atan2(p_y, p_x);                                     // phi
    Zsig_radar(2, i) = (p_x * v1 + p_y * v2) / sqrt(p_x * p_x + p_y * p_y); // r_dot
  }

  // mean predicted measurement
  z_pred_r_.fill(0.0);
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    z_pred_r_ = z_pred_r_ + weights_(i) * Zsig_radar.col(i);
  }

  // measurement covariance matrix S
  S_r_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i)
  {
    // residual
    VectorXd z_diff = Zsig_radar.col(i) - z_pred_r_;

    // angle normalization
    while (z_diff(1) > M_PI)
    {
      z_diff(1) -= 2. * M_PI;
    }
    while (z_diff(1) < -M_PI)
    {
      z_diff(1) += 2. * M_PI;
    }

    S_r_ = S_r_ + weights_(i) * z_diff * z_diff.transpose();
  }

  // add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z_radar_, n_z_radar_);
  R << std_radr_ * std_radr_, 0, 0,
      0, std_radphi_ * std_radphi_, 0,
      0, 0, std_radrd_ * std_radrd_;
  S_r_ = S_r_ + R;
}

void UKF::PredictLidarMeasurement()
{

  Zsig_lidar.fill(0.0);
  // transform sigma points into measurement space
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    // extract state vector values from sigma points
    double p_x = Xsig_pred_(0, i);
    double p_y = Xsig_pred_(1, i);

    // measurement model
    Zsig_lidar(0, i) = p_x;
    Zsig_lidar(1, i) = p_y;
  }

  // mean predicted measurement
  z_pred_l_.fill(0.0);
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    z_pred_l_ = z_pred_l_ + weights_(i) * Zsig_lidar.col(i);
  }

  // measurement covariance matrix S
  S_l_.fill(0.0);
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    // residual
    VectorXd z_diff = Zsig_lidar.col(i) - z_pred_l_;

    S_l_ = S_l_ + weights_(i) * z_diff * z_diff.transpose();
  }

  // add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z_lidar_, n_z_lidar_);
  R << std_laspx_ * std_laspx_, 0,
      0, std_laspy_ * std_laspy_;

  S_l_ = S_l_ + R;
}
void UKF::UpdateLidar(MeasurementPackage meas_package)
{
  /**
   * Uses lidar data to update the belief 
   * about the object's position. Modifies the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */

  VectorXd z = VectorXd(n_z_lidar_);
  z << meas_package.raw_measurements_[0], // x position
      meas_package.raw_measurements_[1];  // y position

  // create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z_lidar_);

  // calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    // residual
    VectorXd z_diff = Zsig_lidar.col(i) - z_pred_l_;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  // Kalman gain K;
  MatrixXd K = Tc * S_l_.inverse();

  // residual
  VectorXd z_diff = z - z_pred_l_;

  // update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K * S_l_ * K.transpose();
}

void UKF::UpdateRadar(MeasurementPackage meas_package)
{
  /**
   * Uses radar data to update the belief 
   * about the object's position. Modifies the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

  // create vector for incoming radar measurement
  VectorXd z = VectorXd(n_z_radar_);
  z << meas_package.raw_measurements_[0], // rho in m
      meas_package.raw_measurements_[1],  // phi in rad
      meas_package.raw_measurements_[2];  // rho_dot in m/s

  // std::cout << "Update state z measurement = " << std::endl << z << std::endl;

  // create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z_radar_);

  // calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < (2 * n_aug_ + 1); ++i)
  {
    // residual
    VectorXd z_diff = Zsig_radar.col(i) - z_pred_r_;

    // angle normalization
    while (z_diff(1) > M_PI)
    {
      z_diff(1) -= 2. * M_PI;
    }
    while (z_diff(1) < -M_PI)
    {
      z_diff(1) += 2. * M_PI;
    }

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3) > M_PI)
    {
      x_diff(3) -= 2. * M_PI;
    }
    while (x_diff(3) < -M_PI)
    {
      x_diff(3) += 2. * M_PI;
    }

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  // Kalman gain K;
  MatrixXd K = Tc * S_r_.inverse();

  // residual
  VectorXd z_diff = z - z_pred_r_;

  //angle normalization
  while (z_diff(1) > M_PI)
  {
    z_diff(1) -= 2. * M_PI;
  }
  while (z_diff(1) < -M_PI)
  {
    z_diff(1) += 2. * M_PI;
  }

  // update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K * S_r_ * K.transpose();
}

const float UKF::CalculateNIS(const VectorXd &z_prediction, const VectorXd &z_measurement, const MatrixXd &covariance)
{
  VectorXd difference{z_measurement - z_prediction};
  return difference.transpose() * covariance.inverse() * difference;
}