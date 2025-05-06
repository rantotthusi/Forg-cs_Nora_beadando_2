//#version 330 core
//
//layout (location = 0) in vec3 aPos;
//
//void main(void)
//{
//    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
//}

//#version 430
#version 330 core

out vec4 color;

void main(void)
{
    color = vec4(1.0, 0.0, 0.0, 1.0);
}
