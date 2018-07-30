/*
 * Copyright 2018 Simbe Robotics, Inc.
 * Author: Steve Macenski (stevenmacenski@gmail.com)
 */

#include "ceres_solver.hpp"
#include <open_karto/Karto.h>

#include "ros/console.h"
#include <pluginlib/class_list_macros.h>

PLUGINLIB_EXPORT_CLASS(solver_plugins::CeresSolver, karto::ScanSolver)

namespace solver_plugins
{

/*****************************************************************************/
CeresSolver::CeresSolver() : nodes_(new std::unordered_map<int, Eigen::Vector3d>()),
                             problem_(new ceres::Problem()), was_constant_set_(false)
/*****************************************************************************/
{
  ros::NodeHandle nh("~");
  std::string solver_type, preconditioner_type, dogleg_type, trust_strategy, loss_fn;
  nh.getParam("ceres_linear_solver", solver_type);
  nh.getParam("ceres_preconditioner", preconditioner_type);
  nh.getParam("ceres_dogleg_type", dogleg_type);
  nh.getParam("ceres_trust_strategy", trust_strategy);
  nh.getParam("ceres_loss_function", loss_fn);

  corrections_.clear();
  first_node_ = nodes_->end();

  // formulate problem
  angle_local_parameterization_ = AngleLocalParameterization::Create();

  // choose loss function default squared loss (NULL)
  loss_function_ = NULL;
  if (loss_fn == "HuberLoss")
  {
    ROS_INFO("CeresSolver: Using HuberLoss loss function.");
    loss_function_ = new ceres::HuberLoss(0.7);
  }
  else if (loss_fn == "CauchyLoss")
  {
    ROS_INFO("CeresSolver: Using CauchyLoss loss function.");
    loss_function_ = new ceres::CauchyLoss(0.7);
  }

  // choose linear solver default CHOL
  options_.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
  if (solver_type == "SPARSE_SCHUR")
  {
    ROS_INFO("CeresSolver: Using SPARSE_SCHUR solver.");
    options_.linear_solver_type = ceres::SPARSE_SCHUR;
  }
  else if (solver_type == "ITERATIVE_SCHUR")
  {
    ROS_INFO("CeresSolver: Using ITERATIVE_SCHUR solver.");
    options_.linear_solver_type = ceres::ITERATIVE_SCHUR;
  }
  else if (solver_type == "CGNR")
  {
    ROS_INFO("CeresSolver: Using CGNR solver.");
    options_.linear_solver_type = ceres::CGNR;
  }

  // choose preconditioner default Jacobi
  options_.preconditioner_type = ceres::JACOBI;
  if (preconditioner_type == "IDENTITY")
  {
    ROS_INFO("CeresSolver: Using IDENTITY preconditioner.");
    options_.preconditioner_type = ceres::IDENTITY;
  }
  else if (preconditioner_type == "SCHUR_JACOBI")
  {
    ROS_INFO("CeresSolver: Using SCHUR_JACOBI preconditioner.");
    options_.preconditioner_type = ceres::SCHUR_JACOBI;
  }

  if (options_.preconditioner_type == ceres::CLUSTER_JACOBI || 
      options_.preconditioner_type == ceres::CLUSTER_TRIDIAGONAL)
  {
    //default canonical view is O(n^2) which is unacceptable for 
    // problems of this size
    options_.visibility_clustering_type = ceres::SINGLE_LINKAGE;
  }

  // choose trust region strategy default LM
  options_.trust_region_strategy_type = ceres::LEVENBERG_MARQUARDT;
  if (trust_strategy == "DOGLEG")
  {
    ROS_INFO("CeresSolver: Using DOGLEG trust region strategy.");
    options_.trust_region_strategy_type = ceres::DOGLEG;
  }

  // choose dogleg type default traditional
  if(options_.trust_region_strategy_type == ceres::DOGLEG)
  {
    options_.dogleg_type = ceres::TRADITIONAL_DOGLEG;
    if (dogleg_type == "SUBSPACE_DOGLEG")
    {
      ROS_INFO("CeresSolver: Using SUBSPACE_DOGLEG dogleg type.");
      options_.dogleg_type = ceres::SUBSPACE_DOGLEG;
    }
  }

  // a typical ros map is 5cm, this is 0.001, 50x the resolution
  options_.function_tolerance = 1e-3;
  options_.gradient_tolerance = 1e-6;
  options_.parameter_tolerance = 1e-3; 

  options_.sparse_linear_algebra_library_type = ceres::SUITE_SPARSE;
  options_.max_num_consecutive_invalid_steps = 3;
  options_.max_consecutive_nonmonotonic_steps = options_.max_num_consecutive_invalid_steps;
  options_.num_threads = 50;
  options_.use_nonmonotonic_steps = true;
  options_.jacobi_scaling = true;

  options_.min_relative_decrease = 1e-3;

  options_.initial_trust_region_radius = 1e4;
  options_.max_trust_region_radius = 1e8;
  options_.min_trust_region_radius = 1e-16;

  options_.min_lm_diagonal = 1e-6;
  options_.max_lm_diagonal = 1e32;

  if(options_.linear_solver_type == ceres::SPARSE_NORMAL_CHOLESKY)
  {
    options_.dynamic_sparsity = true;
  }
  return;
}

/*****************************************************************************/
CeresSolver::~CeresSolver()
/*****************************************************************************/
{
  if ( loss_function_ != NULL)
  {
    delete loss_function_;
  }
  if (nodes_ != NULL)
  {
    delete nodes_;
  }
  if (problem_ != NULL)
  {
    delete problem_;  
  }
  if (angle_local_parameterization_ != NULL)
  {
    delete angle_local_parameterization_;
  }
}

/*****************************************************************************/
void CeresSolver::Compute()
/*****************************************************************************/
{
  if (nodes_->size() == 0)
  {
    ROS_ERROR("CeresSolver: Ceres was called when there are no nodes."
              " This shouldn't happen.");
    return;
  }

  boost::mutex::scoped_lock lock(nodes_mutex_, boost::try_to_lock);
  if (!lock)
  {
    ROS_DEBUG("CeresSolver: Did not achieve lock to compute, "
              "someone else must be already optimizing "
              " it's okay, this isn't time critical.");
    return;
  }

  // populate contraint for static initial pose
  if (!was_constant_set_ && first_node_ != nodes_->end())
  {
    ROS_DEBUG("CeresSolver: Setting first node as a constant pose.");
    problem_->SetParameterBlockConstant(&first_node_->second(0));
    problem_->SetParameterBlockConstant(&first_node_->second(1));
    problem_->SetParameterBlockConstant(&first_node_->second(2));
    was_constant_set_ = !was_constant_set_;
  }

  const ros::Time start_time = ros::Time::now();
  ceres::Solver::Summary summary;
  ceres::Solve(options_, problem_, &summary);
  std::cout << summary.FullReport() << '\n';

  if (!summary.IsSolutionUsable())
  {
    ROS_WARN("CeresSolver: "
                          "Ceres could not find a usable solution to optimize.");
    return;
  }

  // store corrected poses
  if (!corrections_.empty())
  {
    corrections_.clear();
  }
  corrections_.reserve(nodes_->size());
  karto::Pose2 pose;
  const_graph_iterator iter = nodes_->begin();
  for ( iter; iter != nodes_->end(); ++iter )
  {
    pose.SetX(iter->second(0));
    pose.SetY(iter->second(1));
    pose.SetHeading(iter->second(2));
    corrections_.push_back(std::make_pair(iter->first, pose));
  }
  return;
}

/*****************************************************************************/
const karto::ScanSolver::IdPoseVector& CeresSolver::GetCorrections() const
/*****************************************************************************/
{
  return corrections_;
}

/*****************************************************************************/
void CeresSolver::Clear()
/*****************************************************************************/
{
  corrections_.clear();
}

/*****************************************************************************/
void CeresSolver::AddNode(karto::Vertex<karto::LocalizedRangeScan>* pVertex)
/*****************************************************************************/
{
  // store nodes
  karto::Pose2 pose = pVertex->GetObject()->GetCorrectedPose();
  Eigen::Vector3d pose2d(pose.GetX(), pose.GetY(), pose.GetHeading());

  const int id = pVertex->GetObject()->GetUniqueId();

  boost::mutex::scoped_lock lock(nodes_mutex_);
  nodes_->insert(std::pair<int,Eigen::Vector3d>(id,pose2d));

  if (nodes_->size() == 1)
  {
    first_node_ = nodes_->find(id);
  }
}

/*****************************************************************************/
void CeresSolver::AddConstraint(karto::Edge<karto::LocalizedRangeScan>* pEdge)
/*****************************************************************************/
{
  // get IDs in graph for this edge
  boost::mutex::scoped_lock lock(nodes_mutex_);
  const int node1 = pEdge->GetSource()->GetObject()->GetUniqueId();
  graph_iterator node1it = nodes_->find(node1);
  const int node2 = pEdge->GetTarget()->GetObject()->GetUniqueId();
  graph_iterator node2it = nodes_->find(node2);

  if (node1it == nodes_->end() || node2it == nodes_->end() || node1it == node2it)
  {
    ROS_WARN("CeresSolver: Failed to add constraint, could not find nodes.");
    return;
  }

  // extract transformation
  karto::LinkInfo* pLinkInfo = (karto::LinkInfo*)(pEdge->GetLabel());
  karto::Pose2 diff = pLinkInfo->GetPoseDifference();
  Eigen::Vector3d pose2d(diff.GetX(), diff.GetY(), diff.GetHeading());

  karto::Matrix3 precisionMatrix = pLinkInfo->GetCovariance().Inverse();
  Eigen::Matrix3d sqrt_information;
  sqrt_information(0,0) = precisionMatrix(0,0);
  sqrt_information(0,1) = sqrt_information(1,0) = precisionMatrix(0,1);
  sqrt_information(0,2) = sqrt_information(2,0) = precisionMatrix(0,2);
  sqrt_information(1,1) = precisionMatrix(1,1);
  sqrt_information(1,2) = sqrt_information(2,1) = precisionMatrix(1,2);
  sqrt_information(2,2) = precisionMatrix(2,2);

  // populate residual and parameterization for heading normalization
  ceres::CostFunction* cost_function = PoseGraph2dErrorTerm::Create(pose2d(0), 
                                       pose2d(1), pose2d(2), sqrt_information);
  problem_->AddResidualBlock( cost_function, loss_function_, 
                     &node1it->second(0), &node1it->second(1), &node1it->second(2),
                     &node2it->second(0), &node2it->second(1), &node2it->second(2));
  problem_->SetParameterization(&node1it->second(2), angle_local_parameterization_);
  problem_->SetParameterization(&node2it->second(2), angle_local_parameterization_);
  return;
}

/*****************************************************************************/
void CeresSolver::ModifyNode(const int& unique_id, Eigen::Vector3d pose)
/*****************************************************************************/
{
  boost::mutex::scoped_lock lock(nodes_mutex_);
  graph_iterator it = nodes_->find(unique_id);
  if (it != nodes_->end())
  {
    double yaw_init = it->second(2);
    it->second = pose;
    it->second(2)+= yaw_init;
  }
}

/*****************************************************************************/
void CeresSolver::GetNodeOrientation(const int& unique_id, double& pose)
/*****************************************************************************/
{
  boost::mutex::scoped_lock lock(nodes_mutex_);
  graph_iterator it = nodes_->find(unique_id);
  if (it != nodes_->end())
  {
    pose = it->second(2);
  }
}

/*****************************************************************************/
void CeresSolver::getGraph(std::vector<Eigen::Vector2d> &g)
/*****************************************************************************/
{
  boost::mutex::scoped_lock lock(nodes_mutex_);
  g.resize(nodes_->size());
  const_graph_iterator it = nodes_->begin();
  for (it; it!=nodes_->end(); ++it)
  {
    g[it->first] = Eigen::Vector2d(it->second(0), it->second(1));
  }
  return;
}

} // end namespace
