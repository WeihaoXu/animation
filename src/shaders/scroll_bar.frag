R"zzz(#version 330 core
out vec4 fragment_color;
in vec2 tex_coord;

uniform float frame_shift;
uniform int total_preview_num; 

void main() {
	if(total_preview_num <= 3) {
		fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
	}
	else {
		float scroll_v_coord = (1.0 * frame_shift) / (total_preview_num * 240);
		float cube_v_height = 3.0 * 1.0 / total_preview_num;
		float dy = 1.0 - tex_coord.y;
		if(dy > scroll_v_coord && dy < scroll_v_coord + cube_v_height) {
			fragment_color = vec4(0.0, 1.0, 1.0, 1.0);
		} else {
			fragment_color = vec4(0.0, 0.0, 0.0, 1.0);
		}

	}
	fragment_color.a = 0.5;

	

}
)zzz"
