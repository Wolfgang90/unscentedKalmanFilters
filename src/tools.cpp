#include <iostream>
#include "tools.h"
#include "math.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using std::vector;

Tools::Tools() {}

Tools::~Tools() {}

VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
                              const vector<VectorXd> &ground_truth) {
  /**
  TODO:
    * Calculate the RMSE here.
  */
  /**
    * RMSE-Formula: RMSE = sqrt(1/n * sum[from 1 to n](x_est - x_true)**2) 
  */

  assert(estimations.size() == ground_truth.size() || estimations.size() == 0 && "Estimation and ground truth vector do not have the same size");

  //initialized required values for formula
  VectorXd rmse(4);
  rmse << 0,0,0,0;
  double n = estimations.size();
  
  for (int i = 0; i < n; i++) {
    VectorXd residual = estimations[i] - ground_truth[i];

    //coefficient-wise multiplication
    residual = residual.array()*residual.array();
    rmse += residual;
    }

  rmse = (rmse / n);
  rmse = rmse.array().sqrt();

  return rmse;
}
