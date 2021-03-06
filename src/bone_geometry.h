#ifndef BONE_GEOMETRY_H
#define BONE_GEOMETRY_H

#include <ostream>
#include <vector>
#include <string>
#include <map>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <mmdadapter.h>
#include "gui.h"

class TextureToRender;

struct BoundingBox {
	BoundingBox()
		: min(glm::vec3(-std::numeric_limits<float>::max())),
		max(glm::vec3(std::numeric_limits<float>::max())) {}
	glm::vec3 min;
	glm::vec3 max;
};

struct Joint {
	Joint()
		: joint_index(-1),
		  parent_index(-1),
		  position(glm::vec3(0.0f)),
		  init_position(glm::vec3(0.0f))
	{
	}
	Joint(int id, glm::vec3 wcoord, int parent)
		: joint_index(id),
		  parent_index(parent),
		  position(wcoord),
		  init_position(wcoord)
	{
	}

	int joint_index;
	int parent_index;
	glm::vec3 position;             // position of the joint
	glm::fquat orientation;         // rotation w.r.t. initial configuration
	glm::fquat rel_orientation;     // rotation w.r.t. it's parent. Used for animation.
	glm::vec3 init_position;        // initial position of this joint
	std::vector<int> children;
};

struct Configuration {
	std::vector<glm::vec3> trans;
	std::vector<glm::fquat> rot;

	const void* transData() const { return trans.data(); }
	const void* rotData() const { return rot.data(); }
};

struct KeyFrame {
	std::vector<glm::fquat> rel_rot;

	glm::fquat camera_rel_orientation;
	static void interpolate(const KeyFrame& from,
	                        const KeyFrame& to,
	                        float tau,
	                        KeyFrame& target);
	static void interpolate_frame_spline(std::vector<KeyFrame>& key_frames, 
				                        float t,
				                        KeyFrame& target);
	static glm::fquat catmull_rom_spline(const std::vector<glm::fquat>& cp, float t);
};

struct LineMesh {
	std::vector<glm::vec4> vertices;
	std::vector<glm::uvec2> indices;
};

struct Skeleton {
	std::vector<Joint> joints;

	Configuration cache;

	void refreshCache(Configuration* cache = nullptr);
	const glm::vec3* collectJointTrans() const;
	const glm::fquat* collectJointRot() const;

	// FIXME: create skeleton and bone data structures
	const glm::mat4 getBoneTransform(int joint_index) const;
	void rotate_bone(const int bone_index, const glm::fquat& rotate_quat);	// rotate a bone and recompute all children's data
	void update_children(Joint& parent_joint);
	void transform_skeleton_by_frame(KeyFrame& frame);
	void translate_root(glm::vec3 offset);
	void set_rest_pose();

};

struct Mesh {
	Mesh();
	~Mesh();
	void assignGUI(GUI* gui) {this->gui_ = gui;}

	std::vector<glm::vec4> vertices;
	/*
	 * Static per-vertex attrributes for Shaders
	 */
	std::vector<int32_t> joint0;
	std::vector<int32_t> joint1;
	std::vector<float> weight_for_joint0; // weight_for_joint1 can be calculated
	std::vector<glm::vec3> vector_from_joint0;
	std::vector<glm::vec3> vector_from_joint1;
	std::vector<glm::vec4> vertex_normals;
	std::vector<glm::vec4> face_normals;
	std::vector<glm::vec2> uv_coordinates;
	std::vector<glm::uvec3> faces;

	std::vector<KeyFrame> key_frames;
	std::vector<TextureToRender*> textures; // TextureToRender
	bool to_load_animation = false;	// flag of load animation from external files
	bool to_overwrite_keyframe = false;
	bool to_save_preview = false;
	bool spline_interpolation_enabled = false;
	int key_frame_to_overwrite;



	void playAnimation();	

	std::vector<Material> materials;
	BoundingBox bounds;
	Skeleton skeleton;

	void loadPmd(const std::string& fn);
	int getNumberOfBones() const;
	glm::vec3 getCenter() const { return 0.5f * glm::vec3(bounds.min + bounds.max); }
	const Configuration* getCurrentQ() const; // Configuration is abbreviated as Q
	void updateAnimation(float t = -1.0);

	void saveAnimationTo(const std::string& fn);
	void loadAnimationFrom(const std::string& fn);

	glm::vec3 getJointPosition(int joint_index) const;

	void delete_keyframe(int current_keyframe_);
	void overwrite_keyframe_with_current(int target_keyframe);
	void insert_keyframe_before(int keyframe_index);

	void saveKeyFrame();


private:
	void computeBounds();
	void computeNormals();
	Configuration currentQ_;
	GUI* gui_;
};


#endif
