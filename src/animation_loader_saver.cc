#include "bone_geometry.h"
#include "texture_to_render.h"
#include <fstream>
#include <iostream>
#include <glm/gtx/io.hpp>
#include <unordered_map>
/*
 * We put these functions to a separated file because the following jumbo
 * header consists of 20K LOC, and it is very slow to compile.
 */
#include "json.hpp"

using json = nlohmann::json;

void Mesh::saveAnimationTo(const std::string& fn)
{
	// FIXME: Save keyframes to json file.
	json my_json;
	for(int i = 0; i < key_frames.size(); i++) {
		KeyFrame& key_frame = key_frames[i];
		json rot_array = json::array();
		for(int j = 0; j < key_frame.rel_rot.size(); j++) {
			json quat_json = json::array();	// vec4
			// for(int k = 0; k < 4; k++) {
			// 	quat_json.push_back(key_frame.rel_rot[j][k]);
			// }
			quat_json.push_back(key_frame.rel_rot[j].w);
			quat_json.push_back(key_frame.rel_rot[j].x);
			quat_json.push_back(key_frame.rel_rot[j].y);
			quat_json.push_back(key_frame.rel_rot[j].z);

			rot_array.push_back(quat_json);
		}
		my_json.push_back(rot_array);
	}
	std::string json_str = my_json.dump(4);
	std::ofstream outfile;
	outfile.open (fn);
	outfile << json_str;
	outfile.close();
	std::cout << "wrote animation to " << fn << std::endl;



}

void Mesh::loadAnimationFrom(const std::string& fn)
{
	// https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
	std::ifstream in_stream(fn);
	std::string json_str((std::istreambuf_iterator<char>(in_stream)),
                 std::istreambuf_iterator<char>());
	// std::cout << "load json string: " << json_str << std::endl;
	json my_json = json::parse(json_str);
	key_frames.clear();
	for(int i = 0; i < my_json.size(); i++) {
		json& key_frame_json = my_json[i];
		KeyFrame key_frame;
		for(int j = 0; j < key_frame_json.size(); j++) {
			json& quat_json = key_frame_json[j];
			glm::fquat rot_quat(quat_json[0], quat_json[1], quat_json[2], quat_json[3]);
			key_frame.rel_rot.push_back(rot_quat);
		}
		key_frames.push_back(key_frame);
	}
	// skeleton.transform_skeleton_by_frame(key_frames[0]);
	// FIXME: Load keyframes from json file.
}
