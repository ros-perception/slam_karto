#include "spa_graph_visualizer.h"
#include "ros/console.h"
#include <pluginlib/class_list_macros.h>

namespace karto_plugins{

SPAGraphVisualizer::SPAGraphVisualizer():marker_count_(0),visualizer_initialized_(false)
{
}

void SPAGraphVisualizer::initialize(const boost::shared_ptr<karto::ScanSolver>& solver)
{
  solver_ = boost::dynamic_pointer_cast < karto_plugins::SPASolver > (solver);
  if (solver_ == NULL) {
    ROS_ERROR("Could not initialize SPAGraphVisualizer. Check specified solver, this visualizer is only compatible with the SPA solver.");
    return;
  }
  visualizer_initialized_ = true;
}

visualization_msgs::MarkerArray SPAGraphVisualizer::createVisualizationMarkers()
{
  if(!visualizer_initialized_)
    return visualization_msgs::MarkerArray();

  visualization_msgs::MarkerArray marray;
  std::vector<float> graph;
  solver_->getGraph(graph);

  visualization_msgs::Marker m;
  m.header.frame_id = map_frame_id_;
  m.header.stamp = ros::Time::now();
  m.id = 0;
  m.ns = "karto";
  m.type = visualization_msgs::Marker::SPHERE;
  m.pose.position.x = 0.0;
  m.pose.position.y = 0.0;
  m.pose.position.z = 0.0;
  m.scale.x = 0.1;
  m.scale.y = 0.1;
  m.scale.z = 0.1;
  m.color.r = 1.0;
  m.color.g = 0;
  m.color.b = 0.0;
  m.color.a = 1.0;
  m.lifetime = ros::Duration(0);

  visualization_msgs::Marker edge;
  edge.header.frame_id = map_frame_id_;
  edge.header.stamp = ros::Time::now();
  edge.action = visualization_msgs::Marker::ADD;
  edge.ns = "karto";
  edge.id = 0;
  edge.type = visualization_msgs::Marker::LINE_STRIP;
  edge.scale.x = 0.1;
  edge.scale.y = 0.1;
  edge.scale.z = 0.1;
  edge.color.a = 1.0;
  edge.color.r = 0.0;
  edge.color.g = 0.0;
  edge.color.b = 1.0;

  m.action = visualization_msgs::Marker::ADD;
  uint id = 0;
  for (uint i=0; i<graph.size()/2; i++)
  {
    m.id = id;
    m.pose.position.x = graph[2*i];
    m.pose.position.y = graph[2*i+1];
    marray.markers.push_back(visualization_msgs::Marker(m));
    id++;

    if(i>0)
    {
      edge.points.clear();

      geometry_msgs::Point p;
      p.x = graph[2*(i-1)];
      p.y = graph[2*(i-1)+1];
      edge.points.push_back(p);
      p.x = graph[2*i];
      p.y = graph[2*i+1];
      edge.points.push_back(p);
      edge.id = id;

      marray.markers.push_back(visualization_msgs::Marker(edge));
      id++;
    }
  }

  m.action = visualization_msgs::Marker::DELETE;
  for (; id < marker_count_; id++)
  {
    m.id = id;
    marray.markers.push_back(visualization_msgs::Marker(m));
  }

  marker_count_ = marray.markers.size();
  return marray;
}

} // namespace karto_plugins

PLUGINLIB_EXPORT_CLASS(karto_plugins::SPAGraphVisualizer, karto::GraphVisualizer)
