#include "config.h"
#include "bone_geometry.h"
#include "texture_to_render.h"
#include <fstream>
#include <queue>
#include <iostream>
#include <stdexcept>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "procedure_geometry.h"

/*
 * For debugging purpose.
 */
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
	size_t count = std::min(v.size(), static_cast<size_t>(10));
	for (size_t i = 0; i < count; ++i) os << i << " " << v[i] << "\n";
	os << "size = " << v.size() << "\n";
	return os;
}

std::ostream& operator<<(std::ostream& os, const BoundingBox& bounds)
{
	os << "min = " << bounds.min << " max = " << bounds.max;
	return os;
}

void Skeleton::rotate_bone(const int bone_index, const glm::fquat& rotate_quat) {
	Joint& curr_joint = joints[bone_index];
	Joint& parent_joint = joints[curr_joint.parent_index];
	parent_joint.rel_orientation = rotate_quat * parent_joint.rel_orientation;
	parent_joint.orientation = rotate_quat * parent_joint.orientation;
	update_children(parent_joint);
}


void Skeleton::update_children(Joint& parent_joint) {
	for(int child_index : parent_joint.children) {
		Joint& child_joint = joints[child_index];
		child_joint.orientation = child_joint.rel_orientation * parent_joint.orientation;
		child_joint.position = parent_joint.position + parent_joint.orientation * (child_joint.init_position - parent_joint.init_position);
		update_children(child_joint);
	}
}

void Skeleton::translate_root(glm::vec3 offset) {
	for(Joint& joint : joints) {
		joint.position = joint.position + offset;
	}
}

void Skeleton::transform_skeleton_by_frame(KeyFrame& frame) {
	for(int i = 0; i < joints.size(); i++) {
		joints[i].rel_orientation = frame.rel_rot[i];
	}
	for(int i = 0; i < joints.size(); i++) {
		if(joints[i].parent_index == -1) {
			joints[i].orientation = frame.rel_rot[i];
			update_children(joints[i]);
		}
	}
}


void Skeleton::refreshCache(Configuration* target)
{
	if (target == nullptr)
		target = &cache;
	target->rot.resize(joints.size());
	target->trans.resize(joints.size());
	for (size_t i = 0; i < joints.size(); i++) {
		target->rot[i] = joints[i].orientation;
		target->trans[i] = joints[i].position;
	}
}

const glm::vec3* Skeleton::collectJointTrans() const
{
	return cache.trans.data();
}

const glm::fquat* Skeleton::collectJointRot() const
{
	return cache.rot.data();
}

// FIXME: Implement bone animation.

// my implementation for getting bone transform matrix:
// remember that every bone is pointing from parent to child!
const glm::mat4 Skeleton::getBoneTransform(int joint_index) const
{
	// return bone_transforms[joint_index];
	const Joint& curr_joint = joints[joint_index];
	const Joint& parent_joint = joints[curr_joint.parent_index];

	float length = glm::length(curr_joint.position - parent_joint.position);
	glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0), glm::vec3(1.0, length, 1.0));

	glm::fquat rotate_quat = quaternion_between_two_directs(glm::vec3(0.0, 1.0, 0.0), curr_joint.position - parent_joint.position);
	glm::mat4 rotate_matrix = glm::toMat4(rotate_quat);

	glm::vec3 translate = parent_joint.position;
	glm::mat4 translate_matrix = glm::translate(glm::mat4(1.0f), translate);

	return translate_matrix * rotate_matrix * scale_matrix;

}

void Skeleton::set_rest_pose()
{
	for(int i = 0; i < joints.size(); i++) {
		joints[i].rel_orientation = glm::fquat();
	}
	for(int i = 0; i < joints.size(); i++) {
		if(joints[i].parent_index == -1) {
			update_children(joints[i]);
		}
	}
}

void KeyFrame::interpolate(const KeyFrame& from,
	                        const KeyFrame& to,
	                        float tau,
	                        KeyFrame& target) {
	target.rel_rot.clear();
	for(int i = 0; i < from.rel_rot.size(); i++) {
		glm::fquat kf_rot = glm::mix(from.rel_rot[i], to.rel_rot[i], tau);
		target.rel_rot.push_back(kf_rot);
	}
	target.camera_rel_orientation = glm::mix(from.camera_rel_orientation, to.camera_rel_orientation, tau);
} 


Mesh::Mesh()
{
}

Mesh::~Mesh()
{
	for(int i = 0; i < textures.size(); i++) {
		TextureToRender* current_texture = textures[i];
		delete current_texture;
	}
	textures.clear();
}

void Mesh::loadPmd(const std::string& fn)
{
	MMDReader mr;
	mr.open(fn);
	mr.getMesh(vertices, faces, vertex_normals, uv_coordinates);
	computeBounds();
	mr.getMaterial(materials);

	// FIXME: load skeleton and blend weights from PMD file,
	//        initialize std::vectors for the vertex attributes,
	//        also initialize the skeleton as needed
	// read in joint data.
	int jointId = 0;
	while(true) {
		glm::vec3 wcoord;
		int parentId;
		if(mr.getJoint(jointId, wcoord, parentId)) {
			Joint curr_joint(jointId, wcoord, parentId);
			skeleton.joints.push_back(curr_joint);
			jointId++;
			std::cout << "join " << jointId << ": (" << wcoord.x << ", " << wcoord.y << ", " << wcoord.z << ")" << std::endl;
		}
		else {
			break;
		}
	}
	// init orientation, rel_orientation and children list.
	// computation of orientation: https://stackoverflow.com/questions/1171849/finding-quaternion-representing-the-rotation-from-one-vector-to-another
	for(int i = 0; i < skeleton.joints.size(); i++) {
		Joint& curr_joint = skeleton.joints[i];

		curr_joint.orientation = glm::fquat();
		curr_joint.rel_orientation = glm::fquat();
		if(curr_joint.parent_index != -1) {
			Joint& parent_joint = skeleton.joints[curr_joint.parent_index];
			parent_joint.children.push_back(curr_joint.joint_index);
		}	
		// skeleton.bone_transforms.push_back(glm::mat4(1.0));	// for tmp use
	}

	// load wieghts
	std::vector<SparseTuple> sparse_tuples;
	mr.getJointWeights(sparse_tuples);

	for(SparseTuple& tuple : sparse_tuples) {
		int vid = tuple.vid;
		joint0.push_back(tuple.jid0);
		weight_for_joint0.push_back(tuple.weight0);
		vector_from_joint0.push_back(glm::vec3(vertices[vid]) - skeleton.joints[tuple.jid0].position);

		if(tuple.jid1 == -1) {
			joint1.push_back(0);	// avoid joints[-1] access
			vector_from_joint1.push_back(glm::vec3(0.0, 0.0, 0.0));
			// std::cout << "joint1 equals -1" << std::endl;
		}
		else {
			joint1.push_back(tuple.jid1);
			vector_from_joint1.push_back(glm::vec3(vertices[vid]) - skeleton.joints[tuple.jid1].position);
		}
	}
	// updateAnimation();
}



int Mesh::getNumberOfBones() const
{
	return skeleton.joints.size();
}

void Mesh::computeBounds()
{
	bounds.min = glm::vec3(std::numeric_limits<float>::max());
	bounds.max = glm::vec3(-std::numeric_limits<float>::max());
	for (const auto& vert : vertices) {
		bounds.min = glm::min(glm::vec3(vert), bounds.min);
		bounds.max = glm::max(glm::vec3(vert), bounds.max);
	}
}




void Mesh::updateAnimation(float t)
{

	
	// FIXME: Support Animation Here

	int frame_index = floor(t);
	if(t != -1.0 && frame_index + 1 < key_frames.size()) {

		float tao = t - frame_index;
		KeyFrame frame;
		KeyFrame::interpolate(key_frames[frame_index], key_frames[frame_index + 1], tao, frame);
		skeleton.transform_skeleton_by_frame(frame);
		gui_->set_camera_rel_orientation(frame.camera_rel_orientation);
	}
	skeleton.refreshCache(&currentQ_);
	
}

glm::vec3 Mesh::getJointPosition(int joint_index) const
{
	return skeleton.joints[joint_index].position;
}

const Configuration*
Mesh::getCurrentQ() const
{
	return &currentQ_;
}

void Mesh::saveKeyFrame() {
	KeyFrame kf;
	for(int i = 0; i < getNumberOfBones(); i++) {
		kf.rel_rot.push_back(skeleton.joints[i].rel_orientation);
	}
	kf.camera_rel_orientation = gui_->get_camera_rel_orientation();
	key_frames.push_back(kf);
}

void Mesh::delete_keyframe(int keyframe_index) {
	key_frames.erase(key_frames.begin() + keyframe_index);
	// delete mesh_->textures[current_keyframe_];
	TextureToRender* texture = textures[keyframe_index];
	textures.erase(textures.begin() + keyframe_index);
	delete texture;
	std::cout << "deleted key frame " << keyframe_index << std::endl;
}

void Mesh::overwrite_keyframe_with_current(int target_keyframe) {
	KeyFrame& kf = key_frames[target_keyframe];
	for(int i = 0; i < getNumberOfBones(); i++) {
		kf.rel_rot[i] = skeleton.joints[i].rel_orientation;
	}
	kf.camera_rel_orientation = gui_->get_camera_rel_orientation();
	key_frame_to_overwrite = target_keyframe;
	to_overwrite_keyframe = true;

	
}

// insert the current pose in main view before keyframe_index.
// here I reuse the code of overwriting keyframe in the main. The key idea is to insert a nullptr into
// textures, and overwrite it. 
void Mesh::insert_keyframe_before(int keyframe_index) {
	KeyFrame keyframe_to_insert;
	for(int i = 0; i < getNumberOfBones(); i++) {
		keyframe_to_insert.rel_rot.push_back(skeleton.joints[i].rel_orientation);
	}
	keyframe_to_insert.camera_rel_orientation = gui_->get_camera_rel_orientation();

	key_frames.insert(key_frames.begin() + keyframe_index, keyframe_to_insert);	// std::vector::insert() inserts before pos
	textures.insert(textures.begin() + keyframe_index, nullptr);
	key_frame_to_overwrite = keyframe_index;
	to_overwrite_keyframe = true;
	std::cout << "insert new frame before " << keyframe_index << std::endl;
}