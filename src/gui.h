#ifndef SKINNING_GUI_H
#define SKINNING_GUI_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include "procedure_geometry.h"
#include "tictoc.h"

#define PICK_RAY_LEN 1000.0f	// shoot a ray of this len when picking bones

struct Mesh;

/*
 * Hint: call glUniformMatrix4fv on thest pointers
 */
struct MatrixPointers {
	const float *projection, *model, *view;
};

class GUI {
public:
	GUI(GLFWwindow*, int view_width = -1, int view_height = -1, int preview_height = -1, int scroll_bar_width = -1);
	~GUI();
	void assignMesh(Mesh*);

	void keyCallback(int key, int scancode, int action, int mods);
	void mousePosCallback(double mouse_x, double mouse_y);
	void mouseButtonCallback(int button, int action, int mods);
	void mouseScrollCallback(double dx, double dy);
	void updateMatrices();
	MatrixPointers getMatrixPointers() const;

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MousePosCallback(GLFWwindow* window, double mouse_x, double mouse_y);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void MouseScrollCallback(GLFWwindow* window, double dx, double dy);

	glm::vec3 getCenter() const { return center_; }
	const glm::vec3& getCamera() const { return eye_; }
	bool isPoseDirty() const { return pose_changed_; }
	void clearPose() { pose_changed_ = false; }
	void setPoseDirty() {pose_changed_ = true;}
	const float* getLightPositionPtr() const { return &light_position_[0]; }
	
	int getCurrentBone() const { return current_bone_; }
	const int* getCurrentBonePointer() const { return &current_bone_; }
	bool setCurrentBone(int i);

	bool isTransparent() const { return transparent_; }
	bool isPlaying() const { return play_; }
	float getCurrentPlayTime();

	int get_frame_shift() { return frame_shift; }
	int get_current_keyframe() {return current_keyframe_; };

	bool insert_keyframe_enabled() {return insert_keyframe_enabled_;}


	glm::fquat get_camera_rel_orientation() {return rel_orientation_quat_;};

	void set_camera_rel_orientation(glm::fquat rel_orientation_quat);





	bool to_export_video_ = false;
	FILE* export_file = NULL;

private:
	GLFWwindow* window_;
	Mesh* mesh_;

	int window_width_, window_height_;
	int view_width_, view_height_;
	int preview_height_;
	int scroll_bar_width_;
	bool drag_state_ = false;
	bool drag_scroll_bar_state_ = false;
	bool fps_mode_ = false;
	bool pose_changed_ = true;
	bool transparent_ = false;

	bool insert_keyframe_enabled_ = false;
	int current_bone_ = -1;
	int current_keyframe_ = -1;
	int current_button_ = -1;
	float roll_speed_ = M_PI / 64.0f;
	float last_x_ = 0.0f, last_y_ = 0.0f, current_x_ = 0.0f, current_y_ = 0.0f;
	float camera_distance_ = 30.0;
	float pan_speed_ = 0.1f;
	float rotation_speed_ = 0.02f;
	float zoom_speed_ = 0.1f;
	float aspect_;

	TicTocTimer* timer_ = (TicTocTimer*) malloc(sizeof(TicTocTimer));
	float time_ = 0.0;	// fake timer for temp use.

	int frame_shift = 0;


	glm::vec3 eye_ = glm::vec3(0.0f, 0.1f, camera_distance_);
	glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 look_ = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 tangent_ = glm::cross(look_, up_);
	glm::vec3 center_ = eye_ - camera_distance_ * look_;
	glm::mat3 orientation_ = glm::mat3(tangent_, up_, look_);
	glm::vec4 light_position_;

	glm::mat4 view_matrix_ = glm::lookAt(eye_, center_, up_);
	glm::mat4 projection_matrix_;
	glm::mat4 model_matrix_ = glm::mat4(1.0f);


	// glm::vec3 lightInvDir = glm::normalize(light_position_ - center_);
	// // Compute the MVP matrix from the light's point of view
	// glm::mat4 depthProjectionMatrix = glm::ortho<float>(-10,10,-10,10,-10,20);
	// glm::mat4 depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
	// glm::mat4 depthModelMatrix = glm::mat4(1.0);
	// glm::mat4 depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;

	// // Send our transformation to the currently bound shader,
	// // in the "MVP" uniform
	// glUniformMatrix4fv(depthMatrixID, 1, GL_FALSE, &depthMVP[0][0])



	bool captureWASDUPDOWN(int key, int action);
	unsigned char* export_buffer_;
	bool play_ = false;

	glm::mat4 init_camera_orientation_ = glm::mat4(orientation_);
	glm::fquat rel_orientation_quat_;
};

#endif
