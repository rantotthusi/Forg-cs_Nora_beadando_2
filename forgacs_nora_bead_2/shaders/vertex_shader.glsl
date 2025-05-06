//#version 330 core
//
//uniform bool isLine;
//uniform vec3 center;
//out vec4 color;
//
//void main()
//{
//    if(isLine){
//        color = vec4(0.1, 0.1, 1, 0);
//    } else{
//        float dist = sqrt(pow(center.x + 300.0 - gl_FragCoord.x, 2.0) + pow(center.y + 300.0 - gl_FragCoord.y, 2.0));
//        if(abs(center.x - 200.0) > 50.0) {
//            color = vec4(dist / 50.0, 1.0 - dist / 50.0, 0.0, 1.0);
//        } else {
//            color = vec4(1.0 - dist / 50.0, dist / 50.0, 0.0, 1.0);
//        }
//    }
//}


//#version 430
#version 330 core

layout (location = 0) in vec3 aPos;

void main(void)
{
    gl_Position = vec4(aPos, 1.0);
}
