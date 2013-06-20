varying vec4 color;

varying vec3 N;
varying vec3 v;
varying vec4 diffuse;
varying vec4 spec;

void main()
{
vec4 diffuse;
vec4 spec;
vec4 ambient;

   v = vec3(gl_ModelViewMatrix * gl_Vertex);
   N = normalize(gl_NormalMatrix * gl_Normal);
   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;  

   vec3 L = normalize(gl_LightSource[0].position.xyz - v);
   vec3 E = normalize(-v);
   vec3 R = normalize(reflect(-L,N)); 

   ambient = vec4(51.0 / 512.0, 0.5, 0.0, 1.0);
   diffuse = clamp( vec4(1.0, 1.0, 1.0, 1.0) * max(dot(N,L), 0.0)  , 0.0, 1.0 ) ;
   spec = clamp ( vec4(1.0, 1.0, 1.0, 1.0) * pow(max(dot(R,E),0.0),0.3*1.0) , 0.0, 1.0 );

   color = ambient + diffuse + spec;
}
