#include "gui.h"
#include "config.h"
#include <jpegio.h>
#include "bone_geometry.h"
#include "texture_to_render.h"

#include <iostream>
#include <debuggl.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
	// FIXME: Implement a function that performs proper
	//        ray-cylinder intersection detection
	// TIPS: The implement is provided by the ray-tracer starter code.
}

GUI::GUI(GLFWwindow* window, int view_width, int view_height, int preview_height)
	:window_(window), preview_height_(preview_height)
{
	glfwSetWindowUserPointer(window_, this);
	glfwSetKeyCallback(window_, KeyCallback);
	glfwSetCursorPosCallback(window_, MousePosCallback);
	glfwSetMouseButtonCallback(window_, MouseButtonCallback);
	glfwSetScrollCallback(window_, MouseScrollCallback);

	glfwGetWindowSize(window_, &window_width_, &window_height_);
	if (view_width < 0 || view_height < 0) {
		view_width_ = window_width_;
		view_height_ = window_height_;
	} else {
		view_width_ = view_width;
		view_height_ = view_height;
	}
	float aspect_ = static_cast<float>(view_width_) / view_height_;
	projection_matrix_ = glm::perspective((float)(kFov * (M_PI / 180.0f)), aspect_, kNear, kFar);
	*timer_ = tic();

}

GUI::~GUI()
{
	free(timer_);
}

void GUI::assignMesh(Mesh* mesh)
{
	mesh_ = mesh;
	center_ = mesh_->getCenter();
}

void GUI::keyCallback(int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window_, GL_TRUE);
		return ;
	}
	if (key == GLFW_KEY_J && action == GLFW_RELEASE) {
		//FIXME save out a screenshot using SaveJPEG
		unsigned char* pixmap = (unsigned char*) malloc (sizeof(char) * window_width_ * window_height_ * 3);
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		glReadPixels(0, 0, window_width_, window_height_, GL_RGB, GL_UNSIGNED_BYTE, pixmap);
		SaveJPEG("screenshot.jpg", window_width_, window_height_, pixmap);
		std::cout << "done screenshot" << std::endl;	
	}
	if (key == GLFW_KEY_S && (mods & GLFW_MOD_CONTROL)) {
		if (action == GLFW_RELEASE)
			mesh_->saveAnimationTo("animation.json");
		return ;
	}

	if (mods == 0 && captureWASDUPDOWN(key, action))
		return ;
	if (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT) {
		float roll_speed;
		if (key == GLFW_KEY_RIGHT)
			roll_speed = -roll_speed_;
		else
			roll_speed = roll_speed_;
		// FIXME: actually roll the bone here
		bool drag_bone = drag_state_ && current_button_ == GLFW_MOUSE_BUTTON_LEFT;
		if(drag_bone && current_bone_ != -1) {
			Joint& curr_joint = mesh_->skeleton.joints[current_bone_];
			glm::vec3 curr_joint_pos = mesh_->getJointPosition(curr_joint.joint_index);
			glm::vec3 parent_joint_pos = mesh_->getJointPosition(curr_joint.parent_index);
			glm::vec3 rotate_axis = glm::normalize(curr_joint_pos - parent_joint_pos);
			glm::fquat rotate_quat = glm::angleAxis(roll_speed, rotate_axis);
			mesh_->skeleton.rotate_bone(current_bone_, rotate_quat);
			setPoseDirty();
		}
	} else if (key == GLFW_KEY_C && action != GLFW_RELEASE) {
		fps_mode_ = !fps_mode_;
	} else if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_RELEASE) {
		current_bone_--;
		current_bone_ += mesh_->getNumberOfBones();
		current_bone_ %= mesh_->getNumberOfBones();
	} else if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_RELEASE) {
		current_bone_++;
		current_bone_ += mesh_->getNumberOfBones();
		current_bone_ %= mesh_->getNumberOfBones();
	} else if (key == GLFW_KEY_T && action != GLFW_RELEASE) {
		transparent_ = !transparent_;
	}

	// FIXME: implement other controls here.
	else if (key == GLFW_KEY_F && action != GLFW_RELEASE) {
		std::cout << "keyframe saved" << std::endl;
		mesh_->saveKeyFrame();
		to_save_preview = true;
	} else if (key == GLFW_KEY_P && action != GLFW_RELEASE) {	// resume/pause timer
		if(!play_) {
			play_ = true;
			*timer_ = tic();
		} else {
			play_ = false;
		}
	} else if(key == GLFW_KEY_R && action != GLFW_RELEASE) {	// reset timer
		time_ = 0;
	} else if(key == GLFW_KEY_U && action != GLFW_RELEASE) {	// load keyframe into main view
		if(current_keyframe_ != -1) {	// bone selected
			mesh_->skeleton.transform_skeleton_by_frame(mesh_->key_frames[current_keyframe_]);
			mesh_->updateAnimation();
		}
	} else if(key == GLFW_KEY_DELETE && action != GLFW_RELEASE) {
		if(current_keyframe_ != -1) {
			mesh_->delete_keyframe(current_keyframe_);
		}
	} else if(key == GLFW_KEY_SPACE && action != GLFW_RELEASE) {
		if(current_keyframe_ != -1) {
			mesh_->overwrite_keyframe_with_current(current_keyframe_);
		}
	} else if(key == GLFW_KEY_I && action != GLFW_RELEASE) {
		// toggle insert mode
		insert_keyframe_enabled_ = !insert_keyframe_enabled_;
	}

}

void GUI::mousePosCallback(double mouse_x, double mouse_y)
{
	last_x_ = current_x_;
	last_y_ = current_y_;
	current_x_ = mouse_x;
	current_y_ = window_height_ - mouse_y;
	float delta_x = current_x_ - last_x_;
	float delta_y = current_y_ - last_y_;
	if (sqrt(delta_x * delta_x + delta_y * delta_y) < 1e-15)
		return;
	if (mouse_x > view_width_)
		return ;
	glm::vec3 mouse_direction = glm::normalize(glm::vec3(delta_x, delta_y, 0.0f));
	glm::vec2 mouse_start = glm::vec2(last_x_, last_y_);
	glm::vec2 mouse_end = glm::vec2(current_x_, current_y_);
	glm::uvec4 viewport = glm::uvec4(0, 0, view_width_, view_height_);

	bool drag_camera = drag_state_ && current_button_ == GLFW_MOUSE_BUTTON_RIGHT;
	bool drag_bone = drag_state_ && current_button_ == GLFW_MOUSE_BUTTON_LEFT;
	bool translate_root_mode = drag_state_ && current_button_ == GLFW_MOUSE_BUTTON_MIDDLE;

	if (drag_camera) {
		glm::vec3 axis = glm::normalize(
				orientation_ *
				glm::vec3(mouse_direction.y, -mouse_direction.x, 0.0f)
				);
		orientation_ =
			glm::mat3(glm::rotate(rotation_speed_, axis) * glm::mat4(orientation_));
		tangent_ = glm::column(orientation_, 0);
		up_ = glm::column(orientation_, 1);
		look_ = glm::column(orientation_, 2);
	} else if (drag_bone && current_bone_ != -1) {
		// FIXME: Handle bone rotation
		int parent_index = mesh_->skeleton.joints[current_bone_].parent_index;
		glm::vec3 parent_joint_pos = mesh_->getJointPosition(parent_index);
		glm::vec3 projected_parent_pos = glm::project(parent_joint_pos,
											view_matrix_,
											projection_matrix_,
											viewport);

		// glm::vec2 start_direct_2D(projected_joint_pos.x - projected_parent_pos.x, projected_joint_pos.y - projected_parent_pos.y);
		glm::vec2 start_direct_2D(last_x_ - projected_parent_pos.x, last_y_ - projected_parent_pos.y);
		glm::vec2 end_direct_2D(current_x_ - projected_parent_pos.x, current_y_ - projected_parent_pos.y);
		float angle_2D = angle_between_two_directs_2D(start_direct_2D, end_direct_2D);
		// std::cout << "dragging angle: " << angle_2D << std::endl;

		glm::vec3 rotate_axis = glm::normalize(parent_joint_pos - eye_);
		// use radians because GLM_FORCE_RADIANS is set. See: https://glm.g-truc.net/0.9.4/api/a00153.html#ga30071b5b9773087b7212a5ce67d0d90a
		glm::fquat rotate_quat = glm::angleAxis(angle_2D, rotate_axis);

		glm::fquat& joint_rot = mesh_->skeleton.cache.rot[current_bone_];
		mesh_->skeleton.rotate_bone(current_bone_, rotate_quat);
		// std::cout << "rotate bone: " << current_bone_ << ", by theta = " << angle_2D 
		// 	<< ", quat: (" << rotate_quat[0] << "," << rotate_quat[1] << ", " << rotate_quat[2]  << ", " << rotate_quat[3] << ")" 
		// 	<< ", curr orientation:  (" << joint_rot[0] << "," << joint_rot[1] << ", " << joint_rot[2]  << ", " << joint_rot[3] << ")"
		// 	<< std::endl;

		setPoseDirty();
		// std::cout << "2-D coords of bone " << current_bone_ << ": " << projected_joint_pos.x 
		// 	<< ", " << projected_joint_pos.y << std::endl;
		return ;
	} else if(translate_root_mode && current_bone_ != -1) {
		int parent_index = mesh_->skeleton.joints[current_bone_].parent_index;
		glm::vec3 parent_joint_pos = mesh_->getJointPosition(parent_index);
		glm::vec3 projected_parent_pos = glm::project(parent_joint_pos,
											view_matrix_,
											projection_matrix_,
											viewport);
		float projected_depth = projected_parent_pos[2];
		glm::vec3 cursor_pos_3d_0 = glm::unProject(glm::vec3(last_x_, last_y_, projected_depth),
											view_matrix_,
											projection_matrix_,
											viewport);
		glm::vec3 cursor_pos_3d_1 = glm::unProject(glm::vec3(current_x_, current_y_, projected_depth),
									view_matrix_,
									projection_matrix_,
									viewport);
		glm::vec3 offset = cursor_pos_3d_1 - cursor_pos_3d_0;
		mesh_->skeleton.translate_root(offset);
		// std::cout << "translating root: " << offset.x << ", " << offset.y << ", " << offset.z << std::endl;
		setPoseDirty();
	}

	// FIXME: highlight bones that have been moused over
	current_bone_ = -1;
	glm::vec3 mouse_pos = glm::unProject(glm::vec3(current_x_, current_y_, 1.0f),
											view_matrix_,
											projection_matrix_,
											viewport);
	// std::cout << "current position in world coords: (" << mouse_pos.x << ", " << mouse_pos.y << ", " << mouse_pos.z << ")" << std::endl;
	glm::vec3 click_ray_direct = glm::normalize(mouse_pos - eye_);
	glm::vec3 click_ray_end = eye_ + PICK_RAY_LEN * click_ray_direct;
	float min_distance = std::numeric_limits<float>::max();
	for(int i = 0; i < mesh_->getNumberOfBones(); i++) {	// iterate all bones, and find the one with min distance
		int parent_index = mesh_->skeleton.joints[i].parent_index;
		if(parent_index == -1) {
			continue;	// this joint is a root.
		}
		glm::vec3 bone_start_position = mesh_->getJointPosition(i);
		glm::vec3 bone_end_position = mesh_->getJointPosition(parent_index);
		float curr_distance = line_segment_distance(eye_, click_ray_end, bone_start_position, bone_end_position);
		if(curr_distance < kCylinderRadius && curr_distance < min_distance) {
			min_distance = curr_distance;
			current_bone_ = i;
		}
	}

}

void GUI::mouseButtonCallback(int button, int action, int mods)
{
	if (current_x_ <= view_width_) {
		drag_state_ = (action == GLFW_PRESS);
		current_button_ = button;
		return ;
	}
	// FIXME: Key Frame Selection
	if (current_x_ > view_width_&& action == GLFW_PRESS) {
		// std::cout << "mouse over preview! current_y_: " << current_y_ << std::endl;
		current_keyframe_ = ceil((view_height_ + frame_shift - current_y_) / preview_height_) - 1;
		if(current_keyframe_ < 0 || current_keyframe_ >= mesh_->key_frames.size()) {	// invalid keyframe index
			current_keyframe_ = -1;
		}
		// std::cout << "current key frame: " << current_keyframe_ << std::endl;
	}


}

void GUI::mouseScrollCallback(double dx, double dy)
{
	if (current_x_ < view_width_)
		return;
	// FIXME: Mouse Scrolling
	frame_shift += -20 * (int)dy;
}

void GUI::updateMatrices()
{
	// Compute our view, and projection matrices.
	if (fps_mode_)
		center_ = eye_ + camera_distance_ * look_;
	else
		eye_ = center_ - camera_distance_ * look_;

	view_matrix_ = glm::lookAt(eye_, center_, up_);
	light_position_ = glm::vec4(eye_, 1.0f);

	aspect_ = static_cast<float>(view_width_) / view_height_;
	projection_matrix_ =
		glm::perspective((float)(kFov * (M_PI / 180.0f)), aspect_, kNear, kFar);
	model_matrix_ = glm::mat4(1.0f);
}

MatrixPointers GUI::getMatrixPointers() const
{
	MatrixPointers ret;
	ret.projection = &projection_matrix_[0][0];
	ret.model= &model_matrix_[0][0];
	ret.view = &view_matrix_[0][0];
	return ret;
}

bool GUI::setCurrentBone(int i)
{
	if (i < 0 || i >= mesh_->getNumberOfBones())
		return false;
	current_bone_ = i;
	return true;
}

float GUI::getCurrentPlayTime()
{
	double delta_time = toc(timer_);
	time_ += delta_time;
	return time_;
	// return 0.0f;
}


bool GUI::captureWASDUPDOWN(int key, int action)
{
	if (key == GLFW_KEY_W) {
		if (fps_mode_)
			eye_ += zoom_speed_ * look_;
		else
			camera_distance_ -= zoom_speed_;
		return true;
	} else if (key == GLFW_KEY_S) {
		if (fps_mode_)
			eye_ -= zoom_speed_ * look_;
		else
			camera_distance_ += zoom_speed_;
		return true;
	} else if (key == GLFW_KEY_A) {
		if (fps_mode_)
			eye_ -= pan_speed_ * tangent_;
		else
			center_ -= pan_speed_ * tangent_;
		return true;
	} else if (key == GLFW_KEY_D) {
		if (fps_mode_)
			eye_ += pan_speed_ * tangent_;
		else
			center_ += pan_speed_ * tangent_;
		return true;
	} else if (key == GLFW_KEY_DOWN) {
		if (fps_mode_)
			eye_ -= pan_speed_ * up_;
		else
			center_ -= pan_speed_ * up_;
		return true;
	} else if (key == GLFW_KEY_UP) {
		if (fps_mode_)
			eye_ += pan_speed_ * up_;
		else
			center_ += pan_speed_ * up_;
		return true;
	}
	return false;
}


// Delegrate to the actual GUI object.
void GUI::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->keyCallback(key, scancode, action, mods);
}

void GUI::MousePosCallback(GLFWwindow* window, double mouse_x, double mouse_y)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->mousePosCallback(mouse_x, mouse_y);
}

void GUI::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->mouseButtonCallback(button, action, mods);
}

void GUI::MouseScrollCallback(GLFWwindow* window, double dx, double dy)
{
	GUI* gui = (GUI*)glfwGetWindowUserPointer(window);
	gui->mouseScrollCallback(dx, dy);
}
