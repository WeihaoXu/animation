#include <GL/glew.h>
#include <dirent.h>

#include "bone_geometry.h"
#include "procedure_geometry.h"
#include "render_pass.h"
#include "config.h"
#include "gui.h"
#include "texture_to_render.h"

#include <memory>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>
#include <debuggl.h>

int scroll_bar_width = 30;
int window_width = 1280 + scroll_bar_width;
int window_height = 720;
int main_view_width = 960;
int main_view_height = 720;
int preview_width = window_width - main_view_width - scroll_bar_width; // 320
int preview_height = preview_width / 4 * 3; // 320 / 4 * 3 = 240
int preview_bar_width = preview_width;
int preview_bar_height = main_view_height;





const std::string window_title = "Animation";



const char* vertex_shader =
#include "shaders/default.vert"
;

const char* blending_shader =
#include "shaders/blending.vert"
;

const char* geometry_shader =
#include "shaders/default.geom"
;

const char* fragment_shader =
#include "shaders/default.frag"
;

const char* floor_fragment_shader =
#include "shaders/floor.frag"
;

const char* bone_vertex_shader =
#include "shaders/bone.vert"
;

const char* bone_fragment_shader =
#include "shaders/bone.frag"
;

// FIXME: Add more shaders here.
const char* cylinder_vertex_shader = 
#include "shaders/cylinder.vert"
;

const char* cylinder_fragment_shader = 
#include "shaders/cylinder.frag"
;

const char* preview_vertex_shader = 
#include "shaders/preview.vert"
;

const char* preview_fragment_shader = 
#include "shaders/preview.frag"
;

const char* scroll_bar_vertex_shader = 
#include "shaders/scroll_bar.vert"
;

const char* scroll_bar_fragment_shader  = 
#include "shaders/scroll_bar.frag"
;

void ErrorCallback(int error, const char* description) {
	std::cerr << "GLFW Error: " << description << "\n";
}

GLFWwindow* init_glefw()
{
	if (!glfwInit())
		exit(EXIT_FAILURE);
	glfwSetErrorCallback(ErrorCallback);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE); // Disable resizing, for simplicity
	glfwWindowHint(GLFW_SAMPLES, 4);
	auto ret = glfwCreateWindow(window_width, window_height, window_title.data(), nullptr, nullptr);
	CHECK_SUCCESS(ret != nullptr);
	glfwMakeContextCurrent(ret);
	glewExperimental = GL_TRUE;
	CHECK_SUCCESS(glewInit() == GLEW_OK);
	glGetError();  // clear GLEW's error for it
	glfwSwapInterval(1);
	const GLubyte* renderer = glGetString(GL_RENDERER);  // get renderer string
	const GLubyte* version = glGetString(GL_VERSION);    // version as a string
	std::cout << "Renderer: " << renderer << "\n";
	std::cout << "OpenGL version supported:" << version << "\n";

	return ret;
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cerr << "Input model file is missing" << std::endl;
		std::cerr << "Usage: " << argv[0] << " <PMD file>" << std::endl;
		return -1;
	}
	GLFWwindow *window = init_glefw();
	GUI gui(window, main_view_width, main_view_height, preview_height, scroll_bar_width);

	std::vector<glm::vec4> floor_vertices;
	std::vector<glm::uvec3> floor_faces;
	create_floor(floor_vertices, floor_faces);

	std::vector<glm::vec4> quad_vertices;
	std::vector<glm::uvec3> quad_faces;
	std::vector<glm::vec2> quad_coords;
	create_quad(quad_vertices, quad_faces, quad_coords);

	std::vector<glm::vec4> scroll_bar_vertices;
	std::vector<glm::uvec3> scroll_bar_faces;
	std::vector<glm::vec2> scroll_bar_coords;
	create_quad(scroll_bar_vertices, scroll_bar_faces, scroll_bar_coords);

	LineMesh cylinder_mesh;
	LineMesh axes_mesh;

	// preview uniforms
	glm::mat4 orthomat(1.0);
	float frame_shift = 0.0;
	int sampler = 0;
	int show_border = 0;
	int show_insert_cursor = 0;

	// output video things
	const char* export_cmd = "ffmpeg -framerate 30 -f rawvideo -pix_fmt rgb24 -s 960x720 -i - -threads 0 -preset fast -y -pix_fmt yuv420p -crf 21 -vf vflip Video.mp4";
	FILE* export_file;
	unsigned char* export_buffer = (unsigned char*) malloc (sizeof(char) * 960 * 720 * 3);
	bool export_file_opened = false;

	// FIXME: we already created meshes for cylinders. Use them to render
	//        the cylinder and axes if required by the assignment.
	create_cylinder_mesh(cylinder_mesh);
	create_axes_mesh(axes_mesh);

	Mesh mesh;
	mesh.loadPmd(argv[1]);
	std::cout << "Loaded object  with  " << mesh.vertices.size()
		<< " vertices and " << mesh.faces.size() << " faces.\n";

	glm::vec4 mesh_center = glm::vec4(0.0f);
	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		mesh_center += mesh.vertices[i];
	}
	mesh_center /= mesh.vertices.size();

	/*
	 * GUI object needs the mesh object for bone manipulation.
	 */
	gui.assignMesh(&mesh);
	mesh.assignGUI(&gui);

	glm::vec4 light_position = glm::vec4(0.0f, 100.0f, 0.0f, 1.0f);
	MatrixPointers mats; // Define MatrixPointers here for lambda to capture
	/*
	 * In the following we are going to define several lambda functions to bind Uniforms.
	 *
	 * Introduction about lambda functions:
	 *      http://en.cppreference.com/w/cpp/language/lambda
	 *      http://www.stroustrup.com/C++11FAQ.html#lambda
	 */
	/*
	 * The following lambda functions are defined to bind uniforms
	 */
	auto matrix_binder = [](int loc, const void* data) {
		glUniformMatrix4fv(loc, 1, GL_FALSE, (const GLfloat*)data);
	};
	auto bone_matrix_binder = [&mesh](int loc, const void* data) {
		auto nelem = mesh.getNumberOfBones();
		glUniformMatrix4fv(loc, nelem, GL_FALSE, (const GLfloat*)data);
	};
	auto vector_binder = [](int loc, const void* data) {
		glUniform4fv(loc, 1, (const GLfloat*)data);
	};
	auto vector3_binder = [](int loc, const void* data) {
		glUniform3fv(loc, 1, (const GLfloat*)data);
	};
	auto float_binder = [](int loc, const void* data) {
		glUniform1fv(loc, 1, (const GLfloat*)data);
	};
	auto int_binder = [](int loc, const void* data) {
		glUniform1iv(loc, 1, (const GLint*)data);
	};
	auto joint_trans_binder = [&mesh](int loc, const void *data) {
		// std::cerr << "Trans Binder: " << mesh.getNumberOfBones() << std::endl;
		// for (const auto& q : mesh.skeleton.cache.trans) {
		// 	std::cerr << "\t" << q << std::endl;
		// }
		glUniform3fv(loc, mesh.getNumberOfBones(), (const GLfloat*)data);
	};
	auto joint_rot_binder = [&mesh](int loc, const void *data) {
		glUniform4fv(loc, mesh.getNumberOfBones(), (const GLfloat*)data);
	};
	auto sampler0_binder = [](int loc, const void* data) {
		CHECK_GL_ERROR(glBindSampler(0, (GLuint)(long)data));
	};
	auto texture0_binder = [](int loc, const void* data) {
		CHECK_GL_ERROR(glUniform1i(loc, 0));
		CHECK_GL_ERROR(glActiveTexture(GL_TEXTURE0 + 0));
		CHECK_GL_ERROR(glBindTexture(GL_TEXTURE_2D, (long)data));
		//std::cerr << " bind texture " << long(data) << std::endl;
	};

	/*
	 * The lambda functions below are used to retrieve data
	 */
	auto std_model_data = [&mats]() -> const void* {
		return mats.model;
	}; // This returns point to model matrix
	glm::mat4 floor_model_matrix = glm::mat4(1.0f);
	auto floor_model_data = [&floor_model_matrix]() -> const void* {
		return &floor_model_matrix[0][0];
	}; // This return model matrix for the floor.
	auto std_view_data = [&mats]() -> const void* {
		return mats.view;
	};
	auto std_camera_data  = [&gui]() -> const void* {
		return &gui.getCamera()[0];
	};
	auto std_proj_data = [&mats]() -> const void* {
		return mats.projection;
	};
	auto std_light_data = [&light_position]() -> const void* {
		return &light_position[0];
	};
	auto alpha_data  = [&gui]() -> const void* {
		static const float transparet = 0.5; // Alpha constant goes here
		static const float non_transparet = 1.0;
		if (gui.isTransparent())
			return &transparet;
		else
			return &non_transparet;
	};
	auto joint_trans_data = [&mesh]() -> const void* {
		auto ret = mesh.getCurrentQ()->transData();
		return ret;
	};
	auto joint_rot_data = [&mesh]() -> const void* {
		return mesh.getCurrentQ()->rotData();
	};
	// FIXME: add more lambdas for data_source if you want to use RenderPass.
	//        Otherwise, do whatever you like here
	glm::mat4 bone_transform_matrix;
	auto bone_transform_data = [&mesh, &bone_transform_matrix, &gui]() -> const void* {
		int current_bone = gui.getCurrentBone();
		if(current_bone != -1) {
			bone_transform_matrix = mesh.skeleton.getBoneTransform(current_bone);	
		}
		else {
			bone_transform_matrix = glm::mat4(1.0f);	// won't be used anyway
		}
		// printMat4(bone_transform_matrix);
		return &bone_transform_matrix[0][0];
		
	};
	float radius = kCylinderRadius;
	auto cylinder_radius_data = [&radius]() -> const void* {
		return &radius;
	};

	// preview uniforms
	auto texture_data = [&sampler]() -> const void* {
		return (const void*)(intptr_t)sampler;
	};	
	auto sampler_data = [&sampler]() -> const void* {
		// std::cout << "sampler: " << (intptr_t)sampler << std::endl;
		return (const void*)(intptr_t)sampler;
	};

	auto show_border_data = [&show_border]() -> const void* {
		return &show_border;
	};

	auto orthomat_data = [&orthomat]() -> const void* {
		return &orthomat[0][0];
	};
	auto frame_shift_data = [&frame_shift, &gui]() -> const void* {
		frame_shift = gui.get_frame_shift();
		return &frame_shift;
	};

	auto show_insert_cursor_data = [&show_insert_cursor]() -> const void* {
		return &show_insert_cursor;
	};

	int total_preview_num = 0;
	auto total_preview_num_data =  [&total_preview_num, &mesh]() -> const void* {
		total_preview_num = mesh.textures.size();
		return &total_preview_num;
	};

	


	ShaderUniform std_model = { "model", matrix_binder, std_model_data };
	ShaderUniform floor_model = { "model", matrix_binder, floor_model_data };
	ShaderUniform std_view = { "view", matrix_binder, std_view_data };
	ShaderUniform std_camera = { "camera_position", vector3_binder, std_camera_data };
	ShaderUniform std_proj = { "projection", matrix_binder, std_proj_data };
	ShaderUniform std_light = { "light_position", vector_binder, std_light_data };
	ShaderUniform object_alpha = { "alpha", float_binder, alpha_data };
	ShaderUniform joint_trans = { "joint_trans", joint_trans_binder, joint_trans_data };
	ShaderUniform joint_rot = { "joint_rot", joint_rot_binder, joint_rot_data };
	// FIXME: define more ShaderUniforms for RenderPass if you want to use it.
	//        Otherwise, do whatever you like here
	ShaderUniform bone_transform = { "bone_transform", matrix_binder, bone_transform_data };
	ShaderUniform cylinder_radius = { "cylinder_radius", float_binder, cylinder_radius_data};

	// preview uniforms
	ShaderUniform sampler_uniform = { "sampler", texture0_binder, sampler_data };
	ShaderUniform show_border_uniform = { "show_border", int_binder, show_border_data };
	ShaderUniform orthomat_uniform = { "orthomat", matrix_binder, orthomat_data };
	ShaderUniform frame_shift_uniform = { "frame_shift", float_binder, frame_shift_data };
	ShaderUniform show_insert_cursor_uniform = {"show_insert_cursor", int_binder, show_insert_cursor_data};

	ShaderUniform total_preview_num_uniform = {"total_preview_num", int_binder, total_preview_num_data};


	// Floor render pass
	RenderDataInput floor_pass_input;
	floor_pass_input.assign(0, "vertex_position", floor_vertices.data(), floor_vertices.size(), 4, GL_FLOAT);
	floor_pass_input.assignIndex(floor_faces.data(), floor_faces.size(), 3);
	RenderPass floor_pass(-1,
			floor_pass_input,
			{ vertex_shader, geometry_shader, floor_fragment_shader},
			{ floor_model, std_view, std_proj, std_light },
			{ "fragment_color" }
			);

	// PMD Model render pass
	// FIXME: initialize the input data at Mesh::loadPmd
	std::vector<glm::vec2>& uv_coordinates = mesh.uv_coordinates;
	RenderDataInput object_pass_input;
	object_pass_input.assign(0, "jid0", mesh.joint0.data(), mesh.joint0.size(), 1, GL_INT);
	object_pass_input.assign(1, "jid1", mesh.joint1.data(), mesh.joint1.size(), 1, GL_INT);
	object_pass_input.assign(2, "w0", mesh.weight_for_joint0.data(), mesh.weight_for_joint0.size(), 1, GL_FLOAT);
	object_pass_input.assign(3, "vector_from_joint0", mesh.vector_from_joint0.data(), mesh.vector_from_joint0.size(), 3, GL_FLOAT);
	object_pass_input.assign(4, "vector_from_joint1", mesh.vector_from_joint1.data(), mesh.vector_from_joint1.size(), 3, GL_FLOAT);
	object_pass_input.assign(5, "normal", mesh.vertex_normals.data(), mesh.vertex_normals.size(), 4, GL_FLOAT);
	object_pass_input.assign(6, "uv", uv_coordinates.data(), uv_coordinates.size(), 2, GL_FLOAT);
	// TIPS: You won't need vertex position in your solution.
	//       This only serves the stub shader.
	object_pass_input.assign(7, "vert", mesh.vertices.data(), mesh.vertices.size(), 4, GL_FLOAT);
	object_pass_input.assignIndex(mesh.faces.data(), mesh.faces.size(), 3);
	object_pass_input.useMaterials(mesh.materials);
	RenderPass object_pass(-1,
			object_pass_input,
			{
			  blending_shader,
			  geometry_shader,
			  fragment_shader
			},
			{ std_model, std_view, std_proj,
			  std_light,
			  std_camera, object_alpha,
			  joint_trans, joint_rot
			},
			{ "fragment_color" }
			);

	// Setup the render pass for drawing bones
	// FIXME: You won't see the bones until Skeleton::joints were properly
	//        initialized
	std::vector<int> bone_vertex_id;
	std::vector<glm::uvec2> bone_indices;
	for (int i = 0; i < (int)mesh.skeleton.joints.size(); i++) {
		bone_vertex_id.emplace_back(i);
	}
	for (const auto& joint: mesh.skeleton.joints) {
		if (joint.parent_index < 0)
			continue;
		bone_indices.emplace_back(joint.joint_index, joint.parent_index);
	}
	RenderDataInput bone_pass_input;
	bone_pass_input.assign(0, "jid", bone_vertex_id.data(), bone_vertex_id.size(), 1, GL_UNSIGNED_INT);
	bone_pass_input.assignIndex(bone_indices.data(), bone_indices.size(), 2);
	RenderPass bone_pass(-1, bone_pass_input,
			{ bone_vertex_shader, nullptr, bone_fragment_shader},
			{ std_model, std_view, std_proj, joint_trans },
			{ "fragment_color" }
			);

	// FIXME: Create the RenderPass objects for bones here.
	//        Otherwise do whatever you like.
	/* my cylinder bones */
	RenderDataInput cylinder_pass_input;
	cylinder_pass_input.assign(0, "vertex_position", cylinder_mesh.vertices.data(), cylinder_mesh.vertices.size(), 4, GL_FLOAT);
	cylinder_pass_input.assignIndex(cylinder_mesh.indices.data(), cylinder_mesh.indices.size(), 2);
	RenderPass cylinder_pass(-1, cylinder_pass_input,
			{ cylinder_vertex_shader, nullptr, cylinder_fragment_shader },
			{ std_model, std_view, std_proj, bone_transform, cylinder_radius },
			{ "fragment_color" }
			);

	RenderDataInput preview_pass_input;
	preview_pass_input.assign(0, "vertex_position", quad_vertices.data(), quad_vertices.size(), 4, GL_FLOAT);
	preview_pass_input.assign(1, "tex_coord_in", quad_coords.data(), quad_coords.size(), 2, GL_FLOAT);
	preview_pass_input.assignIndex(quad_faces.data(), quad_faces.size(), 3);
	RenderPass preview_pass(-1, preview_pass_input,
			{preview_vertex_shader, nullptr, preview_fragment_shader},
			{orthomat_uniform, frame_shift_uniform, sampler_uniform, show_border_uniform, show_insert_cursor_uniform},
			{"fragment_color"}
			);

	// scroll_bar_vertices, scroll_bar_faces, scroll_bar_coords

	RenderDataInput scroll_bar_pass_input;
	scroll_bar_pass_input.assign(0, "vertex_position", scroll_bar_vertices.data(), scroll_bar_vertices.size(), 4, GL_FLOAT);
	scroll_bar_pass_input.assign(1, "tex_coord_in", scroll_bar_coords.data(), scroll_bar_coords.size(), 2, GL_FLOAT);
	scroll_bar_pass_input.assignIndex(scroll_bar_faces.data(), scroll_bar_faces.size(), 3);
	RenderPass scroll_bar_pass(-1, scroll_bar_pass_input,
			{scroll_bar_vertex_shader, nullptr, scroll_bar_fragment_shader},
			{frame_shift_uniform, total_preview_num_uniform},
			{"fragment_color"}
			);

	float aspect = 0.0f;
	std::cout << "center = " << mesh.getCenter() << "\n";

	bool draw_floor = true;
	bool draw_skeleton = true;
	bool draw_object = true;
	bool draw_cylinder = true;
	bool draw_scroll_bar = true;

	if (argc >= 3) {
		mesh.loadAnimationFrom(argv[2]);
		// load external animation files
		mesh.to_load_animation = true;	
	}

	while (!glfwWindowShouldClose(window)) {
		// Setup some basic window stuff.
		glfwGetFramebufferSize(window, &window_width, &window_height);
		glViewport(0, 0, main_view_width, main_view_height);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LESS);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glCullFace(GL_BACK);

		gui.updateMatrices();
		mats = gui.getMatrixPointers();

		if (gui.isPlaying()) {
			std::stringstream title;
			float cur_time = gui.getCurrentPlayTime();
			// float cur_time = gui.time_;
			// gui.time_ += 0.01f;

			title << window_title << " Playing: "
			      << std::setprecision(2)
			      << std::setfill('0') << std::setw(6)
			      << cur_time << " sec";
			glfwSetWindowTitle(window, title.str().data());
			mesh.updateAnimation(cur_time);
		} else if (gui.isPoseDirty()) {
			mesh.updateAnimation();
			gui.clearPose();
		}

		
		// render keyframes that loaded from json file into preview textures 
		if(mesh.to_load_animation) {
			for(int i = 0; i < mesh.key_frames.size(); i++) {
				mesh.skeleton.transform_skeleton_by_frame(mesh.key_frames[i]);
				gui.set_camera_rel_orientation(mesh.key_frames[i].camera_rel_orientation);
				mesh.updateAnimation();
				TextureToRender* texture = new TextureToRender();
				texture->create(main_view_width, main_view_height);
				texture->bind();

				floor_pass.setup();
				CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
				                              floor_faces.size() * 3,
				                              GL_UNSIGNED_INT, 0));
				object_pass.setup();
				int mid = 0;
				while (object_pass.renderWithMaterial(mid))
					mid++;
	
				mesh.textures.push_back(texture);
				texture->unbind();		
			}
			mesh.to_load_animation = false;
			mesh.skeleton.set_rest_pose();
			gui.set_camera_rel_orientation(glm::fquat());
			mesh.updateAnimation();
		}

		if(mesh.to_overwrite_keyframe) {
			int key_frame_idx = mesh.key_frame_to_overwrite;
			TextureToRender* new_texture = new TextureToRender();

			new_texture->create(main_view_width, main_view_height);
			new_texture->bind();

			floor_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
			                              floor_faces.size() * 3,
			                              GL_UNSIGNED_INT, 0));
			object_pass.setup();
			int mid = 0;
			while (object_pass.renderWithMaterial(mid))
				mid++;

			// mesh.textures.push_back(texture);	
			TextureToRender* old_texture = mesh.textures[key_frame_idx];
			delete old_texture;
			mesh.textures[key_frame_idx] = new_texture;
			new_texture->unbind();
			mesh.to_overwrite_keyframe = false;
		}



		// draw scroll bar
		if(draw_scroll_bar) {
			glViewport(window_width - scroll_bar_width, 0, scroll_bar_width, window_height);
			scroll_bar_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
			                              scroll_bar_faces.size() * 3,
			                              GL_UNSIGNED_INT, 0));
			glViewport(0, 0, main_view_width, main_view_height);
		}
		

		int current_bone = gui.getCurrentBone();
		// Draw bones first.
		if (draw_skeleton && gui.isTransparent()) {
			bone_pass.setup();
			// Draw our lines.
			// FIXME: you need setup skeleton.joints properly in
			//        order to see the bones.
			CHECK_GL_ERROR(glDrawElements(GL_LINES,
			                              bone_indices.size() * 2,
			                              GL_UNSIGNED_INT, 0));
		}
		draw_cylinder = (current_bone != -1 && gui.isTransparent());
		if(draw_cylinder) {
			cylinder_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_LINES,
		                              cylinder_mesh.indices.size() * 2,
		                              GL_UNSIGNED_INT, 0));
		}
		
		// Then draw floor.
		if (draw_floor) {
			floor_pass.setup();
			// Draw our triangles.
			CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
			                              floor_faces.size() * 3,
			                              GL_UNSIGNED_INT, 0));
		}

		// Draw the model
		if (draw_object) {
			object_pass.setup();
			int mid = 0;
			while (object_pass.renderWithMaterial(mid))
				mid++;
#if 0
			// For debugging also
			if (mid == 0) // Fallback
				CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES, mesh.faces.size() * 3, GL_UNSIGNED_INT, 0));
#endif
		}

		

		// FIXME: update the preview textures here
		if(mesh.to_save_preview) {
			TextureToRender* texture = new TextureToRender();
			texture->create(main_view_width, main_view_height);
			texture->bind();

			floor_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
			                              floor_faces.size() * 3,
			                              GL_UNSIGNED_INT, 0));
			object_pass.setup();
			int mid = 0;
			while (object_pass.renderWithMaterial(mid))
				mid++;
				
			mesh.textures.push_back(texture);
			texture->unbind();
			mesh.to_save_preview = false;
		}

		// std::cout << "texture numbers: " << mesh.textures.size() << std::endl;
		
		// FIXME: Draw previews here, note you need to call glViewport
		for(int i = 0; i < mesh.textures.size(); i++) {
			glViewport(main_view_width, main_view_height - (i + 1) * preview_height + gui.get_frame_shift(), preview_width, preview_height);
			// std::cout << "shift is " << gui.get_frame_shift() << std::endl;
			sampler = mesh.textures[i]->getTexture();

			bool insert_enabled = gui.insert_keyframe_enabled();
			if(i == gui.get_current_keyframe()) {
				if(!insert_enabled) {
					show_border = 1;
					show_insert_cursor = 0;
				} 
				else {
					show_border = 0;
					show_insert_cursor = 1;
				}
			}
			else {
				show_border = 0;
				show_insert_cursor = 0;
			}
			// std::cout << "texture " << i << ", show cursor? " << show_insert_cursor << ", show border? " << show_border << std::endl;
			preview_pass.setup();
			CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES,
			                              quad_faces.size() * 3,
			                              GL_UNSIGNED_INT, 0));
		}	
		glViewport(0, 0, main_view_width, main_view_height);

		
		// Poll and swap.
		glfwPollEvents();
		glfwSwapBuffers(window);
		if (gui.to_export_video_) {
			if(!export_file_opened) {
				export_file = popen(export_cmd, "w");
				export_file_opened = true;
			}
			// std::cout << "export a frame" << std::endl;
			// glPixelStorei(GL_UNPACK_ALIGNMENT,1);
			glReadPixels(0, 0, 960, 720, GL_RGB, GL_UNSIGNED_BYTE, export_buffer);

			fwrite(export_buffer, 960 *720*3 , 1, export_file);
			
			if(gui.getCurrentPlayTime() > mesh.key_frames.size() * 1.0 - 1.0) {
				
				pclose(export_file);
				export_file_opened = false;
				gui.to_export_video_ = false;
				std::cout << "export video done" << std::endl;
			}
		}

	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
