/**
Software License Agreement (BSD)

\authors   Mike Purvis <mpurvis@clearpathrobotics.com>
\copyright Copyright (c) 2014, Clearpath Robotics, Inc., All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the
   following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
   following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Clearpath Robotics nor the names of its contributors may be used to endorse or promote
   products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WAR-
RANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, IN-
DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "geometry_msgs/Twist.h"
#include "ros/ros.h"
#include "sensor_msgs/Joy.h"
#include "freight_lite_teleop/freight_lite_teleop_joy.h"
#include "freight_lite_teleop/freight_lite_defines.h"

#include <iostream>
#include <std_msgs/Int16.h>
#include <map>
#include <string>

namespace freight_lite_teleop
{

/**
 * Internal members of class. This is the pimpl idiom, and allows more flexibility in adding
 * parameters later without breaking ABI compatibility, for robots which link FreightLiteTeleopJoy
 * directly into base nodes.
 */
struct FreightLiteTeleopJoy::Impl
{
  void joyCallback(const sensor_msgs::Joy::ConstPtr& joy);
  void sendCmdVelMsg(const sensor_msgs::Joy::ConstPtr& joy_msg, const std::string& which_map);
  void sendAdjustSteeringMsg(const sensor_msgs::Joy::ConstPtr& joy_msg, int wheel);
  void send_stop_if_necessary(const sensor_msgs::Joy::ConstPtr& joy_msg, int enable_button);

  ros::Subscriber joy_sub;
  ros::Publisher cmd_vel_pub;
  ros::Publisher adjust_steering_pub;

  int enable_button;
  int enable_horiz_button;
  int enable_twist_button;
  int axis_adjust_front;
  int axis_adjust_back;
  int prev_enable;  // detect if we just changed modes & reinitialize robot

  std::map<std::string, int> axis_linear_map;
  std::map< std::string, std::map<std::string, double> > scale_linear_map;

  std::map<std::string, int> axis_angular_map;
  std::map< std::string, std::map<std::string, double> > scale_angular_map;

  int axis_adjust_steering;
  // std::map<std::string, int> axis_adjust_steering;
  // std::map< std::string, std::map<std::string, double> > scale_adjust_steering_map;

  bool sent_disable_msg;
};

/**
 * Constructs FreightLiteTeleopJoy.
 * \param nh NodeHandle to use for setting up the publisher and subscriber.
 * \param nh_param NodeHandle to use for searching for configuration parameters.
 */
FreightLiteTeleopJoy::FreightLiteTeleopJoy(ros::NodeHandle* nh, ros::NodeHandle* nh_param)
{
  pimpl_ = new Impl;

  pimpl_->cmd_vel_pub = nh->advertise<geometry_msgs::Twist>("cmd_vel", 1, true);
  pimpl_->joy_sub = nh->subscribe<sensor_msgs::Joy>("joy", 1, &FreightLiteTeleopJoy::Impl::joyCallback, pimpl_);
  pimpl_->adjust_steering_pub = nh->advertise<std_msgs::Int16>("adjust_steering", 1, true);

  nh_param->param<int>("enable_button", pimpl_->enable_button, 0);
  nh_param->param<int>("enable_horiz_button", pimpl_->enable_horiz_button, -1);
  nh_param->param<int>("enable_twist_button", pimpl_->enable_twist_button, -1);
  nh_param->param<int>("axis_adjust_front", pimpl_->axis_adjust_front, -1);
  nh_param->param<int>("axis_adjust_back", pimpl_->axis_adjust_back, -1);
  nh_param->getParam("axis_adjust_steering", pimpl_->axis_adjust_steering);

  if (nh_param->getParam("axis_linear", pimpl_->axis_linear_map))
  {
    nh_param->getParam("scale_linear", pimpl_->scale_linear_map["normal"]);
    // nh_param->getParam("scale_linear_turbo", pimpl_->scale_linear_map["turbo"]);
  }
  else
  {
    nh_param->param<int>("axis_linear", pimpl_->axis_linear_map["x"], 1);
    nh_param->param<double>("scale_linear", pimpl_->scale_linear_map["normal"]["x"], 0.5);
    // nh_param->param<double>("scale_linear_turbo", pimpl_->scale_linear_map["turbo"]["x"], 1.0);
  }

  if (nh_param->getParam("axis_angular", pimpl_->axis_angular_map))
  {
    nh_param->getParam("scale_angular", pimpl_->scale_angular_map["normal"]);
    // nh_param->getParam("scale_angular_turbo", pimpl_->scale_angular_map["turbo"]);
  }
  else
  {
    nh_param->param<int>("axis_angular", pimpl_->axis_angular_map["yaw"], 0);
    nh_param->param<double>("scale_angular", pimpl_->scale_angular_map["normal"]["yaw"], 0.5);
    // nh_param->param<double>("scale_angular_turbo",
        // pimpl_->scale_angular_map["turbo"]["yaw"], pimpl_->scale_angular_map["normal"]["yaw"]);
  }

  /*
  if (nh_param->getParam("axis_adjust_steering", pimpl_->axis_adjust_steering))
  {
    nh_param->getParam("scale_adjust", pimpl_->scale_angular_map["normal"]);
  }
  else
  {
    nh_param->param<int>("axis_adjust_steering", pimpl_->axis_adjust_steering["delta"], 0);
    nh_param->param<double>("scale_adjust", pimpl_->scale_adjust_steering_map["normal"]["delta"], 1);
  }
  */


  ROS_INFO_NAMED("FreightLiteTeleopJoy", "Teleop enable button %i.", pimpl_->enable_button);
  ROS_INFO_NAMED("FreightLiteTeleopJoy", "Horiz enable button %i.", pimpl_->enable_horiz_button);
  ROS_INFO_NAMED("FreightLiteTeleopJoy", "Twist enable button %i.", pimpl_->enable_twist_button);
  // ROS_INFO_COND_NAMED(pimpl_->enable_turbo_button >= 0, "TeleopTwistJoy",
      // "Turbo on button %i.", pimpl_->enable_turbo_button);

  for (std::map<std::string, int>::iterator it = pimpl_->axis_linear_map.begin();
      it != pimpl_->axis_linear_map.end(); ++it)
  {
    ROS_INFO_NAMED("FreightLiteTeleopJoy", "Linear axis %s on %i at scale %f.",
    it->first.c_str(), it->second, pimpl_->scale_linear_map["normal"][it->first]);
    // ROS_INFO_COND_NAMED(pimpl_->enable_turbo_button >= 0, "TeleopTwistJoy",
        // "Turbo for linear axis %s is scale %f.", it->first.c_str(), pimpl_->scale_linear_map["turbo"][it->first]);
  }

  for (std::map<std::string, int>::iterator it = pimpl_->axis_angular_map.begin();
      it != pimpl_->axis_angular_map.end(); ++it)
  {
    ROS_INFO_NAMED("FreightLiteTeleopJoy", "Angular axis %s on %i at scale %f.",
    it->first.c_str(), it->second, pimpl_->scale_angular_map["normal"][it->first]);
    // ROS_INFO_COND_NAMED(pimpl_->enable_turbo_button >= 0, "FreightLiteTeleopJoy",
        // "Turbo for angular axis %s is scale %f.", it->first.c_str(), pimpl_->scale_angular_map["turbo"][it->first]);
  }

  /*
  for (std::map<std::string, int>::iterator it = pimpl_->axis_adjust_steering_map.begin();
      it != pimpl_->axis_adjust_steering.end(); ++it)
  {
    ROS_INFO_NAMED("FreightLiteTeleopJoy", "Adjust steering axis %s on %i at scale %f.",
    it->first.c_str(), it->second, pimpl_->scale_adjust_steering_map["normal"][it->first]);
  }
  */

  pimpl_->sent_disable_msg = false;
}

double getVal(const sensor_msgs::Joy::ConstPtr& joy_msg, const std::map<std::string, int>& axis_map,
              const std::map<std::string, double>& scale_map, const std::string& fieldname)
{
  if (axis_map.find(fieldname) == axis_map.end() ||
      scale_map.find(fieldname) == scale_map.end() ||
      joy_msg->axes.size() <= axis_map.at(fieldname))
  {
    return 0.0;
  }

  return joy_msg->axes[axis_map.at(fieldname)] * scale_map.at(fieldname);
}

void FreightLiteTeleopJoy::Impl::sendAdjustSteeringMsg(const sensor_msgs::Joy::ConstPtr& joy_msg, int wheel)
{
  std_msgs::Int16 adjust_steering_msg;
  int adjust_servo;
  double delta;

  if (wheel != ADJUST_WHEEL_ALL_STRAIGHT   
   && wheel != ADJUST_WHEEL_ALL_HORIZ   
   && wheel != ADJUST_WHEEL_ALL_TWIST
   && joy_msg->axes.size() > axis_adjust_steering) {   
    delta = joy_msg->axes[axis_adjust_steering];
    if (delta < -0.5)
      wheel = -1 * wheel;
    else if (delta > 0.5)
      wheel = wheel;
    else
      wheel = ADJUST_WHEEL_NONE;
  }
  if (wheel != ADJUST_WHEEL_NONE) {
    adjust_steering_msg.data = wheel;
    adjust_steering_pub.publish(adjust_steering_msg);
  }
}

void FreightLiteTeleopJoy::Impl::sendCmdVelMsg(const sensor_msgs::Joy::ConstPtr& joy_msg,
                                         const std::string& which_map)
{
  // Initializes with zeros by default.
  geometry_msgs::Twist cmd_vel_msg;

  cmd_vel_msg.linear.x = getVal(joy_msg, axis_linear_map, scale_linear_map[which_map], "x");
  cmd_vel_msg.linear.y = getVal(joy_msg, axis_linear_map, scale_linear_map[which_map], "y");
  cmd_vel_msg.linear.z = getVal(joy_msg, axis_linear_map, scale_linear_map[which_map], "z");
  cmd_vel_msg.angular.z = getVal(joy_msg, axis_angular_map, scale_angular_map[which_map], "yaw");
  cmd_vel_msg.angular.y = getVal(joy_msg, axis_angular_map, scale_angular_map[which_map], "pitch");
  cmd_vel_msg.angular.x = getVal(joy_msg, axis_angular_map, scale_angular_map[which_map], "roll");

  cmd_vel_pub.publish(cmd_vel_msg);
  sent_disable_msg = false;
}

void FreightLiteTeleopJoy::Impl::joyCallback(const sensor_msgs::Joy::ConstPtr& joy_msg)
{
  /*
  if (enable_turbo_button >= 0 &&
      joy_msg->buttons.size() > enable_turbo_button &&
      joy_msg->buttons[enable_turbo_button])
  {
    sendCmdVelMsg(joy_msg, "turbo");
  } else  
  */
  // std::cout << joy_msg << "\n";
  if (joy_msg->buttons.size() > enable_button &&
           joy_msg->buttons[enable_button])
  {
    send_stop_if_necessary(joy_msg, enable_button);
    sendCmdVelMsg(joy_msg, "normal");
  } 
  else if (joy_msg->buttons.size() > enable_horiz_button &&
           joy_msg->buttons[enable_horiz_button])
  {
    send_stop_if_necessary(joy_msg, enable_horiz_button);
    sendCmdVelMsg(joy_msg, "normal");
  } 
  else if (joy_msg->buttons.size() > enable_twist_button &&
           joy_msg->buttons[enable_twist_button])
  {
    send_stop_if_necessary(joy_msg, enable_twist_button);
    sendCmdVelMsg(joy_msg, "normal");
  } 
  else if (joy_msg->axes.size() > axis_adjust_front
           && joy_msg->axes[axis_adjust_front] != 0.0)
  {
    send_stop_if_necessary(joy_msg, axis_adjust_front);
    if (joy_msg->axes[axis_adjust_front] > 0)
      sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_FL);
    else if (joy_msg->axes[axis_adjust_front] < 0)
      sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_FR);
  } 
  else if (joy_msg->axes.size() > axis_adjust_back
           && joy_msg->axes[axis_adjust_back] != 0.0)
  {
    send_stop_if_necessary(joy_msg, axis_adjust_back);
    if (joy_msg->axes[axis_adjust_back] > 0)
      sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_BL);
    else if (joy_msg->axes[axis_adjust_back] < 0)
      sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_BR);
  } 
  else
  {
    // When enable button is released, immediately send a single no-motion command
    // in order to stop the robot.
    if (!sent_disable_msg)
    {
      // Initializes with zeros by default.
      geometry_msgs::Twist cmd_vel_msg;
      cmd_vel_pub.publish(cmd_vel_msg);
      sent_disable_msg = true;
    }
  }
}

void FreightLiteTeleopJoy::Impl::send_stop_if_necessary(const sensor_msgs::Joy::ConstPtr& joy_msg, int new_enable)
{
  // When different enable button is selected, send a single no-motion command
  if (prev_enable != new_enable) {
      // Initializes with zeros by default.
      if (new_enable == enable_button)
        sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_ALL_STRAIGHT);
      else if (new_enable == enable_horiz_button)
        sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_ALL_HORIZ);
      else if (new_enable == enable_twist_button)
        sendAdjustSteeringMsg(joy_msg, ADJUST_WHEEL_ALL_TWIST);

      geometry_msgs::Twist cmd_vel_msg;
      cmd_vel_pub.publish(cmd_vel_msg);
      sent_disable_msg = true;
  }
  prev_enable = new_enable;
}

}  // namespace freight_lite
