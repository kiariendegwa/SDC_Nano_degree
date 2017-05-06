#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
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

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 30;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 30;

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

  //set state dimension
  n_x_ = 5;

  //set augmented dimension
  n_aug_ = 7;

  //define spreading parameter
  lambda_ = 3 - n_aug_;

  ///* the current NIS for radar
  NIS_radar_ = 0;

  ///* the current NIS for laser
  NIS_laser_ = 0;

  double weight_0 = lambda_/(lambda_+n_aug_);
  double weight_remaining = 0.5/(lambda_+n_aug_);
  weights_ = VectorXd::Zero(15);
  weights_ =  weights_.array() + weight_remaining;
  weights_(0) = weight_0;

  //add measurement noise covariance matrix
  R_radar_ = MatrixXd(3,3);
  R_radar_ << std_radr_*std_radr_, 0, 0,
              0, std_radphi_*std_radphi_, 0,
              0, 0,std_radrd_*std_radrd_;

  R_laser_ = MatrixXd(2,2);
  R_laser_ << std_laspx_*std_laspx_, 0,
              0, std_laspy_*std_laspy_;

  H_laser_ = MatrixXd(2,5);
  H_laser_ << 1, 0, 0, 0, 0,
              0, 1, 0, 0, 0;

  Xsig_pred_ = MatrixXd(5, 15);


  ///* Sigma point prediction
  MatrixXd Xsig_pred_;
  MatrixXd Zsig_pred_;
}

UKF::~UKF(){}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage measurement_pack) {
  if (!is_initialized_){
    if (measurement_pack.sensor_type_ == MeasurementPackage::LASER) {
      x_ <<measurement_pack.raw_measurements_(0), measurement_pack.raw_measurements_(1), 0.0, 0.0, 0.0;
    }
    else if(measurement_pack.sensor_type_ == MeasurementPackage::RADAR)
    {
      double rho = measurement_pack.raw_measurements_(0);
      double phi = measurement_pack.raw_measurements_(1);
      double rho_dot = measurement_pack.raw_measurements_(2);

      double px = rho*cos(phi);
      double py = rho*sin(phi);
      double vx = rho_dot * cos(phi);
      double vy = rho_dot * sin(phi);

      x_ << px, py, 0.0, 0.0, 0.0;
    }
    previous_timestamp_ = measurement_pack.timestamp_;

   // done initializing, no need to predict or update
   is_initialized_ = true;
   return;
  }

  float delta_t = (measurement_pack.timestamp_ - previous_timestamp_) / 1.0e6;	//dt - expressed in seconds

  //Run prediction.
  Prediction(delta_t);

  //Run update step
  Update(measurement_pack);
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

  //1. Generate sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  //create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);
  //create augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

  x_aug.head(n_x_) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  //create augmented covariance matrix
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(n_x_,n_x_) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  //create square root matrix
  MatrixXd A = P_aug.llt().matrixL();
  //create augmented sigma points
  Xsig_aug.col(0) = x_aug;

  //create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * A.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * A.col(i);
  }
  // ~~~

  //2. Predict Sigma points
 for (int i = 0; i< 2*n_aug_+1; i++)
 {
   //extract values for better readability
   double p_x = Xsig_aug(0,i);
   double p_y = Xsig_aug(1,i);
   double v = Xsig_aug(2,i);
   double yaw = Xsig_aug(3,i);
   double yawd = Xsig_aug(4,i);
   double nu_a = Xsig_aug(5,i);
   double nu_yawdd = Xsig_aug(6,i);

   //predicted state values
   double px_p, py_p;

   //avoid division by zero
   if (fabs(yawd) > 0.001) {
       px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
       py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
   }
   else {
       px_p = p_x + v*delta_t*cos(yaw);
       py_p = p_y + v*delta_t*sin(yaw);
   }

   double v_p = v;
   double yaw_p = yaw + yawd*delta_t;
   double yawd_p = yawd;

   //add noise
   px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
   py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
   v_p = v_p + nu_a*delta_t;

   yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
   yawd_p = yawd_p + nu_yawdd*delta_t;

   //write predicted sigma point into right column
   Xsig_pred_(0,i) = px_p;
   Xsig_pred_(1,i) = py_p;
   Xsig_pred_(2,i) = v_p;
   Xsig_pred_(3,i) = yaw_p;
   Xsig_pred_(4,i) = yawd_p;
   // ~~~

   //3. Predict State covariance matrix

   //create vector for predicted state
   VectorXd x = VectorXd(n_x_);

   //create covariance matrix for prediction
   MatrixXd P = MatrixXd(n_x_, n_x_);

   //predicted state mean
   x.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
     x = x+ weights_(i) * Xsig_pred_.col(i);
   }

   //predicted state covariance matrix
   P.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

     //state difference
     VectorXd x_diff = Xsig_pred_.col(i) - x;
     //angle normalization
     while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
     while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

     P += weights_(i) * x_diff * x_diff.transpose() ;
   }
 }
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(const MatrixXd& Xsig_pred,
                      const MatrixXd& Zsig,
                      const VectorXd& z_pred,
                      const MatrixXd& S,
                      MeasurementPackage meas_package){
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd::Zero(5, 2);

  //calculate cross correlation matrix
  for (int i = 0; i < 15; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    x_diff(3) = atan2(sin(x_diff(3)), cos(x_diff(3)));

    Tc += weights_(i) * x_diff * z_diff.transpose();
  }

  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = meas_package.raw_measurements_ - z_pred;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(const MatrixXd& Xsig_pred,
                      const MatrixXd& Zsig,
                      const VectorXd& z_pred,
                      const MatrixXd& S,
                      MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd::Zero(5, 3);

  //calculate cross correlation matrix
  for (int i = 0; i < 15; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    z_diff(1) = atan2(sin(z_diff(1)), cos(z_diff(1)));

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    x_diff(3) = atan2(sin(x_diff(3)), cos(x_diff(3)));

    Tc += weights_(i) * x_diff * z_diff.transpose();
  }

  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = meas_package.raw_measurements_ - z_pred;

  //angle normalization
  z_diff(1) = atan2(sin(z_diff(1)), cos(z_diff(1)));

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
}

void UKF::Update(MeasurementPackage meas_package){
  if (meas_package.sensor_type_==MeasurementPackage::LASER){
    VectorXd z_pred = VectorXd(3);
    MatrixXd Zsig_pred = MatrixXd(3,15);
    MatrixXd S = MatrixXd(3,3);
    PredictLidarMeasurement(Xsig_pred_, &z_pred, &S, &Zsig_pred);
    UpdateLidar(Xsig_pred_,Zsig_pred, z_pred, S, meas_package);
  }else {
    VectorXd z_pred = VectorXd(2);
    MatrixXd Zsig_pred = MatrixXd(2,15);
    MatrixXd S = MatrixXd(2,2);
    PredictRadarMeasurement(Xsig_pred_, &z_pred, &S, &Zsig_pred);
    UpdateRadar(Xsig_pred_,Zsig_pred, z_pred, S, meas_package);
  }
}

void UKF::PredictLidarMeasurement(const MatrixXd& Xsig_pred_,
                                  VectorXd* z_out,
                                  MatrixXd* S_out,
                                  MatrixXd* Zsig_out){
  MatrixXd Zsig_pred = MatrixXd(2, 15);

  Zsig_pred = H_laser_ * Xsig_pred_;

  //mean predicted measurement
  VectorXd z_pred = VectorXd::Zero(2);
  z_pred = Zsig_pred * weights_;

  MatrixXd S = MatrixXd::Zero(2,2);
  for (int i = 0; i < 15; i++) {  //2n+1 simga points
  //residual
    VectorXd z_diff = Zsig_pred.col(i) - z_pred;
    S += weights_(i) * z_diff * z_diff.transpose();
  }

  S += R_laser_;

  *z_out = z_pred;
  *S_out = S;
  *Zsig_out = Zsig_pred;
}

void UKF::PredictRadarMeasurement(const MatrixXd& Xsig_pred_,
                                  VectorXd* z_out,
                                  MatrixXd* S_out,
                                  MatrixXd* Zsig_out){

  MatrixXd Zsig_pred = MatrixXd(3, 15);

  //transform sigma points into measurement space
  for (int i = 0; i < 15; i++) {
    // extract values for better readibility
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    // measurement model
    Zsig_pred(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig_pred(1,i) = atan2(p_y,p_x);                                 //phi
    Zsig_pred(2,i) = (Zsig_pred(0,i) < 1e-4) ? 0.0 : (p_x*v1 + p_y*v2 )/sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }

  //mean predicted measurement
  VectorXd z_pred = VectorXd::Zero(3);
  z_pred = Zsig_pred * weights_;

//  MatrixXd Zd = Zsig_pred.colwise() - z_pred;
//  MatrixXd Zdw = Zd*weights_.asDiagonal();
//  MatrixXd S = Zdw * Zd.transpose();

  //measurement covariance matrix S
  MatrixXd S = MatrixXd::Zero(3,3);
  for (int i = 0; i < 15; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_pred.col(i) - z_pred;
    //angle normalization
    z_diff(1) = atan2(sin(z_diff(1)), cos(z_diff(1)));

    S += weights_(i) * z_diff * z_diff.transpose();
  }

  S += R_radar_;

  *z_out = z_pred;
  *S_out = S;
  *Zsig_out = Zsig_pred;
}