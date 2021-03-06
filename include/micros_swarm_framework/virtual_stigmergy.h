/**
Software License Agreement (BSD)
\file      virtual_stigmergy.h 
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

#ifndef VIRTUAL_STIGMERGY_H_
#define VIRTUAL_STIGMERGY_H_

#include <iostream>
#include <string>
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <stack>
#include <map>
#include <set>
#include <queue>
#include <algorithm>

#include "ros/ros.h"

#include "micros_swarm_framework/kernel.h"

namespace micros_swarm_framework{
    
    template<class Type>
    class VirtualStigmergy{
        private:
            unsigned int vstig_id_;
        public:
            VirtualStigmergy(){vstig_id_=-1;}
            
            VirtualStigmergy(unsigned int vstig_id)
            {
                vstig_id_=vstig_id;
                micros_swarm_framework::KernelHandle kh;
                kh.createVirtualStigmergy(vstig_id_);
            }
            
            void virtualStigmergyPut(std::string key, Type data)
            {
                std::ostringstream archiveStream;
                boost::archive::text_oarchive archive(archiveStream);
                archive<<data;
                std::string s=archiveStream.str();
                
                micros_swarm_framework::KernelHandle kh;
                unsigned int id=vstig_id_;
                std::string key_str=key;
                std::string value_str=s;
                time_t time_now=time(0);
                unsigned int robot_id=kh.getRobotID();
                kh.insertOrUpdateVirtualStigmergy(id, key_str, value_str, time_now, robot_id);
                
                VirtualStigmergyPut vsp(id, key_str, value_str, time_now, robot_id);
                
                std::ostringstream archiveStream2;
                boost::archive::text_oarchive archive2(archiveStream2);
                archive2<<vsp;
                std::string vsp_str=archiveStream2.str();   
                      
                micros_swarm_framework::MSFPPacket p;
                p.packet_source=robot_id;
                p.packet_version=1;
                p.packet_type=VIRTUAL_STIGMERGY_PUT;
                p.packet_data=vsp_str;
                p.package_check_sum=0;
                
                kh.publishPacket(p);
                
                ros::Duration(0.1).sleep();   
            }
            
            Type virtualStigmergyGet(std::string key)
            {
                micros_swarm_framework::KernelHandle kh;
                VstigTuple vst=kh.getVirtualStigmergyTuple(vstig_id_, key);
                
                if(vst.getVstigTimestamp()==0)
                {
                    std::cout<<"ID"<<vstig_id_<<" virtual stigmergy, "<<key<<"is not exist."<<std::endl;
                }
                
                std::string data_str=vst.getVstigValue();
                Type data;
                std::istringstream archiveStream(data_str);
                boost::archive::text_iarchive archive(archiveStream);
                archive>>data;
                
                unsigned int id=vstig_id_;
                std::string key_std=key;
                std::string value_std=vst.getVstigValue();
                time_t time_now=vst.getVstigTimestamp();
                unsigned int robot_id=vst.getRobotID();
                VirtualStigmergyQuery vsq(id, key_std, value_std, time_now, robot_id);
                
                std::ostringstream archiveStream2;
                boost::archive::text_oarchive archive2(archiveStream2);
                archive2<<vsq;
                std::string vsg_str=archiveStream2.str();  
                
                micros_swarm_framework::MSFPPacket p;
                p.packet_source=kh.getRobotID();
                p.packet_version=1;
                p.packet_type=VIRTUAL_STIGMERGY_QUERY;
                p.packet_data=vsg_str;
                p.package_check_sum=0;
                kh.publishPacket(p);
                
                ros::Duration(0.1).sleep();
                
                return data;  
            }
            
            unsigned int virtualStigmergySize()
            {
                micros_swarm_framework::KernelHandle kh;
                return kh.getVirtualStigmergySize(vstig_id_);
            }
    };
}
#endif
