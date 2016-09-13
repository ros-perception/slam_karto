#ifndef GRAPH_VISUALIZER_H
#define GRAPH_VISUALIZER_H

#include <open_karto/Mapper.h>
#include <visualization_msgs/MarkerArray.h>
#include <string>

namespace karto {

/*
 * Interface for graph visualizer plugins to be used in the slam_karto node
 */
class GraphVisualizer
{
 public:
  virtual ~GraphVisualizer(){}
  virtual void initialize(const boost::shared_ptr<karto::ScanSolver>& solver)=0;
  virtual visualization_msgs::MarkerArray createVisualizationMarkers()=0;
  inline void setFrameId(const std::string &frame_id)
  {
    map_frame_id_ = frame_id;
  }
 protected:
  std::string map_frame_id_;
};

}; // namespace karto

#endif // GRAPH_VISUALIZER_H
