#ifndef SPA_GRAPH_VISUALIZER_H
#define SPA_GRAPH_VISUALIZER_H

#include <slam_karto/graph_visualizer.h>
#include <slam_karto/spa_solver.h>

namespace karto_plugins {

/*
 * Graph visualizer plugin for the SPA solver
 */
class SPAGraphVisualizer : public karto::GraphVisualizer
{
 public:
  SPAGraphVisualizer();
  void initialize(const boost::shared_ptr<karto::ScanSolver>& solver);
  visualization_msgs::MarkerArray createVisualizationMarkers();
 private:
  int marker_count_;
  boost::shared_ptr<karto_plugins::SPASolver> solver_;
  bool visualizer_initialized_;
};

}; // namespace karto_plugins

#endif // SPA_GRAPH_VISUALIZER_H
