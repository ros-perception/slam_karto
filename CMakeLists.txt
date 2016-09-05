cmake_minimum_required(VERSION 2.8.3)
project(slam_karto)

find_package(catkin REQUIRED
  COMPONENTS
    cmake_modules
    message_filters
    nav_msgs
    open_karto
    rosconsole
    roscpp
    sensor_msgs
    sparse_bundle_adjustment
    tf
    visualization_msgs
    pluginlib
)
find_package(Eigen REQUIRED)

include_directories(${catkin_INCLUDE_DIRS} ${EIGEN_INCLUDE_DIRS})
add_definitions(${EIGEN_DEFINITIONS})

catkin_package(
  LIBRARIES karto_scan_solver_plugins karto_graph_visualizer_plugins
  )

add_library(karto_scan_solver_plugins src/spa_solver.cpp)
add_library(karto_graph_visualizer_plugins src/spa_graph_visualizer.cpp)
add_executable(slam_karto src/slam_karto.cpp)

target_link_libraries(karto_scan_solver_plugins ${catkin_LIBRARIES})
target_link_libraries(karto_graph_visualizer_plugins ${catkin_LIBRARIES})
target_link_libraries(slam_karto ${catkin_LIBRARIES} karto_scan_solver_plugins karto_graph_visualizer_plugins)

install(TARGETS slam_karto karto_scan_solver_plugins karto_graph_visualizer_plugins
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(FILES karto_scan_solver_plugins.xml karto_graph_visualizer_plugins.xml
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)