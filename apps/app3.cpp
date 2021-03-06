/**
Software License Agreement (BSD)
\file      kernel.cpp 
\authors Xuefeng Chang <changxuefengcn@163.com>
\copyright Copyright (c) 2016, the micROS Team, HPCL (National University of Defense Technology), All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the
   following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
   following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of micROS Team, HPCL, nor the names of its contributors may be used to endorse or promote
   products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WAR-
RANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, IN-
DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "std_msgs/String.h"
#include "nav_msgs/Odometry.h"
#include "geometry_msgs/Twist.h"

#include <string>
#include <list>
#include <vector>
#include <iostream>
#include <utility>
#include <cmath>
#include <ctime>

#include "micros_swarm_framework/micros_swarm_framework.h"

#define BARRIER_VSTIG  1
#define ROBOT_SUM 20

#define PI 3.14159265358979323846
#define EPSILON 0.1
#define A 5
#define B 5
const double C = abs(A-B) / sqrt(4*A*B);
#define H 0.2
#define D 10
#define R 12
#define C1 0.3
#define C2 0.3

double pm1=1,pm2=3,pm3=1;

using namespace std;

class NeighborHandle
{
    public:
    double _px,_py,_vx,_vy;
    pair<double,double> _position,_velocity;
    int _r_id; 
    double mypm_g;
    NeighborHandle(int r_id, float x, float y, float vx, float vy)
    {
        
        _px=x;
        _py=y;
        _vx=vx;
        _vy=vy;
        _position=pair<double,double>(x,y);
        _velocity=pair<double,double>(vx,vy);
        _r_id = r_id;
        mypm_g=1;
    }
};

static list<NeighborHandle*> neighbor_list;
pair<double,double> my_position=pair<double,double>(0,0);
pair<double,double> my_velocity=pair<double,double>(0,0);

pair<double,double> get_vector(pair<double,double> start,pair<double,double> end)
{
    pair<double,double> re=pair<double,double>(0,0);
    re.first=end.first-start.first;
    re.second=end.second-start.second;
    return re;
}

double segma_norm(pair<double,double> v)
{
    double re = EPSILON*(v.first*v.first+v.second*v.second);
    re = sqrt(1+re)-1;
    re /= EPSILON;
    return re;
}

double R_alpha = segma_norm(pair<double,double>(R,0));
double D_alpha = segma_norm(pair<double,double>(D,0));

pair<double,double> segma_epsilon(pair<double,double> v)
{
    pair<double,double> re = pair<double,double>(0,0);
    double scale = 1+EPSILON*(v.first*v.first+v.second*v.second);
    scale = sqrt(scale);
    re.first = v.first / scale;
    re.second = v.second / scale;
    return re;
}

double segma_1(double z)
{
    return z / sqrt(1+z*z);
}

double phi(double z)
{
    return 0.5*((A+B)*segma_1(z+C)+A-B);
}

double rho(double z)
{
    if(z<H)
        return 1;
    if(z>1)
        return 0;
    return 0.5*(1+cos(PI*(z-H)/(1-H)));
}

double phi_alpha(double z)
{
    return rho(z/R_alpha)*phi(z-D_alpha);
}

pair<double,double> f_g()
{
    pair<double,double> re = pair<double,double>(0,0);
    for(list<NeighborHandle*>::iterator i=neighbor_list.begin();i!=neighbor_list.end();i++)
    {
        pair<double,double> q_ij = get_vector(my_position,(*i)->_position);
        pair<double,double> n_ij = segma_epsilon(q_ij);
        re.first += phi_alpha(segma_norm(q_ij))*n_ij.first*(*i)->mypm_g;
        re.second += phi_alpha(segma_norm(q_ij))*n_ij.second*(*i)->mypm_g;
    }
    return re;
}

double a_ij(pair<double,double> j_p)
{
    return rho(segma_norm(get_vector(my_position,j_p)) / R_alpha);
}

pair<double,double> f_d()
{
    pair<double,double> re = pair<double,double>(0,0);
    for(list<NeighborHandle*>::iterator i=neighbor_list.begin();i!=neighbor_list.end();i++)
    {
        pair<double,double> p_ij = get_vector(my_velocity,(*i)->_velocity);
        re.first += a_ij((*i)->_position) * p_ij.first;
        re.second += a_ij((*i)->_position) * p_ij.second;
    }
    return re;
}

double interval = 0.01;
pair<double,double> f_r()
{
    pair<double,double> re = pair<double,double>(0,0);
    static pair<double,double> q_r = pair<double,double>(0,0);
    pair<double,double> p_r = pair<double,double>(0.1,0.1);
    q_r.first += p_r.first * interval;
    q_r.second += p_r.second * interval;
    re.first = -C1*get_vector(q_r,my_position).first - C2*get_vector(p_r,my_velocity).first;
    re.second = -C1*get_vector(q_r,my_position).second - C2*get_vector(p_r,my_velocity).second;
    return re;
}

void barrier_wait()
{
    //barrier
    micros_swarm_framework::KernelHandle kh;
    micros_swarm_framework::VirtualStigmergy<bool> barrier(BARRIER_VSTIG);
    std::string robot_id_string=boost::lexical_cast<std::string>(micros_swarm_framework::KernelInitializer::unique_robot_id_);
    barrier.virtualStigmergyPut(robot_id_string, 1);
    
    ros::Rate loop_rate(1);
    while(ros::ok())
    {
        if(barrier.virtualStigmergySize()==ROBOT_SUM)
            break;
        ros::spinOnce();
        loop_rate.sleep();
    }
}

void locationCallback(const nav_msgs::Odometry& lmsg)
{
    float x=lmsg.pose.pose.position.x;
    float y=lmsg.pose.pose.position.y;

    float vx=lmsg.twist.twist.linear.x;
    float vy=lmsg.twist.twist.linear.y;
    
    micros_swarm_framework::KernelHandle kh;
    micros_swarm_framework::Base l(x, y, 0, vx, vy, 0);
    kh.setRobotBase(l);
}

int main(int argc, char** argv){
    
    ros::init(argc, argv, "micros_swarm_framework_app1_node");
    ros::NodeHandle nh;

    micros_swarm_framework::KernelHandle kh;
    
    int robot_id=-1;
    bool param_ok = ros::param::get ("~unique_robot_id", robot_id);
    if(!param_ok)
    {
        std::cout<<"could not get parameter unique_robot_id"<<std::endl;
        exit(1);
    }else{
        std::cout<<"unique_robot_id = "<<robot_id<<std::endl;
    }
    micros_swarm_framework::KernelInitializer::initRobotID(robot_id);
    
    kh.setNeighborDistance(12);
    
    ros::Subscriber sub = nh.subscribe("base_pose_ground_truth", 1000, locationCallback, ros::TransportHints().udp());
    
    boost::thread barrier(&barrier_wait);
    barrier.join();

    ros::Publisher pub = nh.advertise<geometry_msgs::Twist>("cmd_vel",1000);
    int hz=10;
    interval=1.0/hz;
    ros::Rate loop_rate(hz);
    geometry_msgs:: Twist msg;
    while(ros::ok())
    {
        ros::spinOnce();
        micros_swarm_framework::Neighbors<micros_swarm_framework::NeighborBase> n(true);

        typename std::map<unsigned int, micros_swarm_framework::NeighborBase>::iterator it; 
        for(it=n.data_.begin();it!=n.data_.end();it++)
        {
            NeighborHandle* nh=new NeighborHandle(it->first, it->second.getX(), it->second.getY(), it->second.getVX(), it->second.getVY());
            neighbor_list.push_back(nh);
        }

        micros_swarm_framework::Base nl=kh.getRobotBase();

        my_position=pair<double,double>(nl.getX(), nl.getY());
        my_velocity=pair<double,double>(nl.getVX(), nl.getVY());
      
        msg.linear.x += (f_g().first*pm1+f_d().first*pm2+f_r().first*pm3)/hz;
        msg.linear.y += (f_g().second*pm1+f_d().second*pm2+f_r().second*pm3)/hz;
        //cout<<f_g().second<<' '<<f_d().second<<' '<<msg.linear.y<<endl;
      
        if (msg.linear.x >1)
            msg.linear.x=1;
        if (msg.linear.x <-1)
            msg.linear.x=-1;
        if (msg.linear.y >1)
            msg.linear.y=1;
        if (msg.linear.y <-1)
            msg.linear.y=-1;
        pub.publish(msg);
      
        neighbor_list.clear();
        loop_rate.sleep();
    }
    
    return 0;
}
